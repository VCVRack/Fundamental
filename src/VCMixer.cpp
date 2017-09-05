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

	VCMixer();
	void step();
};


VCMixer::VCMixer() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);
}


void VCMixer::step() {
	float ch1 = getf(inputs[CH1_INPUT]) * params[CH1_PARAM] * clampf(getf(inputs[CH1_CV_INPUT], 10.0) / 10.0, 0.0, 1.0);
	float ch2 = getf(inputs[CH2_INPUT]) * params[CH2_PARAM] * clampf(getf(inputs[CH2_CV_INPUT], 10.0) / 10.0, 0.0, 1.0);
	float ch3 = getf(inputs[CH3_INPUT]) * params[CH3_PARAM] * clampf(getf(inputs[CH3_CV_INPUT], 10.0) / 10.0, 0.0, 1.0);
	float mix = (ch1 + ch2 + ch3) * params[MIX_PARAM] * getf(inputs[MIX_CV_INPUT], 10.0) / 10.0;

	setf(outputs[CH1_OUTPUT], ch1);
	setf(outputs[CH2_OUTPUT], ch2);
	setf(outputs[CH3_OUTPUT], ch3);
	setf(outputs[MIX_OUTPUT], mix);
}


VCMixerWidget::VCMixerWidget() {
	VCMixer *module = new VCMixer();
	setModule(module);
	box.size = Vec(15*10, 380);

	{
		Panel *panel = new LightPanel();
		panel->box.size = box.size;
		panel->backgroundImage = Image::load("plugins/Fundamental/res/VCMixer.png");
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(createParam<Davies1900hLargeBlackKnob>(Vec(48, 55), module, VCMixer::MIX_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Davies1900hBlackKnob>(Vec(57, 139), module, VCMixer::CH1_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(57, 219), module, VCMixer::CH2_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(57, 300), module, VCMixer::CH3_PARAM, 0.0, 1.0, 0.0));

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
