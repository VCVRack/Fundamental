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

	VCMixer() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
		params[MIX_LVL_PARAM].config(0.0, 2.0, 1.0);
		params[LVL_PARAM + 0].config(0.0, 1.0, 1.0);
		params[LVL_PARAM + 1].config(0.0, 1.0, 1.0);
		params[LVL_PARAM + 2].config(0.0, 1.0, 1.0);
		params[LVL_PARAM + 3].config(0.0, 1.0, 1.0);
	}

	void step() override {
		float mix = 0.f;
		for (int i = 0; i < 4; i++) {
			float ch = inputs[CH_INPUT + i].value;
			ch *= std::pow(params[LVL_PARAM + i].value, 2.f);
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
		setPanel(SVG::load(asset::plugin(plugin, "res/VCMixer.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<RoundLargeBlackKnob>(mm2px(Vec(19.049999, 21.161154)), module, VCMixer::MIX_LVL_PARAM));
		addParam(createParam<LEDSliderGreen>(mm2px(Vec(5.8993969, 44.33149).plus(Vec(-2, 0))), module, VCMixer::LVL_PARAM + 0));
		addParam(createParam<LEDSliderGreen>(mm2px(Vec(17.899343, 44.331486).plus(Vec(-2, 0))), module, VCMixer::LVL_PARAM + 1));
		addParam(createParam<LEDSliderGreen>(mm2px(Vec(29.899292, 44.331486).plus(Vec(-2, 0))), module, VCMixer::LVL_PARAM + 2));
		addParam(createParam<LEDSliderGreen>(mm2px(Vec(41.90065, 44.331486).plus(Vec(-2, 0))), module, VCMixer::LVL_PARAM + 3));

		// Use old interleaved order for backward compatibility with <0.6
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.2935331, 23.404598)), module, VCMixer::MIX_CV_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.2935331, 78.531639)), module, VCMixer::CH_INPUT + 0));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.2935331, 93.531586)), module, VCMixer::CV_INPUT + 0));
		addInput(createInput<PJ301MPort>(mm2px(Vec(15.29348, 78.531639)), module, VCMixer::CH_INPUT + 1));
		addInput(createInput<PJ301MPort>(mm2px(Vec(15.29348, 93.531586)), module, VCMixer::CV_INPUT + 1));
		addInput(createInput<PJ301MPort>(mm2px(Vec(27.293465, 78.531639)), module, VCMixer::CH_INPUT + 2));
		addInput(createInput<PJ301MPort>(mm2px(Vec(27.293465, 93.531586)), module, VCMixer::CV_INPUT + 2));
		addInput(createInput<PJ301MPort>(mm2px(Vec(39.293411, 78.531639)), module, VCMixer::CH_INPUT + 3));
		addInput(createInput<PJ301MPort>(mm2px(Vec(39.293411, 93.531586)), module, VCMixer::CV_INPUT + 3));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(39.293411, 23.4046)), module, VCMixer::MIX_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.2935331, 108.53153)), module, VCMixer::CH_OUTPUT + 0));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(15.29348, 108.53153)), module, VCMixer::CH_OUTPUT + 1));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(27.293465, 108.53153)), module, VCMixer::CH_OUTPUT + 2));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(39.293411, 108.53153)), module, VCMixer::CH_OUTPUT + 3));
	}
};


Model *modelVCMixer = createModel<VCMixer, VCMixerWidget>("VCMixer");
