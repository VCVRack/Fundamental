#include "plugin.hpp"


struct SHASR : Module {
	enum ParamId {
		RND_PARAM,
		PUSH_PARAM,
		CLEAR_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(IN_INPUTS, 8),
		ENUMS(TRIG_INPUTS, 8),
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(SH_OUTPUTS, 8),
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	dsp::BooleanTrigger randomizeTrigger;
	dsp::BooleanTrigger pushTrigger;
	dsp::BooleanTrigger clearTrigger;
	dsp::SchmittTrigger triggers[8];
	float values[8] = {};

	SHASR() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(RND_PARAM, "Randomize");
		configButton(PUSH_PARAM, "Push");
		configButton(CLEAR_PARAM, "Clear");
		for (int i = 0; i < 8; i++) {
			configInput(IN_INPUTS + i, string::f("Sample %d", i + 1));
			configInput(TRIG_INPUTS + i, string::f("Trigger %d", i + 1));
			configOutput(SH_OUTPUTS + i, string::f("Sample %d", i + 1));
		}
	}

	void onReset(const ResetEvent& e) override {
		for (int i = 0; i < 8; i++) {
			values[i] = 0.f;
		}
		Module::onReset(e);
	}

	void process(const ProcessArgs& args) override {
		bool randomize = randomizeTrigger.process(params[RND_PARAM].getValue());
		bool push = pushTrigger.process(params[PUSH_PARAM].getValue());
		bool clear = clearTrigger.process(params[CLEAR_PARAM].getValue());
		bool lastTrig = push;

		for (int i = 0; i < 8; i++) {
			if (inputs[TRIG_INPUTS + i].isConnected()) {
				lastTrig = triggers[i].process(inputs[TRIG_INPUTS + i].getVoltage(), 0.1f, 1.f);
			}
			if (lastTrig) {
				float lastValue = (i >= 1) ? outputs[SH_OUTPUTS + i - 1].getVoltage() : 0.f;
				values[i] = inputs[IN_INPUTS + i].getNormalVoltage(lastValue);
			}
			if (randomize) {
				values[i] = random::uniform() * 10.f;
			}
			if (clear) {
				values[i] = 0.f;
			}
		}

		// Set outputs
		for (int i = 0; i < 8; i++) {
			outputs[SH_OUTPUTS + i].setVoltage(values[i]);
		}
	}
};


struct SHASRWidget : ModuleWidget {
	SHASRWidget(SHASR* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SHASR.svg"), asset::plugin(pluginInstance, "res/SHASR-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<VCVButton>(mm2px(Vec(6.96, 21.852)), module, SHASR::RND_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(17.788, 21.852)), module, SHASR::PUSH_PARAM));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(28.6, 21.852)), module, SHASR::CLEAR_PARAM));

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(6.96, 42.113)), module, SHASR::IN_INPUTS + 0));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(17.788, 42.055)), module, SHASR::TRIG_INPUTS + 0));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(6.96, 52.241)), module, SHASR::IN_INPUTS + 1));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(17.788, 52.241)), module, SHASR::TRIG_INPUTS + 1));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(6.96, 62.368)), module, SHASR::IN_INPUTS + 2));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(17.788, 62.368)), module, SHASR::TRIG_INPUTS + 2));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(6.96, 72.496)), module, SHASR::IN_INPUTS + 3));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(17.788, 72.496)), module, SHASR::TRIG_INPUTS + 3));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(6.96, 82.623)), module, SHASR::IN_INPUTS + 4));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(17.788, 82.623)), module, SHASR::TRIG_INPUTS + 4));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(6.96, 92.75)), module, SHASR::IN_INPUTS + 5));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(17.788, 92.75)), module, SHASR::TRIG_INPUTS + 5));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(6.96, 102.878)), module, SHASR::IN_INPUTS + 6));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(17.788, 102.878)), module, SHASR::TRIG_INPUTS + 6));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(6.96, 113.005)), module, SHASR::IN_INPUTS + 7));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(17.788, 113.005)), module, SHASR::TRIG_INPUTS + 7));

		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(28.6, 42.113)), module, SHASR::SH_OUTPUTS + 0));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(28.6, 52.241)), module, SHASR::SH_OUTPUTS + 1));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(28.6, 62.368)), module, SHASR::SH_OUTPUTS + 2));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(28.6, 72.496)), module, SHASR::SH_OUTPUTS + 3));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(28.6, 82.623)), module, SHASR::SH_OUTPUTS + 4));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(28.6, 92.75)), module, SHASR::SH_OUTPUTS + 5));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(28.6, 102.878)), module, SHASR::SH_OUTPUTS + 6));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(28.6, 113.005)), module, SHASR::SH_OUTPUTS + 7));
	}
};


Model* modelSHASR = createModel<SHASR, SHASRWidget>("SHASR");