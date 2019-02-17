#include "plugin.hpp"


template <int TYPE>
struct SequentialSwitch : Module {
	enum ParamIds {
		CHANNELS_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		ENUMS(IN_INPUT, TYPE == 1 ? 1 : 4),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUT_OUTPUT, TYPE == 1 ? 4 : 1),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(CHANNEL_LIGHT, 4),
		NUM_LIGHTS
	};

	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger resetTrigger;
	int channel = 0;
	dsp::SlewLimiter channelFilter[4];

	SequentialSwitch() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		params[CHANNELS_PARAM].config(0.0, 2.0, 0.0, "Channels", "", 0.f, 1.f, 2.f);

		for (int i = 0; i < 4; i++) {
			channelFilter[i].rise = 0.01f;
			channelFilter[i].fall = 0.01f;
		}
	}

	void process(const ProcessArgs &args) override {
		// Determine current channel
		if (clockTrigger.process(inputs[CLOCK_INPUT].value / 2.f)) {
			channel++;
		}
		if (resetTrigger.process(inputs[RESET_INPUT].value / 2.f)) {
			channel = 0;
		}
		int channels = 4 - (int) params[CHANNELS_PARAM].value;
		channel %= channels;

		// Filter channels
		for (int i = 0; i < 4; i++) {
			channelFilter[i].process(channel == i ? 1.f : 0.f);
		}

		// Set outputs
		if (TYPE == 1) {
			float out = inputs[IN_INPUT + 0].value;
			for (int i = 0; i < 4; i++) {
				outputs[OUT_OUTPUT + i].value = channelFilter[i].out * out;
			}
		}
		else {
			float out = 0.f;
			for (int i = 0; i < 4; i++) {
				out += channelFilter[i].out * inputs[IN_INPUT + i].value;
			}
			outputs[OUT_OUTPUT + 0].value = out;
		}

		// Set lights
		for (int i = 0; i < 4; i++) {
			lights[CHANNEL_LIGHT + i].setBrightness(channelFilter[i].out);
		}
	}
};


struct SequentialSwitch1Widget : ModuleWidget {
	typedef SequentialSwitch<1> TSequentialSwitch;

	SequentialSwitch1Widget(SequentialSwitch<1> *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SequentialSwitch1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<CKSSThree>(mm2px(Vec(5.24619, 46.9153)), module, TSequentialSwitch::CHANNELS_PARAM));

		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 17.694)), module, TSequentialSwitch::CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 32.1896)), module, TSequentialSwitch::RESET_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51536, 62.8096)), module, TSequentialSwitch::IN_INPUT + 0));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.51536, 77.8095)), module, TSequentialSwitch::OUT_OUTPUT + 0));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.51398, 87.8113)), module, TSequentialSwitch::OUT_OUTPUT + 1));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.51398, 97.809)), module, TSequentialSwitch::OUT_OUTPUT + 2));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.51398, 107.809)), module, TSequentialSwitch::OUT_OUTPUT + 3));

		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.8203, 77.7158)), module, TSequentialSwitch::CHANNEL_LIGHT + 0));
		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.8203, 87.7163)), module, TSequentialSwitch::CHANNEL_LIGHT + 1));
		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.8203, 97.7167)), module, TSequentialSwitch::CHANNEL_LIGHT + 2));
		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.8203, 107.716)), module, TSequentialSwitch::CHANNEL_LIGHT + 3));
	}
};


Model *modelSequentialSwitch1 = createModel<SequentialSwitch<1>, SequentialSwitch1Widget>("SequentialSwitch1");


struct SequentialSwitch2Widget : ModuleWidget {
	typedef SequentialSwitch<2> TSequentialSwitch;

	SequentialSwitch2Widget(SequentialSwitch<2> *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SequentialSwitch2.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<CKSSThree>(mm2px(Vec(5.24619, 46.9153)), module, TSequentialSwitch::CHANNELS_PARAM));

		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 17.694)), module, TSequentialSwitch::CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 32.191)), module, TSequentialSwitch::RESET_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 62.811)), module, TSequentialSwitch::IN_INPUT + 0));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 72.8114)), module, TSequentialSwitch::IN_INPUT + 1));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 82.8091)), module, TSequentialSwitch::IN_INPUT + 2));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 92.8109)), module, TSequentialSwitch::IN_INPUT + 3));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.51398, 107.622)), module, TSequentialSwitch::OUT_OUTPUT + 0));

		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.7321, 62.6277)), module, TSequentialSwitch::CHANNEL_LIGHT + 0));
		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.7321, 72.6281)), module, TSequentialSwitch::CHANNEL_LIGHT + 1));
		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.7321, 82.6285)), module, TSequentialSwitch::CHANNEL_LIGHT + 2));
		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.7321, 92.6276)), module, TSequentialSwitch::CHANNEL_LIGHT + 3));
	}
};


Model *modelSequentialSwitch2 = createModel<SequentialSwitch<2>, SequentialSwitch2Widget>("SequentialSwitch2");
