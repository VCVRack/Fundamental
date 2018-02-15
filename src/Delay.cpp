#include "Fundamental.hpp"
#include "dsp/samplerate.hpp"
#include "dsp/ringbuffer.hpp"
#include "dsp/filter.hpp"


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

	DoubleRingBuffer<float, HISTORY_SIZE> historyBuffer;
	DoubleRingBuffer<float, 16> outBuffer;
	SampleRateConverter<1> src;
	float lastWet = 0.0f;
	RCFilter lowpassFilter;
	RCFilter highpassFilter;

	Delay() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}

	void step() override;
};


void Delay::step() {
	// Get input to delay block
	float in = inputs[IN_INPUT].value;
	float feedback = clamp(params[FEEDBACK_PARAM].value + inputs[FEEDBACK_INPUT].value / 10.0f, 0.0f, 1.0f);
	float dry = in + lastWet * feedback;

	// Compute delay time in seconds
	float delay = 1e-3 * powf(10.0f / 1e-3, clamp(params[TIME_PARAM].value + inputs[TIME_INPUT].value / 10.0f, 0.0f, 1.0f));
	// Number of delay samples
	float index = delay * engineGetSampleRate();

	// TODO Rewrite this digital delay algorithm.

	// Push dry sample into history buffer
	if (!historyBuffer.full()) {
		historyBuffer.push(dry);
	}

	// How many samples do we need consume to catch up?
	float consume = index - historyBuffer.size();
	// printf("%f\t%d\t%f\n", index, historyBuffer.size(), consume);

	// printf("wanted: %f\tactual: %d\tdiff: %d\tratio: %f\n", index, historyBuffer.size(), consume, index / historyBuffer.size());
	if (outBuffer.empty()) {
		double ratio = 1.0f;
		if (consume <= -16)
			ratio = 0.5f;
		else if (consume >= 16)
			ratio = 2.0f;

		// printf("%f\t%lf\n", consume, ratio);
		int inFrames = min(historyBuffer.size(), 16);
		int outFrames = outBuffer.capacity();
		// printf(">\t%d\t%d\n", inFrames, outFrames);
		src.setRates(ratio * engineGetSampleRate(), engineGetSampleRate());
		src.process((const Frame<1>*)historyBuffer.startData(), &inFrames, (Frame<1>*)outBuffer.endData(), &outFrames);
		historyBuffer.startIncr(inFrames);
		outBuffer.endIncr(outFrames);
		// printf("<\t%d\t%d\n", inFrames, outFrames);
		// printf("====================================\n");
	}

	float wet = 0.0f;
	if (!outBuffer.empty()) {
		wet = outBuffer.shift();
	}

	// Apply color to delay wet output
	// TODO Make it sound better
	float color = clamp(params[COLOR_PARAM].value + inputs[COLOR_INPUT].value / 10.0f, 0.0f, 1.0f);
	float lowpassFreq = 10000.0f * powf(10.0f, clamp(2.0f*color, 0.0f, 1.0f));
	lowpassFilter.setCutoff(lowpassFreq / engineGetSampleRate());
	lowpassFilter.process(wet);
	wet = lowpassFilter.lowpass();
	float highpassFreq = 10.0f * powf(100.0f, clamp(2.0f*color - 1.0f, 0.0f, 1.0f));
	highpassFilter.setCutoff(highpassFreq / engineGetSampleRate());
	highpassFilter.process(wet);
	wet = highpassFilter.highpass();

	lastWet = wet;

	float mix = clamp(params[MIX_PARAM].value + inputs[MIX_INPUT].value / 10.0f, 0.0f, 1.0f);
	float out = crossfade(in, wet, mix);
	outputs[OUT_OUTPUT].value = out;
}


struct DelayWidget : ModuleWidget {
	DelayWidget(Delay *module);
};

DelayWidget::DelayWidget(Delay *module) : ModuleWidget(module) {
	setPanel(SVG::load(assetPlugin(plugin, "res/Delay.svg")));

	addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(ParamWidget::create<RoundBlackKnob>(Vec(67, 57), module, Delay::TIME_PARAM, 0.0f, 1.0f, 0.5f));
	addParam(ParamWidget::create<RoundBlackKnob>(Vec(67, 123), module, Delay::FEEDBACK_PARAM, 0.0f, 1.0f, 0.5f));
	addParam(ParamWidget::create<RoundBlackKnob>(Vec(67, 190), module, Delay::COLOR_PARAM, 0.0f, 1.0f, 0.5f));
	addParam(ParamWidget::create<RoundBlackKnob>(Vec(67, 257), module, Delay::MIX_PARAM, 0.0f, 1.0f, 0.5f));

	addInput(Port::create<PJ301MPort>(Vec(14, 63), Port::INPUT, module, Delay::TIME_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(14, 129), Port::INPUT, module, Delay::FEEDBACK_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(14, 196), Port::INPUT, module, Delay::COLOR_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(14, 263), Port::INPUT, module, Delay::MIX_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(14, 320), Port::INPUT, module, Delay::IN_INPUT));
	addOutput(Port::create<PJ301MPort>(Vec(73, 320), Port::OUTPUT, module, Delay::OUT_OUTPUT));
}


Model *modelDelayWidget = Model::create<Delay, DelayWidget>("Fundamental", "Delay", "Delay", DELAY_TAG);
