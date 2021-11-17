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
		ENUMS(CHANNEL_LIGHTS, 4 * 2),
		NUM_LIGHTS
	};

	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger resetTrigger;
	int index = 0;
	dsp::ClockDivider lightDivider;
	dsp::SlewLimiter clickFilters[4];

	SequentialSwitch() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(STEPS_PARAM, 0.0, 2.0, 2.0, "Steps", {"2", "3", "4"});
		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		if (INPUTS == 1) {
			configInput(IN_INPUTS + 0, "Main");
		}
		else {
			for (int i = 0; i < INPUTS; i++)
				configInput(IN_INPUTS + i, string::f("Channel %d", i + 1));
		}
		if (OUTPUTS == 1) {
			configOutput(OUT_OUTPUTS + 0, "Main");
		}
		else {
			for (int i = 0; i < OUTPUTS; i++)
				configOutput(OUT_OUTPUTS + i, string::f("Channel %d", i + 1));
		}

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
		int length = 2 + (int) std::round(params[STEPS_PARAM].getValue());
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
				lights[CHANNEL_LIGHTS + 2 * i + 0].setBrightness(index == i);
				lights[CHANNEL_LIGHTS + 2 * i + 1].setBrightness(i >= length);
			}
		}
	}

	void fromJson(json_t* rootJ) override {
		Module::fromJson(rootJ);

		// If version <2.0 we should transform STEPS_PARAM
		json_t* versionJ = json_object_get(rootJ, "version");
		if (versionJ) {
			std::string version = json_string_value(versionJ);
			if (string::startsWith(version, "0.") || string::startsWith(version, "1.")) {
				DEBUG("steps %f", params[STEPS_PARAM].getValue());
				params[STEPS_PARAM].setValue(2 - params[STEPS_PARAM].getValue());
			}
		}
	}
};


struct SequentialSwitch1Widget : ModuleWidget {
	typedef SequentialSwitch<1, 4> TSequentialSwitch;

	SequentialSwitch1Widget(TSequentialSwitch* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SequentialSwitch1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<CKSSThreeHorizontal>(mm2px(Vec(7.555, 20.942)), module, TSequentialSwitch::STEPS_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.555, 33.831)), module, TSequentialSwitch::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.555, 50.126)), module, TSequentialSwitch::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.555, 66.379)), module, TSequentialSwitch::IN_INPUTS + 0));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.555, 82.607)), module, TSequentialSwitch::OUT_OUTPUTS + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.555, 92.767)), module, TSequentialSwitch::OUT_OUTPUTS + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.555, 102.927)), module, TSequentialSwitch::OUT_OUTPUTS + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.555, 113.087)), module, TSequentialSwitch::OUT_OUTPUTS + 3));

		addChild(createLightCentered<TinyLight<YellowRedLight<>>>(mm2px(Vec(11.28, 78.863)), module, TSequentialSwitch::CHANNEL_LIGHTS + 2 * 0));
		addChild(createLightCentered<TinyLight<YellowRedLight<>>>(mm2px(Vec(11.28, 89.023)), module, TSequentialSwitch::CHANNEL_LIGHTS + 2 * 1));
		addChild(createLightCentered<TinyLight<YellowRedLight<>>>(mm2px(Vec(11.28, 99.183)), module, TSequentialSwitch::CHANNEL_LIGHTS + 2 * 2));
		addChild(createLightCentered<TinyLight<YellowRedLight<>>>(mm2px(Vec(11.28, 109.343)), module, TSequentialSwitch::CHANNEL_LIGHTS + 2 * 3));
	}
};


Model* modelSequentialSwitch1 = createModel<SequentialSwitch<1, 4>, SequentialSwitch1Widget>("SequentialSwitch1");


struct SequentialSwitch2Widget : ModuleWidget {
	typedef SequentialSwitch<4, 1> TSequentialSwitch;

	SequentialSwitch2Widget(TSequentialSwitch* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SequentialSwitch2.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<CKSSThreeHorizontal>(mm2px(Vec(7.8, 20.942)), module, TSequentialSwitch::STEPS_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.8, 33.831)), module, TSequentialSwitch::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.8, 50.126)), module, TSequentialSwitch::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.8, 66.379)), module, TSequentialSwitch::IN_INPUTS + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.8, 76.539)), module, TSequentialSwitch::IN_INPUTS + 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.8, 86.699)), module, TSequentialSwitch::IN_INPUTS + 2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.8, 96.859)), module, TSequentialSwitch::IN_INPUTS + 3));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.8, 113.115)), module, TSequentialSwitch::OUT_OUTPUTS + 0));

		addChild(createLightCentered<TinyLight<YellowRedLight<>>>(mm2px(Vec(11.526, 63.259)), module, TSequentialSwitch::CHANNEL_LIGHTS + 2 * 0));
		addChild(createLightCentered<TinyLight<YellowRedLight<>>>(mm2px(Vec(11.526, 72.795)), module, TSequentialSwitch::CHANNEL_LIGHTS + 2 * 1));
		addChild(createLightCentered<TinyLight<YellowRedLight<>>>(mm2px(Vec(11.526, 82.955)), module, TSequentialSwitch::CHANNEL_LIGHTS + 2 * 2));
		addChild(createLightCentered<TinyLight<YellowRedLight<>>>(mm2px(Vec(11.526, 93.115)), module, TSequentialSwitch::CHANNEL_LIGHTS + 2 * 3));
	}
};


Model* modelSequentialSwitch2 = createModel<SequentialSwitch<4, 1>, SequentialSwitch2Widget>("SequentialSwitch2");
