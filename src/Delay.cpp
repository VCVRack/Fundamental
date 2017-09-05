#include "Fundamental.hpp"


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
	float lastWet = 0.0;
	RCFilter lowpassFilter;
	RCFilter highpassFilter;

	Delay();

	void step();
};


Delay::Delay() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);
}

void Delay::step() {
	// Get input to delay block
	float in = getf(inputs[IN_INPUT]);
	float feedback = clampf(params[FEEDBACK_PARAM] + getf(inputs[FEEDBACK_INPUT]) / 10.0, 0.0, 0.99);
	float dry = in + lastWet * feedback;

	// Compute delay time in seconds
	float delay = 1e-3 * powf(10.0 / 1e-3, clampf(params[TIME_PARAM] + getf(inputs[TIME_INPUT]) / 10.0, 0.0, 1.0));
	// Number of delay samples
	float index = delay * gSampleRate;

	// TODO This is a horrible digital delay algorithm. Rewrite later.

	// Push dry sample into history buffer
	if (!historyBuffer.full()) {
		historyBuffer.push(dry);
	}

	// How many samples do we need consume to catch up?
	float consume = index - historyBuffer.size();
	// printf("%f\t%d\t%f\n", index, historyBuffer.size(), consume);

	// printf("wanted: %f\tactual: %d\tdiff: %d\tratio: %f\n", index, historyBuffer.size(), consume, index / historyBuffer.size());
	if (outBuffer.empty()) {
		// Idk wtf I'm doing
		double ratio = 1.0;
		if (consume <= -16)
			ratio = 0.5;
		else if (consume >= 16)
			ratio = 2.0;

		// printf("%f\t%lf\n", consume, ratio);
		int inFrames = mini(historyBuffer.size(), 16);
		int outFrames = outBuffer.capacity();
		// printf(">\t%d\t%d\n", inFrames, outFrames);
		src.setRatioSmooth(ratio);
		src.process((const Frame<1>*)historyBuffer.startData(), &inFrames, (Frame<1>*)outBuffer.endData(), &outFrames);
		historyBuffer.startIncr(inFrames);
		outBuffer.endIncr(outFrames);
		// printf("<\t%d\t%d\n", inFrames, outFrames);
		// printf("====================================\n");
	}

	float wet = 0.0;
	if (!outBuffer.empty()) {
		wet = outBuffer.shift();
	}

	// Apply color to delay wet output
	// TODO Make it sound better
	float color = clampf(params[COLOR_PARAM] + getf(inputs[COLOR_INPUT]) / 10.0, 0.0, 1.0);
	float lowpassFreq = 10000.0 * powf(10.0, clampf(2.0*color, 0.0, 1.0));
	lowpassFilter.setCutoff(lowpassFreq / gSampleRate);
	lowpassFilter.process(wet);
	wet = lowpassFilter.lowpass();
	float highpassFreq = 10.0 * powf(100.0, clampf(2.0*color - 1.0, 0.0, 1.0));
	highpassFilter.setCutoff(highpassFreq / gSampleRate);
	highpassFilter.process(wet);
	wet = highpassFilter.highpass();

	lastWet = wet;

	float mix = clampf(params[MIX_PARAM] + getf(inputs[MIX_INPUT]) / 10.0, 0.0, 1.0);
	float out = crossf(in, wet, mix);
	setf(outputs[OUT_OUTPUT], out);
}


DelayWidget::DelayWidget() {
	Delay *module = new Delay();
	setModule(module);
	box.size = Vec(15*8, 380);

	{
		Panel *panel = new LightPanel();
		panel->box.size = box.size;
		panel->backgroundImage = Image::load("plugins/Fundamental/res/Delay.png");
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(createParam<Davies1900hBlackKnob>(Vec(67, 57), module, Delay::TIME_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Davies1900hBlackKnob>(Vec(67, 123), module, Delay::FEEDBACK_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Davies1900hBlackKnob>(Vec(67, 190), module, Delay::COLOR_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Davies1900hBlackKnob>(Vec(67, 257), module, Delay::MIX_PARAM, 0.0, 1.0, 0.5));

	addInput(createInput<PJ301MPort>(Vec(14, 63), module, Delay::TIME_INPUT));
	addInput(createInput<PJ301MPort>(Vec(14, 129), module, Delay::FEEDBACK_INPUT));
	addInput(createInput<PJ301MPort>(Vec(14, 196), module, Delay::COLOR_INPUT));
	addInput(createInput<PJ301MPort>(Vec(14, 263), module, Delay::MIX_INPUT));
	addInput(createInput<PJ301MPort>(Vec(14, 320), module, Delay::IN_INPUT));
	addOutput(createOutput<PJ301MPort>(Vec(73, 320), module, Delay::OUT_OUTPUT));
}
