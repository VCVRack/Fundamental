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
	int channels;

	Merge() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
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

		// In order to allow 0 channels, modify channels directly instead of using `setChannels()`
		outputs[POLY_OUTPUT].channels = (channels >= 0) ? channels : (lastChannel + 1);

		// Set channel lights infrequently
		if (lightDivider.process()) {
			for (int c = 0; c < 16; c++) {
				bool active = (c < outputs[POLY_OUTPUT].getChannels());
				lights[CHANNEL_LIGHTS + c].setBrightness(active);
			}
		}
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


struct MergeChannelItem : MenuItem {
	Merge* module;
	int channels;
	void onAction(const event::Action& e) override {
		module->channels = channels;
	}
};


struct MergeChannelsItem : MenuItem {
	Merge* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		for (int channels = -1; channels <= 16; channels++) {
			MergeChannelItem* item = new MergeChannelItem;
			if (channels < 0)
				item->text = "Automatic";
			else
				item->text = string::f("%d", channels);
			item->rightText = CHECKMARK(module->channels == channels);
			item->module = module;
			item->channels = channels;
			menu->addChild(item);
		}
		return menu;
	}
};


struct MergeWidget : ModuleWidget {
	MergeWidget(Merge* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Merge.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.771, 37.02)), module, Merge::MONO_INPUTS + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.771, 48.02)), module, Merge::MONO_INPUTS + 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.77, 59.02)), module, Merge::MONO_INPUTS + 2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.77, 70.02)), module, Merge::MONO_INPUTS + 3));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.77, 81.02)), module, Merge::MONO_INPUTS + 4));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.77, 92.02)), module, Merge::MONO_INPUTS + 5));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.771, 103.02)), module, Merge::MONO_INPUTS + 6));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.771, 114.02)), module, Merge::MONO_INPUTS + 7));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.275, 37.02)), module, Merge::MONO_INPUTS + 8));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.275, 48.02)), module, Merge::MONO_INPUTS + 9));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.274, 59.02)), module, Merge::MONO_INPUTS + 10));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.274, 70.02)), module, Merge::MONO_INPUTS + 11));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.274, 81.02)), module, Merge::MONO_INPUTS + 12));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.274, 92.02)), module, Merge::MONO_INPUTS + 13));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.275, 103.02)), module, Merge::MONO_INPUTS + 14));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.275, 114.02)), module, Merge::MONO_INPUTS + 15));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.77, 21.347)), module, Merge::POLY_OUTPUT));

		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(15.276, 17.775)), module, Merge::CHANNEL_LIGHTS + 0));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(17.275, 17.775)), module, Merge::CHANNEL_LIGHTS + 1));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(19.275, 17.775)), module, Merge::CHANNEL_LIGHTS + 2));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(21.275, 17.775)), module, Merge::CHANNEL_LIGHTS + 3));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(15.276, 19.775)), module, Merge::CHANNEL_LIGHTS + 4));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(17.275, 19.775)), module, Merge::CHANNEL_LIGHTS + 5));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(19.275, 19.775)), module, Merge::CHANNEL_LIGHTS + 6));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(21.275, 19.775)), module, Merge::CHANNEL_LIGHTS + 7));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(15.276, 21.775)), module, Merge::CHANNEL_LIGHTS + 8));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(17.275, 21.775)), module, Merge::CHANNEL_LIGHTS + 9));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(19.275, 21.775)), module, Merge::CHANNEL_LIGHTS + 10));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(21.276, 21.775)), module, Merge::CHANNEL_LIGHTS + 11));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(15.276, 23.775)), module, Merge::CHANNEL_LIGHTS + 12));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(17.275, 23.775)), module, Merge::CHANNEL_LIGHTS + 13));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(19.275, 23.775)), module, Merge::CHANNEL_LIGHTS + 14));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(21.276, 23.775)), module, Merge::CHANNEL_LIGHTS + 15));
	}

	void appendContextMenu(Menu* menu) override {
		Merge* module = dynamic_cast<Merge*>(this->module);

		menu->addChild(new MenuEntry);

		MergeChannelsItem* channelsItem = new MergeChannelsItem;
		channelsItem->text = "Channels";
		channelsItem->rightText = RIGHT_ARROW;
		channelsItem->module = module;
		menu->addChild(channelsItem);
	}
};


Model* modelMerge = createModel<Merge, MergeWidget>("Merge");
