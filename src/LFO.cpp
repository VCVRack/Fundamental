#include "Fundamental.hpp"


struct LFO : Module {
	enum ParamIds {
		OFFSET_PARAM,
		INVERT_PARAM,
		FREQ_PARAM,
		FM1_PARAM,
		FM2_PARAM,
		PW_PARAM,
		PWM_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FM1_INPUT,
		FM2_INPUT,
		RESET_INPUT,
		PW_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		SIN_OUTPUT,
		TRI_OUTPUT,
		SAW_OUTPUT,
		SQR_OUTPUT,
		NUM_OUTPUTS
	};

	float phase = 0.0;

	float lights[1] = {};

	LFO() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step();
};


void LFO::step() {
	// Compute frequency
	float pitch = params[FREQ_PARAM].value;
	pitch += params[FM1_PARAM].value * inputs[FM1_INPUT].value;
	pitch += params[FM2_PARAM].value * inputs[FM2_INPUT].value;
	pitch = fminf(pitch, 8.0);
	float freq = powf(2.0, pitch);

	// Pulse width
	const float pwMin = 0.01;
	float pw = clampf(params[PW_PARAM].value + params[PWM_PARAM].value * inputs[PW_INPUT].value / 10.0, pwMin, 1.0 - pwMin);

	// Advance phase
	float deltaPhase = fminf(freq / gSampleRate, 0.5);
	phase += deltaPhase;
	if (phase >= 1.0)
		phase -= 1.0;

	float offset = params[OFFSET_PARAM].value > 0.0 ? 5.0 : 0.0;
	float factor = params[INVERT_PARAM].value > 0.0 ? 5.0 : -5.0;

	// Outputs
	float sin = sinf(2*M_PI * phase);

	float tri;
	if (phase < 0.25)
		tri = 4.0*phase;
	else if (phase < 0.75)
		tri = 2.0 - 4.0*phase;
	else
		tri = -4.0 + 4.0*phase;

	float saw;
	if (phase < 0.5)
		saw = 2.0*phase;
	else
		saw = -2.0 + 2.0*phase;

	float sqr = phase < pw ? 1.0 : -1.0;

	outputs[SIN_OUTPUT].value = factor * sin + offset;
	outputs[TRI_OUTPUT].value = factor * tri + offset;
	outputs[SAW_OUTPUT].value = factor * saw + offset;
	outputs[SQR_OUTPUT].value = factor * sqr + offset;

	// Lights
	lights[0] = -1.0 + 2.0*phase;
}


LFOWidget::LFOWidget() {
	LFO *module = new LFO();
	setModule(module);
	box.size = Vec(15*10, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/LFO-1.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(createParam<CKSS>(Vec(15, 77), module, LFO::OFFSET_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<CKSS>(Vec(119, 77), module, LFO::INVERT_PARAM, 0.0, 1.0, 1.0));

	addParam(createParam<RoundHugeBlackKnob>(Vec(47, 61), module, LFO::FREQ_PARAM, -8.0, 6.0, -1.0));
	addParam(createParam<RoundBlackKnob>(Vec(23, 143), module, LFO::FM1_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<RoundBlackKnob>(Vec(91, 143), module, LFO::PW_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<RoundBlackKnob>(Vec(23, 208), module, LFO::FM2_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<RoundBlackKnob>(Vec(91, 208), module, LFO::PWM_PARAM, 0.0, 1.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(11, 276), module, LFO::FM1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(45, 276), module, LFO::FM2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(80, 276), module, LFO::RESET_INPUT));
	addInput(createInput<PJ301MPort>(Vec(114, 276), module, LFO::PW_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(11, 320), module, LFO::SIN_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(45, 320), module, LFO::TRI_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(80, 320), module, LFO::SAW_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(114, 320), module, LFO::SQR_OUTPUT));

	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(99, 41), &module->lights[0]));
}
