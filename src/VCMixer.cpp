#include "Fundamental.hpp"


struct VCMixer : Module {
	enum ParamIds {
		MIX_PARAM,
		CH1_PARAM,
		CH2_PARAM,
		CH3_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		MIX_CV_INPUT,
		CH1_INPUT,
		CH1_CV_INPUT,
		CH2_INPUT,
		CH2_CV_INPUT,
		CH3_INPUT,
		CH3_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MIX_OUTPUT,
		CH1_OUTPUT,
		CH2_OUTPUT,
		CH3_OUTPUT,
		NUM_OUTPUTS
	};

	VCMixer() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step() override;
};


void VCMixer::step() {
	float ch1 = inputs[CH1_INPUT].value * params[CH1_PARAM].value * clampf(inputs[CH1_CV_INPUT].normalize(10.0) / 10.0, 0.0, 1.0);
	float ch2 = inputs[CH2_INPUT].value * params[CH2_PARAM].value * clampf(inputs[CH2_CV_INPUT].normalize(10.0) / 10.0, 0.0, 1.0);
	float ch3 = inputs[CH3_INPUT].value * params[CH3_PARAM].value * clampf(inputs[CH3_CV_INPUT].normalize(10.0) / 10.0, 0.0, 1.0);
	float cv = fmaxf(inputs[MIX_CV_INPUT].normalize(10.0) / 10.0, 0.0);
	float mix = (ch1 + ch2 + ch3) * params[MIX_PARAM].value * cv;

	outputs[CH1_OUTPUT].value = ch1;
	outputs[CH2_OUTPUT].value = ch2;
	outputs[CH3_OUTPUT].value = ch3;
	outputs[MIX_OUTPUT].value = mix;
}


VCMixerWidget::VCMixerWidget() {
	VCMixer *module = new VCMixer();
	setModule(module);
	box.size = Vec(15*10, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/VCMixer.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(createParam<RoundLargeBlackKnob>(Vec(52, 58), module, VCMixer::MIX_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<RoundBlackKnob>(Vec(57, 139), module, VCMixer::CH1_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<RoundBlackKnob>(Vec(57, 219), module, VCMixer::CH2_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<RoundBlackKnob>(Vec(57, 300), module, VCMixer::CH3_PARAM, 0.0, 1.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(16, 69), module, VCMixer::MIX_CV_INPUT));
	addInput(createInput<PJ301MPort>(Vec(22, 129), module, VCMixer::CH1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(22, 160), module, VCMixer::CH1_CV_INPUT));
	addInput(createInput<PJ301MPort>(Vec(22, 209), module, VCMixer::CH2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(22, 241), module, VCMixer::CH2_CV_INPUT));
	addInput(createInput<PJ301MPort>(Vec(22, 290), module, VCMixer::CH3_INPUT));
	addInput(createInput<PJ301MPort>(Vec(22, 322), module, VCMixer::CH3_CV_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(110, 69), module, VCMixer::MIX_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(110, 145), module, VCMixer::CH1_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(110, 225), module, VCMixer::CH2_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(110, 306), module, VCMixer::CH3_OUTPUT));
}
