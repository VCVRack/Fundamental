#include "plugin.hpp"


struct Split : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		POLY_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(MONO_OUTPUTS, 16),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(CHANNEL_LIGHTS, 16),
		NUM_LIGHTS
	};

	int lastChannels = 0;
	dsp::ClockDivider lightDivider;

	Split() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configInput(POLY_INPUT, "Polyphonic");
		for (int i = 0; i < 16; i++)
			configOutput(MONO_OUTPUTS + i, string::f("Channel %d", i + 1));

		lightDivider.setDivision(512);
	}

	void process(const ProcessArgs& args) override {
		for (int c = 0; c < 16; c++) {
			float v = inputs[POLY_INPUT].getVoltage(c);
			// To allow users to debug buggy modules, don't assume that undefined channel voltages are 0V.
			outputs[MONO_OUTPUTS + c].setVoltage(v);
		}

		lastChannels = inputs[POLY_INPUT].getChannels();
	}
};


struct SplitChannelDisplay : ChannelDisplay {
	Split* module;
	void step() override {
		int channels = module ? module->lastChannels : 16;
		text = string::f("%d", channels);
	}
};


struct SplitWidget : ModuleWidget {
	SplitWidget(Split* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Split.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.281, 21.967)), module, Split::POLY_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.281, 41.995)), module, Split::MONO_OUTPUTS + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.281, 52.155)), module, Split::MONO_OUTPUTS + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.281, 62.315)), module, Split::MONO_OUTPUTS + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.281, 72.475)), module, Split::MONO_OUTPUTS + 3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.281, 82.635)), module, Split::MONO_OUTPUTS + 4));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.281, 92.795)), module, Split::MONO_OUTPUTS + 5));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.281, 102.955)), module, Split::MONO_OUTPUTS + 6));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.281, 113.115)), module, Split::MONO_OUTPUTS + 7));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.119, 41.995)), module, Split::MONO_OUTPUTS + 8));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.119, 52.155)), module, Split::MONO_OUTPUTS + 9));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.119, 62.315)), module, Split::MONO_OUTPUTS + 10));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.119, 72.475)), module, Split::MONO_OUTPUTS + 11));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.119, 82.635)), module, Split::MONO_OUTPUTS + 12));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.119, 92.795)), module, Split::MONO_OUTPUTS + 13));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.119, 102.955)), module, Split::MONO_OUTPUTS + 14));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.119, 113.115)), module, Split::MONO_OUTPUTS + 15));

		SplitChannelDisplay* display = createWidget<SplitChannelDisplay>(mm2px(Vec(14.02, 18.611)));
		display->box.size = mm2px(Vec(8.197, 8.197));
		display->module = module;
		addChild(display);
	}
};


Model* modelSplit = createModel<Split, SplitWidget>("Split");
