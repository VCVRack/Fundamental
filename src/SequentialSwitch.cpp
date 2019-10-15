#include "plugin.hpp"


// Only valid for <1, 4> and <4, 1>
template <int INPUTS, int OUTPUTS>
struct SequentialSwitch : Module {
	enum ParamIds {
		STEPS_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		ENUMS(IN_INPUTS, INPUTS),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUT_OUTPUTS, OUTPUTS),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(CHANNEL_LIGHT, 4),
		NUM_LIGHTS
	};

	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger resetTrigger;
	int index = 0;
	dsp::ClockDivider lightDivider;
	dsp::SlewLimiter clickFilters[4];

	SequentialSwitch() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(STEPS_PARAM, 0.0, 2.0, 0.0, "Steps", "", 0, -1, 4);

		for (int i = 0; i < 4; i++) {
			clickFilters[i].rise = 400.f; // Hz
			clickFilters[i].fall = 400.f; // Hz
		}
		lightDivider.setDivision(512);
	}

	void process(const ProcessArgs& args) override {
		// Determine current index
		if (clockTrigger.process(rescale(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f))) {
			index++;
		}
		if (resetTrigger.process(rescale(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f))) {
			index = 0;
		}
		int length = 4 - (int) std::round(params[STEPS_PARAM].getValue());
		if (index >= length)
			index = 0;

		// Use first input to get number of channels
		int channels = std::max(inputs[IN_INPUTS + 0].getChannels(), 1);

		if (INPUTS == 1) {
			// <1, 4>
			// Get input
			float* in = inputs[IN_INPUTS + 0].getVoltages();

			// Set output
			for (int i = 0; i < OUTPUTS; i++) {
				float gain = clickFilters[i].process(args.sampleTime, index == i);
				outputs[OUT_OUTPUTS + i].setChannels(channels);
				if (gain != 0.f) {
					for (int c = 0; c < channels; c++) {
						float out = in[c] * gain;
						outputs[OUT_OUTPUTS + i].setVoltage(out, c);
					}
				}
				else {
					outputs[OUT_OUTPUTS + i].clearVoltages();
				}
			}
		}
		else {
			// <4, 1>
			// Get input
			float out[16] = {};
			for (int i = 0; i < INPUTS; i++) {
				float gain = clickFilters[i].process(args.sampleTime, index == i);
				if (gain != 0.f) {
					for (int c = 0; c < channels; c++) {
						float in = inputs[IN_INPUTS + i].getVoltage(c);
						out[c] += in * gain;
					}
				}
			}

			// Set output
			outputs[OUT_OUTPUTS + 0].setChannels(channels);
			outputs[OUT_OUTPUTS + 0].writeVoltages(out);
		}

		// Set lights
		if (lightDivider.process()) {
			for (int i = 0; i < 4; i++) {
				lights[CHANNEL_LIGHT + i].setBrightness(index == i);
			}
		}
	}
};


struct SequentialSwitch1Widget : ModuleWidget {
	typedef SequentialSwitch<1, 4> TSequentialSwitch;

	SequentialSwitch1Widget(TSequentialSwitch* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SequentialSwitch1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<CKSSThree>(mm2px(Vec(5.24619, 46.9153)), module, TSequentialSwitch::STEPS_PARAM));

		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 17.694)), module, TSequentialSwitch::CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 32.1896)), module, TSequentialSwitch::RESET_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51536, 62.8096)), module, TSequentialSwitch::IN_INPUTS + 0));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.51536, 77.8095)), module, TSequentialSwitch::OUT_OUTPUTS + 0));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.51398, 87.8113)), module, TSequentialSwitch::OUT_OUTPUTS + 1));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.51398, 97.809)), module, TSequentialSwitch::OUT_OUTPUTS + 2));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.51398, 107.809)), module, TSequentialSwitch::OUT_OUTPUTS + 3));

		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.8203, 77.7158)), module, TSequentialSwitch::CHANNEL_LIGHT + 0));
		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.8203, 87.7163)), module, TSequentialSwitch::CHANNEL_LIGHT + 1));
		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.8203, 97.7167)), module, TSequentialSwitch::CHANNEL_LIGHT + 2));
		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.8203, 107.716)), module, TSequentialSwitch::CHANNEL_LIGHT + 3));
	}
};


Model* modelSequentialSwitch1 = createModel<SequentialSwitch<1, 4>, SequentialSwitch1Widget>("SequentialSwitch1");


struct SequentialSwitch2Widget : ModuleWidget {
	typedef SequentialSwitch<4, 1> TSequentialSwitch;

	SequentialSwitch2Widget(TSequentialSwitch* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SequentialSwitch2.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<CKSSThree>(mm2px(Vec(5.24619, 46.9153)), module, TSequentialSwitch::STEPS_PARAM));

		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 17.694)), module, TSequentialSwitch::CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 32.191)), module, TSequentialSwitch::RESET_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 62.811)), module, TSequentialSwitch::IN_INPUTS + 0));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 72.8114)), module, TSequentialSwitch::IN_INPUTS + 1));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 82.8091)), module, TSequentialSwitch::IN_INPUTS + 2));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 92.8109)), module, TSequentialSwitch::IN_INPUTS + 3));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.51398, 107.622)), module, TSequentialSwitch::OUT_OUTPUTS + 0));

		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.7321, 62.6277)), module, TSequentialSwitch::CHANNEL_LIGHT + 0));
		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.7321, 72.6281)), module, TSequentialSwitch::CHANNEL_LIGHT + 1));
		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.7321, 82.6285)), module, TSequentialSwitch::CHANNEL_LIGHT + 2));
		addChild(createLight<TinyLight<GreenLight>>(mm2px(Vec(10.7321, 92.6276)), module, TSequentialSwitch::CHANNEL_LIGHT + 3));
	}
};


Model* modelSequentialSwitch2 = createModel<SequentialSwitch<4, 1>, SequentialSwitch2Widget>("SequentialSwitch2");
