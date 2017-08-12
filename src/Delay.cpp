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
	float lastIndex = 0.0;

	Delay();

	void step();
};


Delay::Delay() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);
}

void Delay::step() {
	// Compute delay time
	float delay = 1e-3 * powf(10.0 / 1e-3, clampf(params[TIME_PARAM] + getf(inputs[TIME_INPUT]) / 10.0, 0.0, 1.0));
	float index = delay * gSampleRate;
	// lastIndex = crossf(lastIndex, index, 0.001);

	// Read the history
	int consume = (int)(historyBuffer.size() - index);
	// printf("wanted: %f\tactual: %d\tdiff: %d\tratio: %f\n", index, historyBuffer.size(), consume, index / historyBuffer.size());
	if (outBuffer.empty()) {
		if (consume > 0) {
			int inFrames = consume;
			int outFrames = outBuffer.capacity();
			// printf("\t%d\t%d\n", inFrames, outFrames);
			src.setRatioSmooth(index / historyBuffer.size());
			src.process((const Frame<1>*)historyBuffer.startData(), &inFrames, (Frame<1>*)outBuffer.endData(), &outFrames);
			historyBuffer.startIncr(inFrames);
			outBuffer.endIncr(outFrames);
			// printf("\t%d\t%d\n", inFrames, outFrames);
			// if (historyBuffer.size() >= index)
			// 	outBuffer.push(historyBuffer.shift());
		}
	}

	float out = 0.0;
	if (!outBuffer.empty()) {
		out = outBuffer.shift();
	}

	// Write the history
	float in = getf(inputs[IN_INPUT]);
	historyBuffer.push(in + out * clampf(params[FEEDBACK_PARAM] + getf(inputs[FEEDBACK_INPUT]) / 10.0, 0.0, 1.0));
	out = crossf(in, out, clampf(params[MIX_PARAM] + getf(inputs[MIX_INPUT]) / 10.0, 0.0, 1.0));

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
	addParam(createParam<Davies1900hBlackKnob>(Vec(67, 190), module, Delay::COLOR_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(67, 257), module, Delay::MIX_PARAM, 0.0, 1.0, 0.5));

	addInput(createInput<PJ301MPort>(Vec(14, 63), module, Delay::TIME_INPUT));
	addInput(createInput<PJ301MPort>(Vec(14, 129), module, Delay::FEEDBACK_INPUT));
	addInput(createInput<PJ301MPort>(Vec(14, 196), module, Delay::COLOR_INPUT));
	addInput(createInput<PJ301MPort>(Vec(14, 263), module, Delay::MIX_INPUT));
	addInput(createInput<PJ301MPort>(Vec(14, 320), module, Delay::IN_INPUT));
	addOutput(createOutput<PJ301MPort>(Vec(73, 320), module, Delay::OUT_OUTPUT));
}
