#include "Fundamental.hpp"


#define NUM_CHANNELS 10


struct Mutes : Module {
	enum ParamIds {
		MUTE_PARAM,
		NUM_PARAMS = MUTE_PARAM + NUM_CHANNELS
	};
	enum InputIds {
		IN_INPUT,
		NUM_INPUTS = IN_INPUT + NUM_CHANNELS
	};
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS = OUT_OUTPUT + NUM_CHANNELS
	};
	enum LightIds {
		MUTE_LIGHT,
		NUM_LIGHTS = MUTE_LIGHT + NUM_CHANNELS
	};

	bool state[NUM_CHANNELS];
	dsp::SchmittTrigger muteTrigger[NUM_CHANNELS];

	Mutes() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i = 0; i < NUM_CHANNELS; i++) {
			params[MUTE_PARAM + i].config(0.0, 1.0, 0.0, string::f("Ch %d mute", i+1));
		}

		onReset();
	}

	void step() override {
		float out = 0.f;
		for (int i = 0; i < NUM_CHANNELS; i++) {
			if (muteTrigger[i].process(params[MUTE_PARAM + i].value))
				state[i] ^= true;
			if (inputs[IN_INPUT + i].active)
				out = inputs[IN_INPUT + i].value;
			outputs[OUT_OUTPUT + i].value = state[i] ? out : 0.f;
			lights[MUTE_LIGHT + i].setBrightness(state[i] ? 0.9f : 0.f);
		}
	}

	void onReset() override {
		for (int i = 0; i < NUM_CHANNELS; i++) {
			state[i] = true;
		}
	}
	void onRandomize() override {
		for (int i = 0; i < NUM_CHANNELS; i++) {
			state[i] = (random::uniform() < 0.5f);
		}
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		// states
		json_t *statesJ = json_array();
		for (int i = 0; i < NUM_CHANNELS; i++) {
			json_t *stateJ = json_boolean(state[i]);
			json_array_append_new(statesJ, stateJ);
		}
		json_object_set_new(rootJ, "states", statesJ);
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		// states
		json_t *statesJ = json_object_get(rootJ, "states");
		if (statesJ) {
			for (int i = 0; i < NUM_CHANNELS; i++) {
				json_t *stateJ = json_array_get(statesJ, i);
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
	MutesWidget(Mutes *module) {
		setModule(module);
		setPanel(SVG::load(asset::plugin(pluginInstance, "res/Mutes.svg")));

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


Model *modelMutes = createModel<Mutes, MutesWidget>("Mutes");
