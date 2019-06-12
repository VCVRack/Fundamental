#include "plugin.hpp"
#include "samplerate.h"


#define HISTORY_SIZE (1<<21)

struct Delay : Module {
	enum ParamIds {
		TIME_PARAM,
		FEEDBACK_PARAM,
		COLOR_PARAM,
		MIX_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		TIME_INPUT,
		FEEDBACK_INPUT,
		COLOR_INPUT,
		MIX_INPUT,
		IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};

	dsp::DoubleRingBuffer<float, HISTORY_SIZE> historyBuffer;
	dsp::DoubleRingBuffer<float, 16> outBuffer;
	SRC_STATE *src;
	float lastWet = 0.f;
	dsp::RCFilter lowpassFilter;
	dsp::RCFilter highpassFilter;

	Delay() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
		configParam(TIME_PARAM, 0.f, 1.f, 0.5f, "Time", " s", 10.f / 1e-3, 1e-3);
		configParam(FEEDBACK_PARAM, 0.f, 1.f, 0.5f, "Feedback", "%", 0, 100);
		configParam(COLOR_PARAM, 0.f, 1.f, 0.5f, "Color", "%", 0, 100);
		configParam(MIX_PARAM, 0.f, 1.f, 0.5f, "Mix", "%", 0, 100);

		src = src_new(SRC_SINC_FASTEST, 1, NULL);
		assert(src);
	}

	~Delay() {
		src_delete(src);
	}

	void process(const ProcessArgs &args) override {
		// Get input to delay block
		float in = inputs[IN_INPUT].getVoltage();
		float feedback = params[FEEDBACK_PARAM].getValue() + inputs[FEEDBACK_INPUT].getVoltage() / 10.f;
		feedback = clamp(feedback, 0.f, 1.f);
		float dry = in + lastWet * feedback;

		// Compute delay time in seconds
		float delay = params[TIME_PARAM].getValue() + inputs[TIME_INPUT].getVoltage() / 10.f;
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
		float color = params[COLOR_PARAM].getValue() + inputs[COLOR_INPUT].getVoltage() / 10.f;
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

		float mix = params[MIX_PARAM].getValue() + inputs[MIX_INPUT].getVoltage() / 10.f;
		mix = clamp(mix, 0.f, 1.f);
		float out = crossfade(in, wet, mix);
		outputs[OUT_OUTPUT].setVoltage(out);
	}
};


struct DelayWidget : ModuleWidget {
	DelayWidget(Delay *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Delay.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParam<RoundLargeBlackKnob>(Vec(67, 57), module, Delay::TIME_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(67, 123), module, Delay::FEEDBACK_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(67, 190), module, Delay::COLOR_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(67, 257), module, Delay::MIX_PARAM));

		addInput(createInput<PJ301MPort>(Vec(14, 63), module, Delay::TIME_INPUT));
		addInput(createInput<PJ301MPort>(Vec(14, 129), module, Delay::FEEDBACK_INPUT));
		addInput(createInput<PJ301MPort>(Vec(14, 196), module, Delay::COLOR_INPUT));
		addInput(createInput<PJ301MPort>(Vec(14, 263), module, Delay::MIX_INPUT));
		addInput(createInput<PJ301MPort>(Vec(14, 320), module, Delay::IN_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(73, 320), module, Delay::OUT_OUTPUT));
	}
};


Model *modelDelay = createModel<Delay, DelayWidget>("Delay");
