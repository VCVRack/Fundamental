#include "plugin.hpp"


struct SEQ3 : Module {
	enum ParamIds {
		TEMPO_PARAM,
		RUN_PARAM,
		RESET_PARAM,
		STEPS_PARAM,
		ENUMS(ROW_PARAMS, 3 * 8),
		ENUMS(GATE_PARAMS, 8),
		// added in 2.0
		TEMPO_CV_PARAM,
		STEPS_CV_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		TEMPO_INPUT,
		CLOCK_INPUT,
		RESET_INPUT,
		STEPS_INPUT,
		// added in 2.0
		RUN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATE_OUTPUT,
		CV1_OUTPUT,
		CV2_OUTPUT,
		CV3_OUTPUT,
		ENUMS(STEP_OUTPUTS, 8),
		// added in 2.0
		STEPS_OUTPUT,
		CLOCK_OUTPUT,
		RUN_OUTPUT,
		RESET_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		CLOCK_LIGHT,
		RUN_LIGHT,
		RESET_LIGHT,
		ENUMS(GATE_LIGHTS, 8),
		ENUMS(STEP_LIGHTS, 8),
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
		configParam(TEMPO_PARAM, -2.f, 4.f, 1.f, "Tempo", " bpm", 2.f, 60.f);
		configParam(TEMPO_CV_PARAM, 0.f, 1.f, 1.f, "Tempo CV", "%", 0, 100);
		configButton(RUN_PARAM, "Run");
		configButton(RESET_PARAM, "Reset");
		configParam(STEPS_PARAM, 1.f, 8.f, 8.f, "Steps");
		configParam(STEPS_CV_PARAM, 0.f, 1.f, 1.f, "Steps CV", "%", 0, 100);
		paramQuantities[STEPS_PARAM]->snapEnabled = true;
		for (int j = 0; j < 3; j++) {
			for (int i = 0; i < 8; i++) {
				configParam(ROW_PARAMS + 8 * j + i, -10.f, 10.f, 0.f, string::f("Row %d step %d", j + 1, i + 1), " V");
			}
		}
		configInput(TEMPO_INPUT, "Clock rate");
		configInput(CLOCK_INPUT, "External clock");
		configInput(RESET_INPUT, "Reset");
		configInput(STEPS_INPUT, "Steps");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(CV1_OUTPUT, "Row 1");
		configOutput(CV2_OUTPUT, "Row 2");
		configOutput(CV3_OUTPUT, "Row 3");
		for (int i = 0; i < 8; i++)
			configOutput(STEP_OUTPUTS + i, string::f("Step %d", i + 1));

		configLight(CLOCK_LIGHT, "Clock trigger");

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
		int numSteps = (int) clamp(std::round(params[STEPS_PARAM].getValue() + inputs[STEPS_INPUT].getVoltage()), 1.f, 8.f);
		phase = 0.f;
		this->index = index;
		if (this->index >= numSteps)
			this->index = 0;
	}

	void process(const ProcessArgs& args) override {
		// Run
		if (runningTrigger.process(params[RUN_PARAM].getValue())) {
			running = !running;
		}

		bool gateIn = false;
		if (running) {
			if (inputs[CLOCK_INPUT].isConnected()) {
				// External clock
				if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
					setIndex(index + 1);
				}
				gateIn = clockTrigger.isHigh();
			}
			else {
				// Internal clock
				float clockTime = std::pow(2.f, params[TEMPO_PARAM].getValue() + inputs[TEMPO_INPUT].getVoltage());
				phase += clockTime * args.sampleTime;
				if (phase >= 1.f) {
					setIndex(index + 1);
				}
				gateIn = (phase < 0.5f);
			}
		}

		// Reset
		if (resetTrigger.process(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage())) {
			setIndex(0);
		}

		// Gate buttons
		for (int i = 0; i < 8; i++) {
			if (gateTriggers[i].process(params[GATE_PARAMS + i].getValue())) {
				gates[i] = !gates[i];
			}
			outputs[STEP_OUTPUTS + i].setVoltage((running && gateIn && i == index && gates[i]) ? 10.f : 0.f);
			lights[GATE_LIGHTS + i].setSmoothBrightness((gateIn && i == index) ? (gates[i] ? 1.f : 0.33) : (gates[i] ? 0.66 : 0.0), args.sampleTime);
		}

		// Outputs
		outputs[CV1_OUTPUT].setVoltage(params[ROW_PARAMS * 8 + 0 + index].getValue());
		outputs[CV2_OUTPUT].setVoltage(params[ROW_PARAMS * 8 + 1 + index].getValue());
		outputs[CV3_OUTPUT].setVoltage(params[ROW_PARAMS * 8 + 2 + index].getValue());
		outputs[GATE_OUTPUT].setVoltage((gateIn && gates[index]) ? 10.f : 0.f);
		lights[RUN_LIGHT].setBrightness(running);
		lights[RESET_LIGHT].setSmoothBrightness(resetTrigger.isHigh(), args.sampleTime);
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

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(11.753, 26.755)), module, SEQ3::TEMPO_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(32.077, 26.782)), module, SEQ3::STEPS_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(49.372, 34.066)), module, SEQ3::TEMPO_CV_PARAM));
		addParam(createLightParamCentered<LEDLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(88.424, 33.679)), module, SEQ3::RUN_PARAM, SEQ3::RUN_LIGHT));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(62.39, 34.066)), module, SEQ3::STEPS_CV_PARAM));
		addParam(createLightParamCentered<LEDLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(101.441, 33.679)), module, SEQ3::RESET_PARAM, SEQ3::RESET_LIGHT));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.319, 46.563)), module, SEQ3::ROW_PARAMS + 8 * 0 + 0));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(23.336, 46.563)), module, SEQ3::ROW_PARAMS + 8 * 0 + 1));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(36.354, 46.563)), module, SEQ3::ROW_PARAMS + 8 * 0 + 2));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(49.371, 46.563)), module, SEQ3::ROW_PARAMS + 8 * 0 + 3));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(62.389, 46.563)), module, SEQ3::ROW_PARAMS + 8 * 0 + 4));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.406, 46.563)), module, SEQ3::ROW_PARAMS + 8 * 0 + 5));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(88.424, 46.563)), module, SEQ3::ROW_PARAMS + 8 * 0 + 6));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(101.441, 46.563)), module, SEQ3::ROW_PARAMS + 8 * 0 + 7));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.319, 60.607)), module, SEQ3::ROW_PARAMS + 8 * 1 + 0));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(23.336, 60.607)), module, SEQ3::ROW_PARAMS + 8 * 1 + 1));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(36.354, 60.607)), module, SEQ3::ROW_PARAMS + 8 * 1 + 2));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(49.371, 60.607)), module, SEQ3::ROW_PARAMS + 8 * 1 + 3));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(62.389, 60.607)), module, SEQ3::ROW_PARAMS + 8 * 1 + 4));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.406, 60.607)), module, SEQ3::ROW_PARAMS + 8 * 1 + 5));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(88.424, 60.607)), module, SEQ3::ROW_PARAMS + 8 * 1 + 6));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(101.441, 60.607)), module, SEQ3::ROW_PARAMS + 8 * 1 + 7));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.319, 74.605)), module, SEQ3::ROW_PARAMS + 8 * 2 + 0));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(23.336, 74.605)), module, SEQ3::ROW_PARAMS + 8 * 2 + 1));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(36.354, 74.605)), module, SEQ3::ROW_PARAMS + 8 * 2 + 2));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(49.371, 74.605)), module, SEQ3::ROW_PARAMS + 8 * 2 + 3));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(62.389, 74.605)), module, SEQ3::ROW_PARAMS + 8 * 2 + 4));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.406, 74.605)), module, SEQ3::ROW_PARAMS + 8 * 2 + 5));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(88.424, 74.605)), module, SEQ3::ROW_PARAMS + 8 * 2 + 6));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(101.441, 74.605)), module, SEQ3::ROW_PARAMS + 8 * 2 + 7));

		addParam(createLightParamCentered<LEDLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(10.319, 85.801)), module, SEQ3::GATE_PARAMS + 0, SEQ3::GATE_LIGHTS + 0));
		addParam(createLightParamCentered<LEDLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(23.336, 85.801)), module, SEQ3::GATE_PARAMS + 1, SEQ3::GATE_LIGHTS + 1));
		addParam(createLightParamCentered<LEDLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(36.354, 85.801)), module, SEQ3::GATE_PARAMS + 2, SEQ3::GATE_LIGHTS + 2));
		addParam(createLightParamCentered<LEDLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(49.371, 85.801)), module, SEQ3::GATE_PARAMS + 3, SEQ3::GATE_LIGHTS + 3));
		addParam(createLightParamCentered<LEDLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(62.389, 85.801)), module, SEQ3::GATE_PARAMS + 4, SEQ3::GATE_LIGHTS + 4));
		addParam(createLightParamCentered<LEDLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(75.406, 85.801)), module, SEQ3::GATE_PARAMS + 5, SEQ3::GATE_LIGHTS + 5));
		addParam(createLightParamCentered<LEDLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(88.424, 85.801)), module, SEQ3::GATE_PARAMS + 6, SEQ3::GATE_LIGHTS + 6));
		addParam(createLightParamCentered<LEDLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(101.441, 85.801)), module, SEQ3::GATE_PARAMS + 7, SEQ3::GATE_LIGHTS + 7));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(49.371, 17.307)), module, SEQ3::TEMPO_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(62.389, 17.307)), module, SEQ3::STEPS_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(75.406, 17.42)), module, SEQ3::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(88.424, 17.42)), module, SEQ3::RUN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(101.441, 17.42)), module, SEQ3::RESET_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.319, 96.859)), module, SEQ3::STEP_OUTPUTS + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(23.336, 96.859)), module, SEQ3::STEP_OUTPUTS + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(36.354, 96.859)), module, SEQ3::STEP_OUTPUTS + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(49.371, 96.859)), module, SEQ3::STEP_OUTPUTS + 3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(62.389, 96.859)), module, SEQ3::STEP_OUTPUTS + 4));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(75.406, 96.859)), module, SEQ3::STEP_OUTPUTS + 5));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(88.424, 96.859)), module, SEQ3::STEP_OUTPUTS + 6));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(101.441, 96.859)), module, SEQ3::STEP_OUTPUTS + 7));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.319, 113.115)), module, SEQ3::CV1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(23.336, 113.115)), module, SEQ3::CV2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(36.354, 113.115)), module, SEQ3::CV3_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(49.371, 113.115)), module, SEQ3::GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(62.389, 113.115)), module, SEQ3::STEPS_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(75.406, 113.115)), module, SEQ3::CLOCK_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(88.424, 113.115)), module, SEQ3::RUN_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(101.441, 113.115)), module, SEQ3::RESET_OUTPUT));

		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(75.406, 33.497)), module, SEQ3::CLOCK_LIGHT));

		addChild(createLightCentered<TinyLight<YellowLight>>(mm2px(Vec(14.064, 93.103)), module, SEQ3::GATE_LIGHTS + 0));
		addChild(createLightCentered<TinyLight<YellowLight>>(mm2px(Vec(27.084, 93.103)), module, SEQ3::GATE_LIGHTS + 1));
		addChild(createLightCentered<TinyLight<YellowLight>>(mm2px(Vec(40.103, 93.103)), module, SEQ3::GATE_LIGHTS + 2));
		addChild(createLightCentered<TinyLight<YellowLight>>(mm2px(Vec(53.122, 93.103)), module, SEQ3::GATE_LIGHTS + 3));
		addChild(createLightCentered<TinyLight<YellowLight>>(mm2px(Vec(66.142, 93.103)), module, SEQ3::GATE_LIGHTS + 4));
		addChild(createLightCentered<TinyLight<YellowLight>>(mm2px(Vec(79.161, 93.103)), module, SEQ3::GATE_LIGHTS + 5));
		addChild(createLightCentered<TinyLight<YellowLight>>(mm2px(Vec(92.181, 93.103)), module, SEQ3::GATE_LIGHTS + 6));
		addChild(createLightCentered<TinyLight<YellowLight>>(mm2px(Vec(105.2, 93.103)), module, SEQ3::GATE_LIGHTS + 7));
	}
};



Model* modelSEQ3 = createModel<SEQ3, SEQ3Widget>("SEQ3");
