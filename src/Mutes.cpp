#include "plugin.hpp"


struct Mutes : Module {
	enum ParamIds {
		ENUMS(MUTE_PARAMS, 10),
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(IN_INPUTS, 10),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUT_OUTPUTS, 10),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(MUTE_LIGHTS, 10),
		NUM_LIGHTS
	};

	bool state[10];
	dsp::BooleanTrigger muteTrigger[10];

	Mutes() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i = 0; i < 10; i++) {
			configButton(MUTE_PARAMS + i, string::f("Row %d mute", i + 1));
			configInput(IN_INPUTS + i, string::f("Row %d", i + 1));
			configOutput(OUT_OUTPUTS + i, string::f("Row %d", i + 1));
		}

		onReset();
	}

	void process(const ProcessArgs& args) override {
		const float zero[16] = {};
		float out[16] = {};
		int channels = 1;

		// Iterate rows
		for (int i = 0; i < 10; i++) {
			// Process trigger
			if (muteTrigger[i].process(params[MUTE_PARAMS + i].getValue() > 0.f))
				state[i] ^= true;

			// Get input
			// Inputs are normalized to the input above it, so only set if connected
			if (inputs[IN_INPUTS + i].isConnected()) {
				channels = inputs[IN_INPUTS + i].getChannels();
				inputs[IN_INPUTS + i].readVoltages(out);
			}

			// Set output
			if (outputs[OUT_OUTPUTS + i].isConnected()) {
				outputs[OUT_OUTPUTS + i].setChannels(channels);
				outputs[OUT_OUTPUTS + i].writeVoltages(state[i] ? out : zero);
			}

			// Set light
			lights[MUTE_LIGHTS + i].setBrightness(state[i] ? 0.9f : 0.f);
		}
	}

	void onReset() override {
		for (int i = 0; i < 10; i++) {
			state[i] = true;
		}
	}
	void onRandomize() override {
		for (int i = 0; i < 10; i++) {
			state[i] = (random::uniform() < 0.5f);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		// states
		json_t* statesJ = json_array();
		for (int i = 0; i < 10; i++) {
			json_t* stateJ = json_boolean(state[i]);
			json_array_append_new(statesJ, stateJ);
		}
		json_object_set_new(rootJ, "states", statesJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// states
		json_t* statesJ = json_object_get(rootJ, "states");
		if (statesJ) {
			for (int i = 0; i < 10; i++) {
				json_t* stateJ = json_array_get(statesJ, i);
				if (stateJ)
					state[i] = json_boolean_value(stateJ);
			}
		}
	}
};


struct MutesWidget : ModuleWidget {
	MutesWidget(Mutes* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Mutes.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createLightParamCentered<LEDLightBezel<>>(mm2px(Vec(20.312, 21.968)), module, Mutes::MUTE_PARAMS + 0, Mutes::MUTE_LIGHTS + 0));
		addParam(createLightParamCentered<LEDLightBezel<>>(mm2px(Vec(20.312, 32.095)), module, Mutes::MUTE_PARAMS + 1, Mutes::MUTE_LIGHTS + 1));
		addParam(createLightParamCentered<LEDLightBezel<>>(mm2px(Vec(20.312, 42.222)), module, Mutes::MUTE_PARAMS + 2, Mutes::MUTE_LIGHTS + 2));
		addParam(createLightParamCentered<LEDLightBezel<>>(mm2px(Vec(20.312, 52.35)), module, Mutes::MUTE_PARAMS + 3, Mutes::MUTE_LIGHTS + 3));
		addParam(createLightParamCentered<LEDLightBezel<>>(mm2px(Vec(20.312, 62.477)), module, Mutes::MUTE_PARAMS + 4, Mutes::MUTE_LIGHTS + 4));
		addParam(createLightParamCentered<LEDLightBezel<>>(mm2px(Vec(20.312, 72.605)), module, Mutes::MUTE_PARAMS + 5, Mutes::MUTE_LIGHTS + 5));
		addParam(createLightParamCentered<LEDLightBezel<>>(mm2px(Vec(20.312, 82.732)), module, Mutes::MUTE_PARAMS + 6, Mutes::MUTE_LIGHTS + 6));
		addParam(createLightParamCentered<LEDLightBezel<>>(mm2px(Vec(20.312, 92.86)), module, Mutes::MUTE_PARAMS + 7, Mutes::MUTE_LIGHTS + 7));
		addParam(createLightParamCentered<LEDLightBezel<>>(mm2px(Vec(20.312, 102.987)), module, Mutes::MUTE_PARAMS + 8, Mutes::MUTE_LIGHTS + 8));
		addParam(createLightParamCentered<LEDLightBezel<>>(mm2px(Vec(20.312, 113.115)), module, Mutes::MUTE_PARAMS + 9, Mutes::MUTE_LIGHTS + 9));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.291, 21.968)), module, Mutes::IN_INPUTS + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.291, 32.095)), module, Mutes::IN_INPUTS + 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.291, 42.222)), module, Mutes::IN_INPUTS + 2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.291, 52.35)), module, Mutes::IN_INPUTS + 3));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.291, 62.477)), module, Mutes::IN_INPUTS + 4));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.291, 72.605)), module, Mutes::IN_INPUTS + 5));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.291, 82.732)), module, Mutes::IN_INPUTS + 6));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.291, 92.86)), module, Mutes::IN_INPUTS + 7));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.291, 102.987)), module, Mutes::IN_INPUTS + 8));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.291, 113.115)), module, Mutes::IN_INPUTS + 9));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.332, 21.968)), module, Mutes::OUT_OUTPUTS + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.332, 32.095)), module, Mutes::OUT_OUTPUTS + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.332, 42.222)), module, Mutes::OUT_OUTPUTS + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.332, 52.35)), module, Mutes::OUT_OUTPUTS + 3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.332, 62.477)), module, Mutes::OUT_OUTPUTS + 4));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.332, 72.605)), module, Mutes::OUT_OUTPUTS + 5));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.332, 82.732)), module, Mutes::OUT_OUTPUTS + 6));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.332, 92.86)), module, Mutes::OUT_OUTPUTS + 7));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.332, 102.987)), module, Mutes::OUT_OUTPUTS + 8));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.332, 113.115)), module, Mutes::OUT_OUTPUTS + 9));
	}
};


Model* modelMutes = createModel<Mutes, MutesWidget>("Mutes");
