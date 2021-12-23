#include "plugin.hpp"
#include <samplerate.h>


struct Delay : Module {
	enum ParamId {
		TIME_PARAM,
		FEEDBACK_PARAM,
		TONE_PARAM,
		MIX_PARAM,
		// new in 2.0
		TIME_CV_PARAM,
		FEEDBACK_CV_PARAM,
		TONE_CV_PARAM,
		MIX_CV_PARAM,
		NUM_PARAMS
	};
	enum InputId {
		TIME_INPUT,
		FEEDBACK_INPUT,
		TONE_INPUT,
		MIX_INPUT,
		IN_INPUT,
		// new in 2.0
		CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputId {
		MIX_OUTPUT,
		// new in 2.0
		WET_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightId {
		CLOCK_LIGHT,
		NUM_LIGHTS
	};

	constexpr static size_t HISTORY_SIZE = 1 << 21;
	dsp::DoubleRingBuffer<float, HISTORY_SIZE> historyBuffer;
	dsp::DoubleRingBuffer<float, 16> outBuffer;
	SRC_STATE* src;
	float lastWet = 0.f;
	dsp::RCFilter lowpassFilter;
	dsp::RCFilter highpassFilter;
	float clockFreq = 1.f;
	dsp::Timer clockTimer;
	dsp::SchmittTrigger clockTrigger;
	float clockPhase = 0.f;

	Delay() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		// This was made before the pitch voltage standard existed, so it uses TIME_PARAM = 0 as 0.001s and TIME_PARAM = 1 as 10s with a formula of:
		// time = 0.001 * 10000^TIME_PARAM
		// or
		// TIME_PARAM = log10(time * 1000) / 4
		const float timeMin = log10(0.001f * 1000) / 4;
		const float timeMax = log10(10.f * 1000) / 4;
		const float timeDefault = log10(0.5f * 1000) / 4;
		configParam(TIME_PARAM, timeMin, timeMax, timeDefault, "Time", " s", 10.f / 1e-3, 1e-3);
		configParam(FEEDBACK_PARAM, 0.f, 1.f, 0.5f, "Feedback", "%", 0, 100);
		configParam(TONE_PARAM, 0.f, 1.f, 0.5f, "Tone", "%", 0, 200, -100);
		configParam(MIX_PARAM, 0.f, 1.f, 0.5f, "Mix", "%", 0, 100);
		configParam(TIME_CV_PARAM, -1.f, 1.f, 0.f, "Time CV", "%", 0, 100);
		getParamQuantity(TIME_CV_PARAM)->randomizeEnabled = false;
		configParam(FEEDBACK_CV_PARAM, -1.f, 1.f, 0.f, "Feedback CV", "%", 0, 100);
		getParamQuantity(FEEDBACK_CV_PARAM)->randomizeEnabled = false;
		configParam(TONE_CV_PARAM, -1.f, 1.f, 0.f, "Tone CV", "%", 0, 100);
		getParamQuantity(TONE_CV_PARAM)->randomizeEnabled = false;
		configParam(MIX_CV_PARAM, -1.f, 1.f, 0.f, "Mix CV", "%", 0, 100);
		getParamQuantity(MIX_CV_PARAM)->randomizeEnabled = false;

		configInput(TIME_INPUT, "Time");
		getInputInfo(TIME_INPUT)->description = "1V/octave when Time CV is 100%";
		configInput(FEEDBACK_INPUT, "Feedback");
		configInput(TONE_INPUT, "Tone");
		configInput(MIX_INPUT, "Mix");
		configInput(IN_INPUT, "Audio");
		configInput(CLOCK_INPUT, "Clock");

		configOutput(MIX_OUTPUT, "Mix");
		configOutput(WET_OUTPUT, "Wet");

		configBypass(IN_INPUT, WET_OUTPUT);
		configBypass(IN_INPUT, MIX_OUTPUT);

		src = src_new(SRC_SINC_FASTEST, 1, NULL);
		assert(src);
	}

	~Delay() {
		src_delete(src);
	}

	void process(const ProcessArgs& args) override {
		// Clock
		if (inputs[CLOCK_INPUT].isConnected()) {
			clockTimer.process(args.sampleTime);

			if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f)) {
				float clockFreq = 1.f / clockTimer.getTime();
				clockTimer.reset();
				if (0.001f <= clockFreq && clockFreq <= 1000.f) {
					this->clockFreq = clockFreq;
				}
			}
		}
		else {
			// Default frequency when clock is unpatched
			clockFreq = 2.f;
		}

		// Get input to delay block
		float in = inputs[IN_INPUT].getVoltageSum();
		float feedback = params[FEEDBACK_PARAM].getValue() + inputs[FEEDBACK_INPUT].getVoltage() / 10.f * params[FEEDBACK_CV_PARAM].getValue();
		feedback = clamp(feedback, 0.f, 1.f);
		float dry = in + lastWet * feedback;

		// Compute freq
		// Scale time knob to 1V/oct pitch based on formula explained in constructor, for backwards compatibility
		float pitch = std::log2(1000.f) - std::log2(10000.f) * params[TIME_PARAM].getValue();
		pitch += inputs[TIME_INPUT].getVoltage() * params[TIME_CV_PARAM].getValue();
		float freq = clockFreq / 2.f * std::pow(2.f, pitch);
		// Number of desired delay samples
		float index = args.sampleRate / freq;
		// In order to delay accurate samples, subtract by the historyBuffer size, and an experimentally tweaked amount.
		index -= 16 + 4.f;
		index = clamp(index, 2.f, float(HISTORY_SIZE - 1));
		// DEBUG("freq %f index %f", freq, index);


		// Push dry sample into history buffer
		if (!historyBuffer.full()) {
			historyBuffer.push(dry);
		}

		if (outBuffer.empty()) {
			// How many samples do we need consume to catch up?
			float consume = index - historyBuffer.size();
			double ratio = std::pow(4.f, clamp(consume / 10000.f, -1.f, 1.f));
			// DEBUG("index %f historyBuffer %lu consume %f ratio %lf", index, historyBuffer.size(), consume, ratio);

			// Convert samples from the historyBuffer to catch up or slow down so `index` and `historyBuffer.size()` eventually match approximately
			SRC_DATA srcData;
			srcData.data_in = (const float*) historyBuffer.startData();
			srcData.data_out = (float*) outBuffer.endData();
			srcData.input_frames = std::min((int) historyBuffer.size(), 16);
			srcData.output_frames = outBuffer.capacity();
			srcData.end_of_input = false;
			srcData.src_ratio = ratio;
			src_process(src, &srcData);
			historyBuffer.startIncr(srcData.input_frames_used);
			outBuffer.endIncr(srcData.output_frames_gen);
			// DEBUG("used %ld gen %ld", srcData.input_frames_used, srcData.output_frames_gen);
		}

		float wet = 0.f;
		if (!outBuffer.empty()) {
			wet = outBuffer.shift();
		}

		// Apply color to delay wet output
		float color = params[TONE_PARAM].getValue() + inputs[TONE_INPUT].getVoltage() / 10.f * params[TONE_CV_PARAM].getValue();
		color = clamp(color, 0.f, 1.f);
		float colorFreq = std::pow(100.f, 2.f * color - 1.f);

		float lowpassFreq = clamp(20000.f * colorFreq, 20.f, 20000.f);
		lowpassFilter.setCutoffFreq(lowpassFreq / args.sampleRate);
		lowpassFilter.process(wet);
		wet = lowpassFilter.lowpass();

		float highpassFreq = clamp(20.f * colorFreq, 20.f, 20000.f);
		highpassFilter.setCutoff(highpassFreq / args.sampleRate);
		highpassFilter.process(wet);
		wet = highpassFilter.highpass();

		// Set wet output
		outputs[WET_OUTPUT].setVoltage(wet);
		lastWet = wet;

		// Set mix output
		float mix = params[MIX_PARAM].getValue() + inputs[MIX_INPUT].getVoltage() / 10.f * params[MIX_CV_PARAM].getValue();
		mix = clamp(mix, 0.f, 1.f);
		float out = crossfade(in, wet, mix);
		outputs[MIX_OUTPUT].setVoltage(out);

		// Clock light
		clockPhase += freq * args.sampleTime;
		if (clockPhase >= 1.f) {
			clockPhase -= 1.f;
			lights[CLOCK_LIGHT].setBrightness(1.f);
		}
		else {
			lights[CLOCK_LIGHT].setBrightnessSmooth(0.f, args.sampleTime);
		}
	}

	void paramsFromJson(json_t* rootJ) override {
		// These attenuators didn't exist in version <2.0, so set to 1 in case they are not overwritten.
		params[FEEDBACK_CV_PARAM].setValue(1.f);
		params[TONE_CV_PARAM].setValue(1.f);
		params[MIX_CV_PARAM].setValue(1.f);
		// The time input scaling has changed, so don't set to 1.
		// params[TIME_CV_PARAM].setValue(1.f);

		Module::paramsFromJson(rootJ);
	}
};


struct DelayWidget : ModuleWidget {
	DelayWidget(Delay* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Delay.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(12.579, 26.747)), module, Delay::TIME_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(32.899, 26.747)), module, Delay::FEEDBACK_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(12.579, 56.388)), module, Delay::TONE_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(32.899, 56.388)), module, Delay::MIX_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.605, 80.561)), module, Delay::TIME_CV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(17.442, 80.561)), module, Delay::FEEDBACK_CV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(28.278, 80.561)), module, Delay::TONE_CV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(39.115, 80.561)), module, Delay::MIX_CV_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.605, 96.859)), module, Delay::TIME_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.442, 96.859)), module, Delay::FEEDBACK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(28.278, 96.819)), module, Delay::TONE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(39.115, 96.819)), module, Delay::MIX_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.605, 113.115)), module, Delay::IN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.442, 113.115)), module, Delay::CLOCK_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(28.278, 113.115)), module, Delay::WET_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.115, 113.115)), module, Delay::MIX_OUTPUT));

		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(22.738, 16.428)), module, Delay::CLOCK_LIGHT));
	}
};


Model* modelDelay = createModel<Delay, DelayWidget>("Delay");
