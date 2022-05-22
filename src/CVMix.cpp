#include "plugin.hpp"


struct CVMix : Module {
	enum ParamId {
		ENUMS(LEVEL_PARAMS, 3),
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(CV_INPUTS, 3),
		INPUTS_LEN
	};
	enum OutputId {
		MIX_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	CVMix() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < 3; i++)
			configParam(LEVEL_PARAMS + i, -1.f, 1.f, 0.f, string::f("Level %d", i + 1), "%", 0, 100);
		for (int i = 0; i < 3; i++)
			configInput(CV_INPUTS + i, string::f("CV %d", i + 1));
		configOutput(MIX_OUTPUT, "Mix");
	}

	void process(const ProcessArgs& args) override {
		if (!outputs[MIX_OUTPUT].isConnected())
			return;

		using simd::float_4;

		// Get number of channels
		int channels = 1;
		for (int i = 0; i < 3; i++)
			channels = std::max(channels, inputs[CV_INPUTS + i].getChannels());

		for (int c = 0; c < channels; c += 4) {
			// Sum CV inputs
			float_4 mix = 0.f;
			for (int i = 0; i < 3; i++) {
				float_4 cv;
				// Normalize first input to 10V
				if (i == 0)
					cv = inputs[CV_INPUTS + i].getNormalPolyVoltageSimd<float_4>(10.f, c);
				else
					cv = inputs[CV_INPUTS + i].getPolyVoltageSimd<float_4>(c);

				// Apply gain
				cv *= params[LEVEL_PARAMS + i].getValue();
				mix += cv;
			}

			// Set mix output
			outputs[MIX_OUTPUT].setVoltageSimd(mix, c);
		}
		outputs[MIX_OUTPUT].setChannels(channels);
	}
};


struct CVMixWidget : ModuleWidget {
	CVMixWidget(CVMix* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/CVMix.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 24.723)), module, CVMix::LEVEL_PARAMS + 0));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 41.327)), module, CVMix::LEVEL_PARAMS + 1));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 57.922)), module, CVMix::LEVEL_PARAMS + 2));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 76.539)), module, CVMix::CV_INPUTS + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 86.699)), module, CVMix::CV_INPUTS + 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 96.859)), module, CVMix::CV_INPUTS + 2));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 113.115)), module, CVMix::MIX_OUTPUT));
	}
};


Model* modelCVMix = createModel<CVMix, CVMixWidget>("CVMix");