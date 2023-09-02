#include "plugin.hpp"


using simd::float_4;


struct RandomValues : Module {
	enum ParamId {
		PUSH_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		TRIG_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(RND_OUTPUTS, 7),
		OUTPUTS_LEN
	};
	enum LightId {
		PUSH_LIGHT,
		LIGHTS_LEN
	};

	dsp::BooleanTrigger pushTrigger;
	dsp::TSchmittTrigger<float_4> trigTriggers[4];

	struct Range {
		float gain;
		float offset;

		bool operator==(const Range& other) const {
			return gain == other.gain && offset == other.offset;
		}
	};
	Range range = {10.f, 0.f};

	RandomValues() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(PUSH_PARAM, "Push");
		configInput(TRIG_INPUT, "Trigger");
		for (int i = 0; i < 7; i++)
			configOutput(RND_OUTPUTS + i, string::f("Random %d", i + 1));
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		range = {10.f, 0.f};
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(1, inputs[TRIG_INPUT].getChannels());

		bool pushed = pushTrigger.process(params[PUSH_PARAM].getValue());
		bool light = false;

		for (int c = 0; c < channels; c += 4) {
			float_4 triggered = trigTriggers[c / 4].process(inputs[TRIG_INPUT].getVoltageSimd<float_4>(c), 0.1f, 1.f);
			int triggeredMask = simd::movemask(triggered);

			// This branch is infrequent so we don't need to use SIMD.
			if (pushed || triggeredMask) {
				light = true;

				for (int c2 = 0; c2 < std::min(4, channels - c); c2++) {
					if (pushed || (triggeredMask & (1 << c2))) {
						for (int i = 0; i < 7; i++) {
							float r = random::get<float>();
							r = r * range.gain + range.offset;
							outputs[RND_OUTPUTS + i].setVoltage(r, c + c2);
						}
					}
				}
			}
		}

		for (int i = 0; i < 7; i++) {
			outputs[RND_OUTPUTS + i].setChannels(channels);
		}
		lights[PUSH_LIGHT].setBrightnessSmooth(light, args.sampleTime);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "gain", json_real(range.gain));
		json_object_set_new(rootJ, "offset", json_real(range.offset));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* gainJ = json_object_get(rootJ, "gain");
		if (gainJ)
			range.gain = json_number_value(gainJ);

		json_t* offsetJ = json_object_get(rootJ, "offset");
		if (offsetJ)
			range.offset = json_number_value(offsetJ);
	}
};


struct RandomValuesWidget : ModuleWidget {
	RandomValuesWidget(RandomValues* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/RandomValues.svg"), asset::plugin(pluginInstance, "res/RandomValues-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createLightParamCentered<LEDLightBezel<>>(mm2px(Vec(7.62, 21.968)), module, RandomValues::PUSH_PARAM, RandomValues::PUSH_LIGHT));

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(7.622, 38.225)), module, RandomValues::TRIG_INPUT));

		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(7.622, 52.35)), module, RandomValues::RND_OUTPUTS + 0));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(7.622, 62.477)), module, RandomValues::RND_OUTPUTS + 1));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(7.622, 72.605)), module, RandomValues::RND_OUTPUTS + 2));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(7.622, 82.732)), module, RandomValues::RND_OUTPUTS + 3));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(7.622, 92.86)), module, RandomValues::RND_OUTPUTS + 4));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(7.622, 102.987)), module, RandomValues::RND_OUTPUTS + 5));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(7.622, 113.013)), module, RandomValues::RND_OUTPUTS + 6));
	}

	void appendContextMenu(Menu* menu) override {
		RandomValues* module = getModule<RandomValues>();

		menu->addChild(new MenuSeparator);

		static const std::vector<RandomValues::Range> ranges = {
			{10.f, 0.f},
			{5.f, 0.f},
			{1.f, 0.f},
			{20.f, -10.f},
			{10.f, -5.f},
			{2.f, -1.f},
		};
		static const std::vector<std::string> labels = {
			"0V to 10V",
			"0V to 5V",
			"0V to 1V",
			"-10V to 10V",
			"-5V to 5V",
			"-1V to 1V",
		};
		menu->addChild(createIndexSubmenuItem("Voltage range", labels,
			[=]() {
				auto it = std::find(ranges.begin(), ranges.end(), module->range);
				return std::distance(ranges.begin(), it);
			},
			[=](int i) {
				module->range = ranges[i];
			}
		));
	}
};


Model* modelRandomValues = createModel<RandomValues, RandomValuesWidget>("RandomValues");