#include "plugin.hpp"


struct Mutes : Module {
	enum ParamIds {
		ENUMS(MUTE_PARAM, 10),
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(IN_INPUT, 10),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUT_OUTPUT, 10),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(MUTE_LIGHT, 10),
		NUM_LIGHTS
	};

	bool state[10];
	dsp::BooleanTrigger muteTrigger[10];

	Mutes() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i = 0; i < 10; i++) {
			configParam(MUTE_PARAM + i, 0.0, 1.0, 0.0, string::f("Ch %d mute", i + 1));
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
			if (muteTrigger[i].process(params[MUTE_PARAM + i].getValue() > 0.f))
				state[i] ^= true;

			// Get input
			// Inputs are normalized to the input above it, so only set if connected
			if (inputs[IN_INPUT + i].isConnected()) {
				channels = inputs[IN_INPUT + i].getChannels();
				inputs[IN_INPUT + i].readVoltages(out);
			}

			// Set output
			if (outputs[OUT_OUTPUT + i].isConnected()) {
				outputs[OUT_OUTPUT + i].setChannels(channels);
				outputs[OUT_OUTPUT + i].writeVoltages(state[i] ? out : zero);
			}

			// Set light
			lights[MUTE_LIGHT + i].setBrightness(state[i] ? 0.9f : 0.f);
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


template <typename BASE>
struct MuteLight : BASE {
	MuteLight() {
		this->box.size = mm2px(Vec(6.f, 6.f));
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

		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 18.165)), module, Mutes::MUTE_PARAM + 0));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 28.164)), module, Mutes::MUTE_PARAM + 1));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 38.164)), module, Mutes::MUTE_PARAM + 2));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 48.165)), module, Mutes::MUTE_PARAM + 3));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 58.164)), module, Mutes::MUTE_PARAM + 4));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 68.165)), module, Mutes::MUTE_PARAM + 5));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 78.164)), module, Mutes::MUTE_PARAM + 6));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 88.164)), module, Mutes::MUTE_PARAM + 7));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 98.165)), module, Mutes::MUTE_PARAM + 8));
		addParam(createParam<LEDBezel>(mm2px(Vec(16.57, 108.166)), module, Mutes::MUTE_PARAM + 9));

		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 17.81)), module, Mutes::IN_INPUT + 0));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 27.809)), module, Mutes::IN_INPUT + 1));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 37.809)), module, Mutes::IN_INPUT + 2));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 47.81)), module, Mutes::IN_INPUT + 3));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 57.81)), module, Mutes::IN_INPUT + 4));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 67.809)), module, Mutes::IN_INPUT + 5));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 77.81)), module, Mutes::IN_INPUT + 6));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 87.81)), module, Mutes::IN_INPUT + 7));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 97.809)), module, Mutes::IN_INPUT + 8));
		addInput(createInput<PJ301MPort>(mm2px(Vec(4.214, 107.809)), module, Mutes::IN_INPUT + 9));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 17.81)), module, Mutes::OUT_OUTPUT + 0));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 27.809)), module, Mutes::OUT_OUTPUT + 1));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 37.809)), module, Mutes::OUT_OUTPUT + 2));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 47.81)), module, Mutes::OUT_OUTPUT + 3));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 57.809)), module, Mutes::OUT_OUTPUT + 4));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 67.809)), module, Mutes::OUT_OUTPUT + 5));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 77.81)), module, Mutes::OUT_OUTPUT + 6));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 87.81)), module, Mutes::OUT_OUTPUT + 7));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 97.809)), module, Mutes::OUT_OUTPUT + 8));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(28.214, 107.809)), module, Mutes::OUT_OUTPUT + 9));

		addChild(createLight<MuteLight<GreenLight>>(mm2px(Vec(17.32, 18.915)), module, Mutes::MUTE_LIGHT + 0));
		addChild(createLight<MuteLight<GreenLight>>(mm2px(Vec(17.32, 28.916)), module, Mutes::MUTE_LIGHT + 1));
		addChild(createLight<MuteLight<GreenLight>>(mm2px(Vec(17.32, 38.915)), module, Mutes::MUTE_LIGHT + 2));
		addChild(createLight<MuteLight<GreenLight>>(mm2px(Vec(17.32, 48.915)), module, Mutes::MUTE_LIGHT + 3));
		addChild(createLight<MuteLight<GreenLight>>(mm2px(Vec(17.32, 58.916)), module, Mutes::MUTE_LIGHT + 4));
		addChild(createLight<MuteLight<GreenLight>>(mm2px(Vec(17.32, 68.916)), module, Mutes::MUTE_LIGHT + 5));
		addChild(createLight<MuteLight<GreenLight>>(mm2px(Vec(17.32, 78.915)), module, Mutes::MUTE_LIGHT + 6));
		addChild(createLight<MuteLight<GreenLight>>(mm2px(Vec(17.32, 88.916)), module, Mutes::MUTE_LIGHT + 7));
		addChild(createLight<MuteLight<GreenLight>>(mm2px(Vec(17.32, 98.915)), module, Mutes::MUTE_LIGHT + 8));
		addChild(createLight<MuteLight<GreenLight>>(mm2px(Vec(17.32, 108.915)), module, Mutes::MUTE_LIGHT + 9));
	}
};


Model* modelMutes = createModel<Mutes, MutesWidget>("Mutes");
