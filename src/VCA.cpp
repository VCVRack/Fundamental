#include "Fundamental.hpp"


struct VCA : Module {
	enum ParamIds {
		LEVEL1_PARAM,
		LEVEL2_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		EXP1_INPUT,
		LIN1_INPUT,
		IN1_INPUT,
		EXP2_INPUT,
		LIN2_INPUT,
		IN2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT1_OUTPUT,
		OUT2_OUTPUT,
		NUM_OUTPUTS
	};

	VCA() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step() override;
};


static void stepChannel(Input &in, Param &level, Input &lin, Input &exp, Output &out) {
	float v = in.value * level.value;
	if (lin.active)
		v *= clampf(lin.value / 10.0, 0.0, 1.0);
	const float expBase = 50.0;
	if (exp.active)
		v *= rescalef(powf(expBase, clampf(exp.value / 10.0, 0.0, 1.0)), 1.0, expBase, 0.0, 1.0);
	out.value = v;
}

void VCA::step() {
	stepChannel(inputs[IN1_INPUT], params[LEVEL1_PARAM], inputs[LIN1_INPUT], inputs[EXP1_INPUT], outputs[OUT1_OUTPUT]);
	stepChannel(inputs[IN2_INPUT], params[LEVEL2_PARAM], inputs[LIN2_INPUT], inputs[EXP2_INPUT], outputs[OUT2_OUTPUT]);
}


VCAWidget::VCAWidget() {
	VCA *module = new VCA();
	setModule(module);
	box.size = Vec(15*6, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/VCA.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(createParam<RoundBlackKnob>(Vec(27, 57), module, VCA::LEVEL1_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<RoundBlackKnob>(Vec(27, 222), module, VCA::LEVEL2_PARAM, 0.0, 1.0, 0.5));

	addInput(createInput<PJ301MPort>(Vec(11, 113), module, VCA::EXP1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(54, 113), module, VCA::LIN1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(11, 156), module, VCA::IN1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(11, 276), module, VCA::EXP2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(54, 276), module, VCA::LIN2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(11, 320), module, VCA::IN2_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(54, 156), module, VCA::OUT1_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(54, 320), module, VCA::OUT2_OUTPUT));
}
