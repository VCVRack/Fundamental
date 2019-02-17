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

	dsp::Counter lightCounter;

	Merge() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		lightCounter.setPeriod(512);
	}

	void process(const ProcessArgs &args) override {
		int lastChannel = -1;
		for (int c = 0; c < 16; c++) {
			if (inputs[MONO_INPUTS + c].isConnected()) {
				lastChannel = c;
				float v = inputs[MONO_INPUTS + c].getVoltage();
				outputs[POLY_OUTPUT].setVoltage(v, c);
			}
		}

		// This also sets higher channels to 0V if the number of channels shrinks.
		outputs[POLY_OUTPUT].setChannels(lastChannel + 1);

		// Set channel lights infrequently
		if (lightCounter.process()) {
			for (int c = 0; c < 16; c++) {
				bool active = (c < outputs[POLY_OUTPUT].getChannels());
				lights[CHANNEL_LIGHTS + c].setBrightness(active);
			}
		}
	}
};


struct MergeWidget : ModuleWidget {
	MergeWidget(Merge *module) {
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
};


Model *modelMerge = createModel<Merge, MergeWidget>("Merge");
