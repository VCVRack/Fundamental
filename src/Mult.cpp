#include "plugin.hpp"


struct Mult : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		MULT_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(MULT_OUTPUTS, 8),
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Mult() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configInput(MULT_INPUT, "Mult");
		for (int i = 0; i < 8; i++)
			configOutput(MULT_OUTPUTS + i, string::f("Mult %d", i + 1));
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(1, inputs[MULT_INPUT].getChannels());

		// Copy input to outputs
		for (int i = 0; i < 8; i++) {
			outputs[MULT_OUTPUTS + i].setChannels(channels);
			outputs[MULT_OUTPUTS + i].writeVoltages(inputs[MULT_INPUT].getVoltages());
		}
	}
};


struct MultWidget : ModuleWidget {
	MultWidget(Mult* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Mult.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 22.001)), module, Mult::MULT_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 42.017)), module, Mult::MULT_OUTPUTS + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 52.155)), module, Mult::MULT_OUTPUTS + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 62.315)), module, Mult::MULT_OUTPUTS + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 72.475)), module, Mult::MULT_OUTPUTS + 3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 82.635)), module, Mult::MULT_OUTPUTS + 4));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 92.795)), module, Mult::MULT_OUTPUTS + 5));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 102.955)), module, Mult::MULT_OUTPUTS + 6));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 113.115)), module, Mult::MULT_OUTPUTS + 7));
	}
};


Model* modelMult = createModel<Mult, MultWidget>("Mult");