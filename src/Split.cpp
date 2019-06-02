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

	dsp::ClockDivider lightDivider;

	Split() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		lightDivider.setDivision(512);
	}

	void process(const ProcessArgs &args) override {
		for (int c = 0; c < 16; c++) {
			float v = inputs[POLY_INPUT].getVoltage(c);
			// To allow users to debug buggy modules, don't assume that undefined channel voltages are 0V.
			outputs[MONO_OUTPUTS + c].setVoltage(v);
		}

		// Set channel lights infrequently
		if (lightDivider.process()) {
			for (int c = 0; c < 16; c++) {
				bool active = (c < inputs[POLY_INPUT].getChannels());
				lights[CHANNEL_LIGHTS + c].setBrightness(active);
			}
		}
	}
};


struct SplitWidget : ModuleWidget {
	SplitWidget(Split *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Split.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.77, 21.347)), module, Split::POLY_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.771, 37.02)), module, Split::MONO_OUTPUTS + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.771, 48.02)), module, Split::MONO_OUTPUTS + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.77, 59.02)), module, Split::MONO_OUTPUTS + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.77, 70.02)), module, Split::MONO_OUTPUTS + 3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.77, 81.02)), module, Split::MONO_OUTPUTS + 4));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.77, 92.02)), module, Split::MONO_OUTPUTS + 5));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.771, 103.02)), module, Split::MONO_OUTPUTS + 6));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.771, 114.02)), module, Split::MONO_OUTPUTS + 7));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.275, 37.02)), module, Split::MONO_OUTPUTS + 8));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.275, 48.02)), module, Split::MONO_OUTPUTS + 9));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.274, 59.02)), module, Split::MONO_OUTPUTS + 10));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.274, 70.02)), module, Split::MONO_OUTPUTS + 11));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.274, 81.02)), module, Split::MONO_OUTPUTS + 12));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.274, 92.02)), module, Split::MONO_OUTPUTS + 13));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.275, 103.02)), module, Split::MONO_OUTPUTS + 14));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.275, 114.02)), module, Split::MONO_OUTPUTS + 15));

		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(15.276, 17.775)), module, Split::CHANNEL_LIGHTS + 0));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(17.275, 17.775)), module, Split::CHANNEL_LIGHTS + 1));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(19.275, 17.775)), module, Split::CHANNEL_LIGHTS + 2));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(21.275, 17.775)), module, Split::CHANNEL_LIGHTS + 3));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(15.276, 19.775)), module, Split::CHANNEL_LIGHTS + 4));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(17.275, 19.775)), module, Split::CHANNEL_LIGHTS + 5));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(19.275, 19.775)), module, Split::CHANNEL_LIGHTS + 6));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(21.275, 19.775)), module, Split::CHANNEL_LIGHTS + 7));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(15.276, 21.775)), module, Split::CHANNEL_LIGHTS + 8));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(17.275, 21.775)), module, Split::CHANNEL_LIGHTS + 9));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(19.275, 21.775)), module, Split::CHANNEL_LIGHTS + 10));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(21.276, 21.775)), module, Split::CHANNEL_LIGHTS + 11));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(15.276, 23.775)), module, Split::CHANNEL_LIGHTS + 12));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(17.275, 23.775)), module, Split::CHANNEL_LIGHTS + 13));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(19.275, 23.775)), module, Split::CHANNEL_LIGHTS + 14));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(21.276, 23.775)), module, Split::CHANNEL_LIGHTS + 15));
	}
};


Model *modelSplit = createModel<Split, SplitWidget>("Split");
