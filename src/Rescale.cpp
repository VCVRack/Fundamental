#include "plugin.hpp"


struct Rescale : Module {
	enum ParamId {
		GAIN_PARAM,
		OFFSET_PARAM,
		MAX_PARAM,
		MIN_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		IN_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	float multiplier = 1.f;
	bool reflectMin = false;
	bool reflectMax = false;

	Rescale() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		struct GainQuantity : ParamQuantity {
			float getDisplayValue() override {
				Rescale* module = reinterpret_cast<Rescale*>(this->module);
				if (module->multiplier == 1.f) {
					unit = "%";
					displayMultiplier = 100.f;
				}
				else {
					unit = "x";
					displayMultiplier = module->multiplier;
				}
				return ParamQuantity::getDisplayValue();
			}
		};
		configParam<GainQuantity>(GAIN_PARAM, -1.f, 1.f, 0.f, "Gain", "%", 0, 100);
		configParam(OFFSET_PARAM, -10.f, 10.f, 0.f, "Offset", " V");
		configParam(MAX_PARAM, -10.f, 10.f, 10.f, "Maximum", " V");
		configParam(MIN_PARAM, -10.f, 10.f, -10.f, "Minimum", " V");
		configInput(IN_INPUT, "Signal");
		configOutput(OUT_OUTPUT, "Signal");
		configBypass(IN_INPUT, OUT_OUTPUT);
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		int channels = std::max(1, inputs[IN_INPUT].getChannels());

		float gain = params[GAIN_PARAM].getValue() * multiplier;
		float offset = params[OFFSET_PARAM].getValue();
		float min = params[MIN_PARAM].getValue();
		float max = params[MAX_PARAM].getValue();

		for (int c = 0; c < channels; c += 4) {
			float_4 x = inputs[IN_INPUT].getPolyVoltageSimd<float_4>(c);
			x = x * gain + offset;

			if (reflectMin && reflectMax) {
				// TODO find a pen to work this out
			}
			else if (reflectMin) {
				x = simd::fabs(x - min) + min;
				x = simd::fmin(x, max);
			}
			else if (reflectMax) {
				x = max - simd::fabs(max - x);
				x = simd::fmax(x, min);
			}
			else {
				x = simd::clamp(x, min, max);
			}

			outputs[OUT_OUTPUT].setVoltageSimd(x, c);
		}

		outputs[OUT_OUTPUT].setChannels(channels);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "multiplier", json_real(multiplier));
		json_object_set_new(rootJ, "reflectMin", json_boolean(reflectMin));
		json_object_set_new(rootJ, "reflectMax", json_boolean(reflectMax));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* multiplierJ = json_object_get(rootJ, "multiplier");
		if (multiplierJ)
			multiplier = json_number_value(multiplierJ);

		json_t* reflectMinJ = json_object_get(rootJ, "reflectMin");
		if (reflectMinJ)
			reflectMin = json_number_value(reflectMinJ);

		json_t* reflectMaxJ = json_object_get(rootJ, "reflectMax");
		if (reflectMaxJ)
			reflectMax = json_number_value(reflectMaxJ);
	}
};


struct RescaleWidget : ModuleWidget {
	RescaleWidget(Rescale* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Rescale.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 24.723)), module, Rescale::GAIN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.617, 43.031)), module, Rescale::OFFSET_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(7.612, 64.344)), module, Rescale::MAX_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(7.612, 80.597)), module, Rescale::MIN_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 96.859)), module, Rescale::IN_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 113.115)), module, Rescale::OUT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Rescale* module = getModule<Rescale>();

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexSubmenuItem("Gain multiplier", {"1x", "10x", "100x", "1000x"},
			[=]() {
				return (int) std::log10(module->multiplier);
			},
			[=](int mode) {
				module->multiplier = std::pow(10.f, (float) mode);
			}
		));

		menu->addChild(createBoolPtrMenuItem("Reflect at minimum", "", &module->reflectMin));
		menu->addChild(createBoolPtrMenuItem("Reflect at maximum", "", &module->reflectMax));
	}
};


Model* modelRescale = createModel<Rescale, RescaleWidget>("Rescale");