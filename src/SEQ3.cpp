#include "plugin.hpp"


struct SEQ3 : Module {
	enum ParamIds {
		PARAM_CLOCK,
		PARAM_RUN,
		PARAM_RESET,
		PARAM_STEPS,
		ENUMS(PARAM_ROW1, 8),
		ENUMS(PARAM_ROW2, 8),
		ENUMS(PARAM_ROW3, 8),
		ENUMS(PARAM_GATE, 8),
		NUM_PARAMS
	};
	enum InputIds {
		INPUT_CLOCK,
		INPUT_EXT_CLOCK,
		INPUT_RESET,
		INPUT_STEPS,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_GATES,
		OUTPUT_ROW1,
		OUTPUT_ROW2,
		OUTPUT_ROW3,
		ENUMS(OUTPUT_GATE, 8),
		NUM_OUTPUTS
	};
	enum LightIds {
		LIGHT_RUN,
		LIGHT_RESET,
		LIGHT_GATES,
		LIGHT_ROW1,
		LIGHT_ROW2,
		LIGHT_ROW3,
		ENUMS(LIGHT_GATE, 8),
		NUM_LIGHTS
	};

	bool running = true;
	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger runningTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger gateTriggers[8];
	/** Phase of internal LFO */
	float phase = 0.f;
	int index = 0;
	bool gates[8] = {};

	SEQ3() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PARAM_CLOCK, -2.f, 6.f, 2.f, "Clock tempo", " bpm", 2.f, 60.f);
		configButton(PARAM_RUN, "Run");
		configButton(PARAM_RESET, "Reset");
		configParam(PARAM_STEPS, 1.f, 8.f, 8.f, "Steps");
		paramQuantities[PARAM_STEPS]->snapEnabled = true;
		for (int i = 0; i < 8; i++)
			configParam(PARAM_ROW1 + i, 0.f, 10.f, 0.f, string::f("Row 1 step %d", i + 1), " V");
		for (int i = 0; i < 8; i++)
			configParam(PARAM_ROW2 + i, 0.f, 10.f, 0.f, string::f("Row 2 step %d", i + 1), " V");
		for (int i = 0; i < 8; i++)
			configParam(PARAM_ROW3 + i, 0.f, 10.f, 0.f, string::f("Row 3 step %d", i + 1), " V");
		for (int i = 0; i < 8; i++)
			configButton(PARAM_GATE + i, string::f("Gate step %d", i + 1));
		configInput(INPUT_CLOCK, "Clock rate");
		configInput(INPUT_EXT_CLOCK, "External clock");
		configInput(INPUT_RESET, "Reset");
		configInput(INPUT_STEPS, "Steps");
		configOutput(OUTPUT_GATES, "Gate");
		configOutput(OUTPUT_ROW1, "Row 1");
		configOutput(OUTPUT_ROW2, "Row 2");
		configOutput(OUTPUT_ROW3, "Row 3");
		for (int i = 0; i < 8; i++)
			configOutput(OUTPUT_GATE + i, string::f("Gate %d", i + 1));

		onReset();
	}

	void onReset() override {
		for (int i = 0; i < 8; i++) {
			gates[i] = true;
		}
	}

	void onRandomize() override {
		for (int i = 0; i < 8; i++) {
			gates[i] = (random::uniform() > 0.5f);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));

		// gates
		json_t* gatesJ = json_array();
		for (int i = 0; i < 8; i++) {
			json_array_insert_new(gatesJ, i, json_integer((int) gates[i]));
		}
		json_object_set_new(rootJ, "gates", gatesJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// running
		json_t* runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		// gates
		json_t* gatesJ = json_object_get(rootJ, "gates");
		if (gatesJ) {
			for (int i = 0; i < 8; i++) {
				json_t* gateJ = json_array_get(gatesJ, i);
				if (gateJ)
					gates[i] = !!json_integer_value(gateJ);
			}
		}
	}

	void setIndex(int index) {
		int numSteps = (int) clamp(std::round(params[PARAM_STEPS].getValue() + inputs[INPUT_STEPS].getVoltage()), 1.f, 8.f);
		phase = 0.f;
		this->index = index;
		if (this->index >= numSteps)
			this->index = 0;
	}

	void process(const ProcessArgs& args) override {
		// Run
		if (runningTrigger.process(params[PARAM_RUN].getValue())) {
			running = !running;
		}

		bool gateIn = false;
		if (running) {
			if (inputs[INPUT_EXT_CLOCK].isConnected()) {
				// External clock
				if (clockTrigger.process(inputs[INPUT_EXT_CLOCK].getVoltage())) {
					setIndex(index + 1);
				}
				gateIn = clockTrigger.isHigh();
			}
			else {
				// Internal clock
				float clockTime = std::pow(2.f, params[PARAM_CLOCK].getValue() + inputs[INPUT_CLOCK].getVoltage());
				phase += clockTime * args.sampleTime;
				if (phase >= 1.f) {
					setIndex(index + 1);
				}
				gateIn = (phase < 0.5f);
			}
		}

		// Reset
		if (resetTrigger.process(params[PARAM_RESET].getValue() + inputs[INPUT_RESET].getVoltage())) {
			setIndex(0);
		}

		// Gate buttons
		for (int i = 0; i < 8; i++) {
			if (gateTriggers[i].process(params[PARAM_GATE + i].getValue())) {
				gates[i] = !gates[i];
			}
			outputs[OUTPUT_GATE + i].setVoltage((running && gateIn && i == index && gates[i]) ? 10.f : 0.f);
			lights[LIGHT_GATE + i].setSmoothBrightness((gateIn && i == index) ? (gates[i] ? 1.f : 0.33) : (gates[i] ? 0.66 : 0.0), args.sampleTime);
		}

		// Outputs
		outputs[OUTPUT_ROW1].setVoltage(params[PARAM_ROW1 + index].getValue());
		outputs[OUTPUT_ROW2].setVoltage(params[PARAM_ROW2 + index].getValue());
		outputs[OUTPUT_ROW3].setVoltage(params[PARAM_ROW3 + index].getValue());
		outputs[OUTPUT_GATES].setVoltage((gateIn && gates[index]) ? 10.f : 0.f);
		lights[LIGHT_RUN].setBrightness(running);
		lights[LIGHT_RESET].setSmoothBrightness(resetTrigger.isHigh(), args.sampleTime);
		lights[LIGHT_GATES].setSmoothBrightness(gateIn, args.sampleTime);
		lights[LIGHT_ROW1].setBrightness(outputs[OUTPUT_ROW1].getVoltage() / 10.f);
		lights[LIGHT_ROW2].setBrightness(outputs[OUTPUT_ROW2].getVoltage() / 10.f);
		lights[LIGHT_ROW3].setBrightness(outputs[OUTPUT_ROW3].getVoltage() / 10.f);
	}
};


struct SEQ3Widget : ModuleWidget {
	SEQ3Widget(SEQ3* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SEQ3.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.381, 23.626)), module, SEQ3::PARAM_CLOCK));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(23.38, 23.538)), module, SEQ3::PARAM_RUN));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(36.38, 23.538)), module, SEQ3::PARAM_RESET));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(49.381, 23.626)), module, SEQ3::PARAM_STEPS));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.381, 57.712)), module, SEQ3::PARAM_ROW1 + 0));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(23.38, 57.712)), module, SEQ3::PARAM_ROW1 + 1));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(36.38, 57.712)), module, SEQ3::PARAM_ROW1 + 2));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(49.381, 57.712)), module, SEQ3::PARAM_ROW1 + 3));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(62.38, 57.712)), module, SEQ3::PARAM_ROW1 + 4));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.381, 57.712)), module, SEQ3::PARAM_ROW1 + 5));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(88.38, 57.712)), module, SEQ3::PARAM_ROW1 + 6));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(101.38, 57.712)), module, SEQ3::PARAM_ROW1 + 7));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.381, 71.734)), module, SEQ3::PARAM_ROW2 + 0));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(23.38, 71.734)), module, SEQ3::PARAM_ROW2 + 1));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(36.38, 71.734)), module, SEQ3::PARAM_ROW2 + 2));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(49.381, 71.734)), module, SEQ3::PARAM_ROW2 + 3));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(62.38, 71.734)), module, SEQ3::PARAM_ROW2 + 4));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.381, 71.734)), module, SEQ3::PARAM_ROW2 + 5));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(88.38, 71.734)), module, SEQ3::PARAM_ROW2 + 6));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(101.38, 71.734)), module, SEQ3::PARAM_ROW2 + 7));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.381, 85.711)), module, SEQ3::PARAM_ROW3 + 0));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(23.38, 85.711)), module, SEQ3::PARAM_ROW3 + 1));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(36.38, 85.711)), module, SEQ3::PARAM_ROW3 + 2));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(49.381, 85.711)), module, SEQ3::PARAM_ROW3 + 3));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(62.38, 85.711)), module, SEQ3::PARAM_ROW3 + 4));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.381, 85.711)), module, SEQ3::PARAM_ROW3 + 5));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(88.38, 85.711)), module, SEQ3::PARAM_ROW3 + 6));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(101.38, 85.711)), module, SEQ3::PARAM_ROW3 + 7));

		addParam(createParamCentered<LEDButton>(mm2px(Vec(10.381, 96.945)), module, SEQ3::PARAM_GATE + 0));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(23.381, 96.945)), module, SEQ3::PARAM_GATE + 1));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(36.38, 96.945)), module, SEQ3::PARAM_GATE + 2));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(49.381, 96.945)), module, SEQ3::PARAM_GATE + 3));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(62.381, 96.945)), module, SEQ3::PARAM_GATE + 4));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(75.381, 96.945)), module, SEQ3::PARAM_GATE + 5));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(88.381, 96.945)), module, SEQ3::PARAM_GATE + 6));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(101.38, 96.945)), module, SEQ3::PARAM_GATE + 7));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.381, 37.331)), module, SEQ3::INPUT_CLOCK));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(23.38, 37.331)), module, SEQ3::INPUT_EXT_CLOCK));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(36.38, 37.331)), module, SEQ3::INPUT_RESET));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(49.381, 37.331)), module, SEQ3::INPUT_STEPS));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(62.38, 37.329)), module, SEQ3::OUTPUT_GATES));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(75.38, 37.329)), module, SEQ3::OUTPUT_ROW1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(88.38, 37.329)), module, SEQ3::OUTPUT_ROW2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(101.381, 37.329)), module, SEQ3::OUTPUT_ROW3));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.381, 107.865)), module, SEQ3::OUTPUT_GATE + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(23.38, 107.865)), module, SEQ3::OUTPUT_GATE + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(36.38, 107.865)), module, SEQ3::OUTPUT_GATE + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(49.381, 107.865)), module, SEQ3::OUTPUT_GATE + 3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(62.38, 107.865)), module, SEQ3::OUTPUT_GATE + 4));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(75.38, 107.865)), module, SEQ3::OUTPUT_GATE + 5));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(88.38, 107.865)), module, SEQ3::OUTPUT_GATE + 6));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(101.381, 107.865)), module, SEQ3::OUTPUT_GATE + 7));

		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(23.38, 23.538)), module, SEQ3::LIGHT_RUN));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(36.38, 23.538)), module, SEQ3::LIGHT_RESET));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(62.38, 23.538)), module, SEQ3::LIGHT_GATE));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(75.38, 23.538)), module, SEQ3::LIGHT_ROW1));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(88.38, 23.538)), module, SEQ3::LIGHT_ROW2));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(101.381, 23.538)), module, SEQ3::LIGHT_ROW3));

		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(10.381, 96.945)), module, SEQ3::LIGHT_GATE + 0));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(23.381, 96.945)), module, SEQ3::LIGHT_GATE + 1));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(36.38, 96.945)), module, SEQ3::LIGHT_GATE + 2));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(49.381, 96.945)), module, SEQ3::LIGHT_GATE + 3));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(62.381, 96.945)), module, SEQ3::LIGHT_GATE + 4));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(75.381, 96.945)), module, SEQ3::LIGHT_GATE + 5));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(88.381, 96.945)), module, SEQ3::LIGHT_GATE + 6));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(101.38, 96.945)), module, SEQ3::LIGHT_GATE + 7));
	}
};



Model* modelSEQ3 = createModel<SEQ3, SEQ3Widget>("SEQ3");
