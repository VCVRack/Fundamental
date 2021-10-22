#include "plugin.hpp"


struct Merge : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(MONO_INPUTS, 16),
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(CHANNEL_LIGHTS, 16),
		NUM_LIGHTS
	};

	dsp::ClockDivider lightDivider;
	int channels = -1;
	int automaticChannels = 0;

	Merge() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i = 0; i < 16; i++)
			configInput(MONO_INPUTS + i, string::f("Channel %d", i + 1));
		configOutput(POLY_OUTPUT, "Polyphonic");

		lightDivider.setDivision(512);
		onReset();
	}

	void onReset() override {
		channels = -1;
	}

	void process(const ProcessArgs& args) override {
		int lastChannel = -1;
		for (int c = 0; c < 16; c++) {
			float v = 0.f;
			if (inputs[MONO_INPUTS + c].isConnected()) {
				lastChannel = c;
				v = inputs[MONO_INPUTS + c].getVoltage();
			}
			outputs[POLY_OUTPUT].setVoltage(v, c);
		}
		automaticChannels = lastChannel + 1;

		// In order to allow 0 channels, modify `channels` directly instead of using `setChannels()`
		outputs[POLY_OUTPUT].channels = (channels >= 0) ? channels : automaticChannels;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "channels", json_integer(channels));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* channelsJ = json_object_get(rootJ, "channels");
		if (channelsJ)
			channels = json_integer_value(channelsJ);
	}
};


struct MergeChannelDisplay : ChannelDisplay {
	Merge* module;
	void step() override {
		int channels = 16;
		if (module) {
			channels = module->channels;
			if (channels < 0)
				channels = module->automaticChannels;
		}

		text = string::f("%d", channels);
	}
};


struct MergeWidget : ModuleWidget {
	MergeWidget(Merge* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Merge.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.281, 41.995)), module, Merge::MONO_INPUTS + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.281, 52.155)), module, Merge::MONO_INPUTS + 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.281, 62.315)), module, Merge::MONO_INPUTS + 2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.281, 72.475)), module, Merge::MONO_INPUTS + 3));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.281, 82.635)), module, Merge::MONO_INPUTS + 4));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.281, 92.795)), module, Merge::MONO_INPUTS + 5));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.281, 102.955)), module, Merge::MONO_INPUTS + 6));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.281, 113.115)), module, Merge::MONO_INPUTS + 7));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.119, 41.995)), module, Merge::MONO_INPUTS + 8));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.119, 52.155)), module, Merge::MONO_INPUTS + 9));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.119, 62.315)), module, Merge::MONO_INPUTS + 10));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.119, 72.475)), module, Merge::MONO_INPUTS + 11));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.119, 82.635)), module, Merge::MONO_INPUTS + 12));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.119, 92.795)), module, Merge::MONO_INPUTS + 13));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.119, 102.955)), module, Merge::MONO_INPUTS + 14));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.119, 113.115)), module, Merge::MONO_INPUTS + 15));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.281, 21.967)), module, Merge::POLY_OUTPUT));

		MergeChannelDisplay* display = createWidget<MergeChannelDisplay>(mm2px(Vec(14.02, 18.611)));
		display->box.size = mm2px(Vec(8.197, 8.197));
		display->module = module;
		addChild(display);
	}

	void appendContextMenu(Menu* menu) override {
		Merge* module = dynamic_cast<Merge*>(this->module);

		menu->addChild(new MenuSeparator);

		std::vector<std::string> channelLabels;
		channelLabels.push_back(string::f("Automatic (%d)", module->automaticChannels));
		for (int i = 0; i <= 16; i++) {
			channelLabels.push_back(string::f("%d", i));
		}
		menu->addChild(createIndexSubmenuItem("Channels", channelLabels,
			[=]() {return module->channels + 1;},
			[=](int i) {module->channels = i - 1;}
		));
	}
};


Model* modelMerge = createModel<Merge, MergeWidget>("Merge");
