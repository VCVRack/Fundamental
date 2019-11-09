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
			configParam(MUTE_PARAMS + i, 0.0, 1.0, 0.0, string::f("Row %d mute", i + 1));
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
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Mutes.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 18.165)), module, Mutes::MUTE_PARAMS + 0));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 28.164)), module, Mutes::MUTE_PARAMS + 1));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 38.164)), module, Mutes::MUTE_PARAMS + 2));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 48.165)), module, Mutes::MUTE_PARAMS + 3));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 58.164)), module, Mutes::MUTE_PARAMS + 4));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 68.165)), module, Mutes::MUTE_PARAMS + 5));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 78.164)), module, Mutes::MUTE_PARAMS + 6));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 88.164)), module, Mutes::MUTE_PARAMS + 7));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 98.165)), module, Mutes::MUTE_PARAMS + 8));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 108.166)), module, Mutes::MUTE_PARAMS + 9));

		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 17.81)), module, Mutes::IN_INPUTS + 0));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 27.809)), module, Mutes::IN_INPUTS + 1));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 37.809)), module, Mutes::IN_INPUTS + 2));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 47.81)), module, Mutes::IN_INPUTS + 3));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 57.81)), module, Mutes::IN_INPUTS + 4));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 67.809)), module, Mutes::IN_INPUTS + 5));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 77.81)), module, Mutes::IN_INPUTS + 6));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 87.81)), module, Mutes::IN_INPUTS + 7));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 97.809)), module, Mutes::IN_INPUTS + 8));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 107.809)), module, Mutes::IN_INPUTS + 9));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 17.81)), module, Mutes::OUT_OUTPUTS + 0));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 27.809)), module, Mutes::OUT_OUTPUTS + 1));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 37.809)), module, Mutes::OUT_OUTPUTS + 2));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 47.81)), module, Mutes::OUT_OUTPUTS + 3));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 57.809)), module, Mutes::OUT_OUTPUTS + 4));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 67.809)), module, Mutes::OUT_OUTPUTS + 5));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 77.81)), module, Mutes::OUT_OUTPUTS + 6));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 87.81)), module, Mutes::OUT_OUTPUTS + 7));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 97.809)), module, Mutes::OUT_OUTPUTS + 8));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 107.809)), module, Mutes::OUT_OUTPUTS + 9));

		addChild(createLight<LEDBezelLight<GreenLight>>(mm2px(Vec(17.32, 18.915)), module, Mutes::MUTE_LIGHTS + 0));
		addChild(createLight<LEDBezelLight<GreenLight>>(mm2px(Vec(17.32, 28.916)), module, Mutes::MUTE_LIGHTS + 1));
		addChild(createLight<LEDBezelLight<GreenLight>>(mm2px(Vec(17.32, 38.915)), module, Mutes::MUTE_LIGHTS + 2));
		addChild(createLight<LEDBezelLight<GreenLight>>(mm2px(Vec(17.32, 48.915)), module, Mutes::MUTE_LIGHTS + 3));
		addChild(createLight<LEDBezelLight<GreenLight>>(mm2px(Vec(17.32, 58.916)), module, Mutes::MUTE_LIGHTS + 4));
		addChild(createLight<LEDBezelLight<GreenLight>>(mm2px(Vec(17.32, 68.916)), module, Mutes::MUTE_LIGHTS + 5));
		addChild(createLight<LEDBezelLight<GreenLight>>(mm2px(Vec(17.32, 78.915)), module, Mutes::MUTE_LIGHTS + 6));
		addChild(createLight<LEDBezelLight<GreenLight>>(mm2px(Vec(17.32, 88.916)), module, Mutes::MUTE_LIGHTS + 7));
		addChild(createLight<LEDBezelLight<GreenLight>>(mm2px(Vec(17.32, 98.915)), module, Mutes::MUTE_LIGHTS + 8));
		addChild(createLight<LEDBezelLight<GreenLight>>(mm2px(Vec(17.32, 108.915)), module, Mutes::MUTE_LIGHTS + 9));
	}
};


Model* modelMutes = createModel<Mutes, MutesWidget>("Mutes");
