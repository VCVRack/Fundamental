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
	float ch1 = inputs[CH1_INPUT].value * params[CH1_PARAM].value * clamp(inputs[CH1_CV_INPUT].normalize(10.0f) / 10.0f, 0.0f, 1.0f);
	float ch2 = inputs[CH2_INPUT].value * params[CH2_PARAM].value * clamp(inputs[CH2_CV_INPUT].normalize(10.0f) / 10.0f, 0.0f, 1.0f);
	float ch3 = inputs[CH3_INPUT].value * params[CH3_PARAM].value * clamp(inputs[CH3_CV_INPUT].normalize(10.0f) / 10.0f, 0.0f, 1.0f);
	float cv = fmaxf(inputs[MIX_CV_INPUT].normalize(10.0f) / 10.0f, 0.0f);
	float mix = (ch1 + ch2 + ch3) * params[MIX_PARAM].value * cv;

	outputs[CH1_OUTPUT].value = ch1;
	outputs[CH2_OUTPUT].value = ch2;
	outputs[CH3_OUTPUT].value = ch3;
	outputs[MIX_OUTPUT].value = mix;
}


struct VCMixerWidget : ModuleWidget {
	VCMixerWidget(VCMixer *module);
};

VCMixerWidget::VCMixerWidget(VCMixer *module) : ModuleWidget(module) {
	setPanel(SVG::load(assetPlugin(plugin, "res/VCMixer.svg")));

	addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(52, 58), module, VCMixer::MIX_PARAM, 0.0f, 1.0f, 0.5f));
	addParam(ParamWidget::create<RoundBlackKnob>(Vec(57, 139), module, VCMixer::CH1_PARAM, 0.0f, 1.0f, 0.0f));
	addParam(ParamWidget::create<RoundBlackKnob>(Vec(57, 219), module, VCMixer::CH2_PARAM, 0.0f, 1.0f, 0.0f));
	addParam(ParamWidget::create<RoundBlackKnob>(Vec(57, 300), module, VCMixer::CH3_PARAM, 0.0f, 1.0f, 0.0f));

	addInput(Port::create<PJ301MPort>(Vec(16, 69), Port::INPUT, module, VCMixer::MIX_CV_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(22, 129), Port::INPUT, module, VCMixer::CH1_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(22, 160), Port::INPUT, module, VCMixer::CH1_CV_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(22, 209), Port::INPUT, module, VCMixer::CH2_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(22, 241), Port::INPUT, module, VCMixer::CH2_CV_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(22, 290), Port::INPUT, module, VCMixer::CH3_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(22, 322), Port::INPUT, module, VCMixer::CH3_CV_INPUT));

	addOutput(Port::create<PJ301MPort>(Vec(110, 69), Port::OUTPUT, module, VCMixer::MIX_OUTPUT));
	addOutput(Port::create<PJ301MPort>(Vec(110, 145), Port::OUTPUT, module, VCMixer::CH1_OUTPUT));
	addOutput(Port::create<PJ301MPort>(Vec(110, 225), Port::OUTPUT, module, VCMixer::CH2_OUTPUT));
	addOutput(Port::create<PJ301MPort>(Vec(110, 306), Port::OUTPUT, module, VCMixer::CH3_OUTPUT));
}


Model *modelVCMixer = Model::create<VCMixer, VCMixerWidget>("Fundamental", "VCMixer", "VC Mixer", MIXER_TAG, AMPLIFIER_TAG);
