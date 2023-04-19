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

	bool declick = false;

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

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		declick = false;
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		int length = 2 + int(params[STEPS_PARAM].getValue());

		// Determine current index
		if (clockTrigger.process(rescale(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f))) {
			index++;
		}
		if (resetTrigger.process(rescale(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f))) {
			index = 0;
		}
		if (index >= length)
			index = 0;

		// Get number of polyphony channels
		int channels = 1;
		for (int i = 0; i < INPUTS; i++) {
			channels = std::max(channels, inputs[IN_INPUTS + i].getChannels());
		}

		if (INPUTS == 1) {
			// 1 -> many

			for (int i = 0; i < OUTPUTS; i++) {
				outputs[OUT_OUTPUTS + i].setChannels(channels);

				if (declick) {
					// Step click filter
					float gain = clickFilters[i].process(args.sampleTime, i == index);
					if (gain != 0.f) {
						for (int c = 0; c < channels; c += 4) {
							// Get input
							float_4 in = inputs[IN_INPUTS + 0].template getVoltageSimd<float_4>(c);
							// Set output
							outputs[OUT_OUTPUTS + i].setVoltageSimd(in * gain, c);
						}
					}
					else {
						outputs[OUT_OUTPUTS + i].clearVoltages();
					}
				}
				else {
					// Set output
					if (i == index)
						outputs[OUT_OUTPUTS + i].writeVoltages(inputs[IN_INPUTS + 0].getVoltages());
					else
						outputs[OUT_OUTPUTS + i].clearVoltages();
				}
			}
		}
		else {
			// many -> 1

			outputs[OUT_OUTPUTS + 0].setChannels(channels);

			if (declick) {
				// Get mixed output
				float_4 out[4] = {};
				for (int i = 0; i < INPUTS; i++) {
					float gain = clickFilters[i].process(args.sampleTime, i == index);
					if (gain != 0.f) {
						for (int c = 0; c < channels; c += 4) {
							float_4 in = inputs[IN_INPUTS + i].template getPolyVoltageSimd<float_4>(c);
							out[c / 4] += in * gain;
						}
					}
				}

				// Set output
				for (int c = 0; c < channels; c += 4) {
					outputs[OUT_OUTPUTS + 0].setVoltageSimd(out[c / 4], c);
				}
			}
			else {
				// Get and set output
				outputs[OUT_OUTPUTS + 0].writeVoltages(inputs[IN_INPUTS + index].getVoltages());
			}
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
			string::Version version(json_string_value(versionJ));
			if (version < string::Version("2")) {
				params[STEPS_PARAM].setValue(2 - params[STEPS_PARAM].getValue());
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "declick", json_boolean(declick));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* declickJ = json_object_get(rootJ, "declick");
		if (declickJ)
			declick = json_boolean_value(declickJ);
		// In <2.5.0, SequentialSwitch always de-clicked.
		else
			declick = true;
	}
};


template <int INPUTS, int OUTPUTS>
struct SequentialSwitchWidget : ModuleWidget {
	typedef SequentialSwitch<INPUTS, OUTPUTS> TSequentialSwitch;

	SequentialSwitchWidget(TSequentialSwitch* module) {
		setModule(module);

		if (INPUTS == 1 && OUTPUTS == 4) {
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

		if (INPUTS == 4 && OUTPUTS == 1) {
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
	}

	void appendContextMenu(Menu* menu) override {
		TSequentialSwitch* module = getModule<TSequentialSwitch>();

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolPtrMenuItem("De-click", "", &module->declick));
	}
};


Model* modelSequentialSwitch1 = createModel<SequentialSwitch<1, 4>, SequentialSwitchWidget<1, 4>>("SequentialSwitch1");
Model* modelSequentialSwitch2 = createModel<SequentialSwitch<4, 1>, SequentialSwitchWidget<4, 1>>("SequentialSwitch2");
