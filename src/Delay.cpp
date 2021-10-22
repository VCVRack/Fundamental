#include "plugin.hpp"
#include <samplerate.h>


#define HISTORY_SIZE (1<<21)

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
		PERIOD_LIGHT,
		NUM_LIGHTS
	};

	dsp::DoubleRingBuffer<float, HISTORY_SIZE> historyBuffer;
	dsp::DoubleRingBuffer<float, 16> outBuffer;
	SRC_STATE* src;
	float lastWet = 0.f;
	dsp::RCFilter lowpassFilter;
	dsp::RCFilter highpassFilter;

	Delay() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TIME_PARAM, 0.f, 1.f, 0.5f, "Time", " s", 10.f / 1e-3, 1e-3);
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
		configInput(FEEDBACK_INPUT, "Feedback");
		configInput(TONE_INPUT, "Tone");
		configInput(MIX_INPUT, "Mix");
		configInput(IN_INPUT, "Audio");
		configInput(CLOCK_INPUT, "Clock");

		configOutput(MIX_OUTPUT, "Mix");
		configOutput(WET_OUTPUT, "Wet");

		src = src_new(SRC_SINC_FASTEST, 1, NULL);
		assert(src);
	}

	~Delay() {
		src_delete(src);
	}

	void process(const ProcessArgs& args) override {
		// Get input to delay block
		float in = inputs[IN_INPUT].getVoltageSum();
		float feedback = params[FEEDBACK_PARAM].getValue() + inputs[FEEDBACK_INPUT].getVoltage() / 10.f * params[FEEDBACK_CV_PARAM].getValue();
		feedback = clamp(feedback, 0.f, 1.f);
		float dry = in + lastWet * feedback;

		// Compute delay time in seconds
		float delay = params[TIME_PARAM].getValue() + inputs[TIME_INPUT].getVoltage() / 10.f * params[TIME_CV_PARAM].getValue();
		delay = clamp(delay, 0.f, 1.f);
		delay = 1e-3 * std::pow(10.f / 1e-3, delay);
		// Number of delay samples
		float index = std::round(delay * args.sampleRate);

		// Push dry sample into history buffer
		if (!historyBuffer.full()) {
			historyBuffer.push(dry);
		}

		// How many samples do we need consume to catch up?
		float consume = index - historyBuffer.size();

		if (outBuffer.empty()) {
			double ratio = 1.f;
			if (std::fabs(consume) >= 16.f) {
				// Here's where the delay magic is. Smooth the ratio depending on how divergent we are from the correct delay time.
				ratio = std::pow(10.f, clamp(consume / 10000.f, -1.f, 1.f));
			}

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

		lastWet = wet;

		float mix = params[MIX_PARAM].getValue() + inputs[MIX_INPUT].getVoltage() / 10.f * params[MIX_CV_PARAM].getValue();
		mix = clamp(mix, 0.f, 1.f);
		float out = crossfade(in, wet, mix);
		outputs[MIX_OUTPUT].setVoltage(out);
	}

	void fromJson(json_t* rootJ) override {
		// These attenuators didn't exist in version <2.0, so set to 1 for default compatibility.
		params[TIME_CV_PARAM].setValue(1.f);
		params[FEEDBACK_CV_PARAM].setValue(1.f);
		params[TONE_CV_PARAM].setValue(1.f);
		params[MIX_CV_PARAM].setValue(1.f);

		Module::fromJson(rootJ);
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

		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(22.738, 16.428)), module, Delay::PERIOD_LIGHT));
	}
};


Model* modelDelay = createModel<Delay, DelayWidget>("Delay");
