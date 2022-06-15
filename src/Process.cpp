#include "plugin.hpp"


struct Process : Module {
	enum ParamId {
		SLEW_PARAM,
		PUSH_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		SLEW_INPUT,
		IN_INPUT,
		GATE_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		SH1_OUTPUT,
		SH2_OUTPUT,
		TH_OUTPUT,
		HT_OUTPUT,
		SLEW_OUTPUT,
		GLIDE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Process() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(SLEW_PARAM, std::log2(1e-3f), std::log2(10.f), std::log2(0.1f), "Slew", " ms/V", 2, 1000);
		configButton(PUSH_PARAM, "Gate");
		configInput(SLEW_INPUT, "Slew");
		configInput(IN_INPUT, "Voltage");
		configInput(GATE_INPUT, "Gate");
		configOutput(SH1_OUTPUT, "Sample & hold");
		configOutput(SH2_OUTPUT, "Sample & hold 2");
		configOutput(TH_OUTPUT, "Trigger & hold");
		configOutput(HT_OUTPUT, "Hold & trigger");
		configOutput(SLEW_OUTPUT, "Slew");
		configOutput(GLIDE_OUTPUT, "Glide");
	}

	void process(const ProcessArgs& args) override {
	}
};


struct ProcessWidget : ModuleWidget {
	ProcessWidget(Process* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Process.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(12.646, 26.755)), module, Process::SLEW_PARAM));
		// addParam(createParamCentered<LEDLightBezel>(mm2px(Vec(18.136, 52.31)), module, Process::PUSH_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.299, 52.31)), module, Process::SLEW_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.297, 67.53)), module, Process::IN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.122, 67.53)), module, Process::GATE_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.297, 82.732)), module, Process::SH1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 82.732)), module, Process::SH2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.297, 97.958)), module, Process::TH_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 97.923)), module, Process::HT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.297, 113.115)), module, Process::SLEW_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 113.115)), module, Process::GLIDE_OUTPUT));
	}
};


Model* modelProcess = createModel<Process, ProcessWidget>("Process");