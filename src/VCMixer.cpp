#include "Fundamental.hpp"


struct VCMixer : Module {
	enum ParamIds {
		MIX_LVL_PARAM,
		ENUMS(LVL_PARAM, 4),
		NUM_PARAMS
	};
	enum InputIds {
		MIX_CV_INPUT,
		ENUMS(CH_INPUT, 4),
		ENUMS(CV_INPUT, 4),
		NUM_INPUTS
	};
	enum OutputIds {
		MIX_OUTPUT,
		ENUMS(CH_OUTPUT, 4),
		NUM_OUTPUTS
	};

	VCMixer() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}

	void step() override {
		float mix = 0.f;
		for (int i = 0; i < 4; i++) {
			float ch = inputs[CH_INPUT + i].value;
			ch *= powf(params[LVL_PARAM + i].value, 2.f);
			if (inputs[CV_INPUT + i].active)
				ch *= clamp(inputs[CV_INPUT + i].value / 10.f, 0.f, 1.f);
			outputs[CH_OUTPUT + i].value = ch;
			mix += ch;
		}
		mix *= params[MIX_LVL_PARAM].value;
		if (inputs[MIX_CV_INPUT].active)
			mix *= clamp(inputs[MIX_CV_INPUT].value / 10.f, 0.f, 1.f);
		outputs[MIX_OUTPUT].value = mix;
	}
};


struct VCMixerWidget : ModuleWidget {
	VCMixerWidget(VCMixer *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/VCMixer.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<RoundLargeBlackKnob>(mm2px(Vec(19.049999, 21.161154)), module, VCMixer::MIX_LVL_PARAM, 0.0, 2.0, 1.0));
		addParam(createParam<LEDSliderGreen>(mm2px(Vec(5.8993969, 44.33149).plus(Vec(-2, 0))), module, VCMixer::LVL_PARAM + 0, 0.0, 1.0, 1.0));
		addParam(createParam<LEDSliderGreen>(mm2px(Vec(17.899343, 44.331486).plus(Vec(-2, 0))), module, VCMixer::LVL_PARAM + 1, 0.0, 1.0, 1.0));
		addParam(createParam<LEDSliderGreen>(mm2px(Vec(29.899292, 44.331486).plus(Vec(-2, 0))), module, VCMixer::LVL_PARAM + 2, 0.0, 1.0, 1.0));
		addParam(createParam<LEDSliderGreen>(mm2px(Vec(41.90065, 44.331486).plus(Vec(-2, 0))), module, VCMixer::LVL_PARAM + 3, 0.0, 1.0, 1.0));

		// Use old interleaved order for backward compatibility with <0.6
		addInput(createPort<PJ301MPort>(mm2px(Vec(3.2935331, 23.404598)), PortWidget::INPUT, module, VCMixer::MIX_CV_INPUT));
		addInput(createPort<PJ301MPort>(mm2px(Vec(3.2935331, 78.531639)), PortWidget::INPUT, module, VCMixer::CH_INPUT + 0));
		addInput(createPort<PJ301MPort>(mm2px(Vec(3.2935331, 93.531586)), PortWidget::INPUT, module, VCMixer::CV_INPUT + 0));
		addInput(createPort<PJ301MPort>(mm2px(Vec(15.29348, 78.531639)), PortWidget::INPUT, module, VCMixer::CH_INPUT + 1));
		addInput(createPort<PJ301MPort>(mm2px(Vec(15.29348, 93.531586)), PortWidget::INPUT, module, VCMixer::CV_INPUT + 1));
		addInput(createPort<PJ301MPort>(mm2px(Vec(27.293465, 78.531639)), PortWidget::INPUT, module, VCMixer::CH_INPUT + 2));
		addInput(createPort<PJ301MPort>(mm2px(Vec(27.293465, 93.531586)), PortWidget::INPUT, module, VCMixer::CV_INPUT + 2));
		addInput(createPort<PJ301MPort>(mm2px(Vec(39.293411, 78.531639)), PortWidget::INPUT, module, VCMixer::CH_INPUT + 3));
		addInput(createPort<PJ301MPort>(mm2px(Vec(39.293411, 93.531586)), PortWidget::INPUT, module, VCMixer::CV_INPUT + 3));

		addOutput(createPort<PJ301MPort>(mm2px(Vec(39.293411, 23.4046)), PortWidget::OUTPUT, module, VCMixer::MIX_OUTPUT));
		addOutput(createPort<PJ301MPort>(mm2px(Vec(3.2935331, 108.53153)), PortWidget::OUTPUT, module, VCMixer::CH_OUTPUT + 0));
		addOutput(createPort<PJ301MPort>(mm2px(Vec(15.29348, 108.53153)), PortWidget::OUTPUT, module, VCMixer::CH_OUTPUT + 1));
		addOutput(createPort<PJ301MPort>(mm2px(Vec(27.293465, 108.53153)), PortWidget::OUTPUT, module, VCMixer::CH_OUTPUT + 2));
		addOutput(createPort<PJ301MPort>(mm2px(Vec(39.293411, 108.53153)), PortWidget::OUTPUT, module, VCMixer::CH_OUTPUT + 3));
	}
};


Model *modelVCMixer = createModel<VCMixer, VCMixerWidget>("VCMixer");
