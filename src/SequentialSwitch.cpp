#include "Fundamental.hpp"


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

	SchmittTrigger clockTrigger;
	SchmittTrigger resetTrigger;
	int channel = 0;
	SlewLimiter channelFilter[4];

	SequentialSwitch() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		for (int i = 0; i < 4; i++) {
			channelFilter[i].rise = 0.01f;
			channelFilter[i].fall = 0.01f;
		}
	}

	void step() override {
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
			channelFilter[i].process(channel == i ? 1.0f : 0.0f);
		}

		// Set outputs
		if (TYPE == 1) {
			float out = inputs[IN_INPUT + 0].value;
			for (int i = 0; i < 4; i++) {
				outputs[OUT_OUTPUT + i].value = channelFilter[i].out * out;
			}
		}
		else {
			float out = 0.0f;
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
	SequentialSwitch1Widget(SequentialSwitch<1> *module);
};

SequentialSwitch1Widget::SequentialSwitch1Widget(SequentialSwitch<1> *module) : ModuleWidget(module) {
	typedef SequentialSwitch<1> TSequentialSwitch;
	setPanel(SVG::load(assetPlugin(plugin, "res/SequentialSwitch1.svg")));

	addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	addParam(createParam<CKSSThree>(mm2px(Vec(5.24619, 46.9153)), module, TSequentialSwitch::CHANNELS_PARAM, 0.0f, 2.0f, 0.0f));

	addInput(createPort<PJ301MPort>(mm2px(Vec(3.51398, 17.694)), PortWidget::INPUT, module, TSequentialSwitch::CLOCK_INPUT));
	addInput(createPort<PJ301MPort>(mm2px(Vec(3.51398, 32.1896)), PortWidget::INPUT, module, TSequentialSwitch::RESET_INPUT));
	addInput(createPort<PJ301MPort>(mm2px(Vec(3.51536, 62.8096)), PortWidget::INPUT, module, TSequentialSwitch::IN_INPUT + 0));

	addOutput(createPort<PJ301MPort>(mm2px(Vec(3.51536, 77.8095)), PortWidget::OUTPUT, module, TSequentialSwitch::OUT_OUTPUT + 0));
	addOutput(createPort<PJ301MPort>(mm2px(Vec(3.51398, 87.8113)), PortWidget::OUTPUT, module, TSequentialSwitch::OUT_OUTPUT + 1));
	addOutput(createPort<PJ301MPort>(mm2px(Vec(3.51398, 97.809)), PortWidget::OUTPUT, module, TSequentialSwitch::OUT_OUTPUT + 2));
	addOutput(createPort<PJ301MPort>(mm2px(Vec(3.51398, 107.809)), PortWidget::OUTPUT, module, TSequentialSwitch::OUT_OUTPUT + 3));

	addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.8203, 77.7158)), module, TSequentialSwitch::CHANNEL_LIGHT + 0));
	addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.8203, 87.7163)), module, TSequentialSwitch::CHANNEL_LIGHT + 1));
	addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.8203, 97.7167)), module, TSequentialSwitch::CHANNEL_LIGHT + 2));
	addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.8203, 107.716)), module, TSequentialSwitch::CHANNEL_LIGHT + 3));
}


Model *modelSequentialSwitch1 = createModel<SequentialSwitch<1>, SequentialSwitch1Widget>("SequentialSwitch1");


struct SequentialSwitch2Widget : ModuleWidget {
	SequentialSwitch2Widget(SequentialSwitch<2> *module);
};

SequentialSwitch2Widget::SequentialSwitch2Widget(SequentialSwitch<2> *module) : ModuleWidget(module) {
	typedef SequentialSwitch<2> TSequentialSwitch;
	setPanel(SVG::load(assetPlugin(plugin, "res/SequentialSwitch2.svg")));

	addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	addParam(createParam<CKSSThree>(mm2px(Vec(5.24619, 46.9153)), module, TSequentialSwitch::CHANNELS_PARAM, 0.0f, 2.0f, 0.0f));

	addInput(createPort<PJ301MPort>(mm2px(Vec(3.51398, 17.694)), PortWidget::INPUT, module, TSequentialSwitch::CLOCK_INPUT));
	addInput(createPort<PJ301MPort>(mm2px(Vec(3.51398, 32.191)), PortWidget::INPUT, module, TSequentialSwitch::RESET_INPUT));
	addInput(createPort<PJ301MPort>(mm2px(Vec(3.51398, 62.811)), PortWidget::INPUT, module, TSequentialSwitch::IN_INPUT + 0));
	addInput(createPort<PJ301MPort>(mm2px(Vec(3.51398, 72.8114)), PortWidget::INPUT, module, TSequentialSwitch::IN_INPUT + 1));
	addInput(createPort<PJ301MPort>(mm2px(Vec(3.51398, 82.8091)), PortWidget::INPUT, module, TSequentialSwitch::IN_INPUT + 2));
	addInput(createPort<PJ301MPort>(mm2px(Vec(3.51398, 92.8109)), PortWidget::INPUT, module, TSequentialSwitch::IN_INPUT + 3));

	addOutput(createPort<PJ301MPort>(mm2px(Vec(3.51398, 107.622)), PortWidget::OUTPUT, module, TSequentialSwitch::OUT_OUTPUT + 0));

	addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.7321, 62.6277)), module, TSequentialSwitch::CHANNEL_LIGHT + 0));
	addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.7321, 72.6281)), module, TSequentialSwitch::CHANNEL_LIGHT + 1));
	addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.7321, 82.6285)), module, TSequentialSwitch::CHANNEL_LIGHT + 2));
	addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.7321, 92.6276)), module, TSequentialSwitch::CHANNEL_LIGHT + 3));
}


Model *modelSequentialSwitch2 = createModel<SequentialSwitch<2>, SequentialSwitch2Widget>("SequentialSwitch2");
