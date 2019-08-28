#include "plugin.hpp"


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
		// x^1 scaling up to 6 dB
		configParam(MIX_LVL_PARAM, 0.0, 2.0, 1.0, "Master level", " dB", -10, 20);
		// x^2 scaling up to 6 dB
		configParam(LVL_PARAM + 0, 0.0, M_SQRT2, 1.0, "Ch 1 level", " dB", -10, 40);
		configParam(LVL_PARAM + 1, 0.0, M_SQRT2, 1.0, "Ch 2 level", " dB", -10, 40);
		configParam(LVL_PARAM + 2, 0.0, M_SQRT2, 1.0, "Ch 3 level", " dB", -10, 40);
		configParam(LVL_PARAM + 3, 0.0, M_SQRT2, 1.0, "Ch 4 level", " dB", -10, 40);
	}

	void process(const ProcessArgs &args) override {
		float mix = 0.f;

		// Channels
		for (int i = 0; i < 4; i++) {
			float in = 0.f;

			if (inputs[CH_INPUT + i].isConnected()) {
				// Get input
				in = inputs[CH_INPUT + i].getVoltageSum();

				// Apply fader gain
				float gain = std::pow(params[LVL_PARAM + i].getValue(), 2.f);
				in *= gain;

				// Apply CV gain
				if (inputs[CV_INPUT + i].isConnected()) {
					float cv = clamp(inputs[CV_INPUT + i].getVoltage() / 10.f, 0.f, 1.f);
					in *= cv;
				}

				// Add to mix
				mix += in;
			}

			// Set channel output
			outputs[CH_OUTPUT + i].setVoltage(in);
		}

		// Mix output
		if (outputs[MIX_OUTPUT].isConnected()) {
			// Apply mix knob gain
			float gain = params[MIX_LVL_PARAM].getValue();
			mix *= gain;

			// Apply mix CV gain
			if (inputs[MIX_CV_INPUT].isConnected()) {
				float cv = clamp(inputs[MIX_CV_INPUT].getVoltage() / 10.f, 0.f, 1.f);
				mix *= cv;
			}

			// Set mix output
			outputs[MIX_OUTPUT].setVoltage(mix);
		}
	}
};


struct VCMixerWidget : ModuleWidget {
	VCMixerWidget(VCMixer *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/VCMixer.svg")));

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
