#include "Fundamental.hpp"


struct SEQ3 : Module {
	enum ParamIds {
		CLOCK_PARAM,
		RUN_PARAM,
		RESET_PARAM,
		STEPS_PARAM,
		ENUMS(ROW1_PARAM, 8),
		ENUMS(ROW2_PARAM, 8),
		ENUMS(ROW3_PARAM, 8),
		ENUMS(GATE_PARAM, 8),
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		EXT_CLOCK_INPUT,
		RESET_INPUT,
		STEPS_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATES_OUTPUT,
		ROW1_OUTPUT,
		ROW2_OUTPUT,
		ROW3_OUTPUT,
		ENUMS(GATE_OUTPUT, 8),
		NUM_OUTPUTS
	};
	enum LightIds {
		RUNNING_LIGHT,
		RESET_LIGHT,
		GATES_LIGHT,
		ENUMS(ROW_LIGHTS, 3),
		ENUMS(GATE_LIGHTS, 8),
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
		params[CLOCK_PARAM].config(-2.0f, 6.0f, 2.0f);
		params[RUN_PARAM].config(0.0f, 1.0f, 0.0f);
		params[RESET_PARAM].config(0.0f, 1.0f, 0.0f);
		params[STEPS_PARAM].config(1.0f, 8.0f, 8.0f);
		for (int i = 0; i < 8; i++) {
			params[ROW1_PARAM + i].config(0.0f, 10.0f, 0.0f);
			params[ROW2_PARAM + i].config(0.0f, 10.0f, 0.0f);
			params[ROW3_PARAM + i].config(0.0f, 10.0f, 0.0f);
			params[GATE_PARAM + i].config(0.0f, 1.0f, 0.0f);
		}


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

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));

		// gates
		json_t *gatesJ = json_array();
		for (int i = 0; i < 8; i++) {
			json_array_insert_new(gatesJ, i, json_integer((int) gates[i]));
		}
		json_object_set_new(rootJ, "gates", gatesJ);

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		// gates
		json_t *gatesJ = json_object_get(rootJ, "gates");
		if (gatesJ) {
			for (int i = 0; i < 8; i++) {
				json_t *gateJ = json_array_get(gatesJ, i);
				if (gateJ)
					gates[i] = !!json_integer_value(gateJ);
			}
		}
	}

	void setIndex(int index) {
		int numSteps = (int) clamp(std::round(params[STEPS_PARAM].value + inputs[STEPS_INPUT].value), 1.0f, 8.0f);
		phase = 0.f;
		this->index = index;
		if (this->index >= numSteps)
			this->index = 0;
	}

	void step() override {
		// Run
		if (runningTrigger.process(params[RUN_PARAM].value)) {
			running = !running;
		}

		bool gateIn = false;
		if (running) {
			if (inputs[EXT_CLOCK_INPUT].active) {
				// External clock
				if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].value)) {
					setIndex(index + 1);
				}
				gateIn = clockTrigger.isHigh();
			}
			else {
				// Internal clock
				float clockTime = std::pow(2.0f, params[CLOCK_PARAM].value + inputs[CLOCK_INPUT].value);
				phase += clockTime * app()->engine->getSampleTime();
				if (phase >= 1.0f) {
					setIndex(index + 1);
				}
				gateIn = (phase < 0.5f);
			}
		}

		// Reset
		if (resetTrigger.process(params[RESET_PARAM].value + inputs[RESET_INPUT].value)) {
			setIndex(0);
		}

		// Gate buttons
		for (int i = 0; i < 8; i++) {
			if (gateTriggers[i].process(params[GATE_PARAM + i].value)) {
				gates[i] = !gates[i];
			}
			outputs[GATE_OUTPUT + i].value = (running && gateIn && i == index && gates[i]) ? 10.0f : 0.0f;
			lights[GATE_LIGHTS + i].setBrightnessSmooth((gateIn && i == index) ? (gates[i] ? 1.f : 0.33) : (gates[i] ? 0.66 : 0.0));
		}

		// Outputs
		outputs[ROW1_OUTPUT].value = params[ROW1_PARAM + index].value;
		outputs[ROW2_OUTPUT].value = params[ROW2_PARAM + index].value;
		outputs[ROW3_OUTPUT].value = params[ROW3_PARAM + index].value;
		outputs[GATES_OUTPUT].value = (gateIn && gates[index]) ? 10.0f : 0.0f;
		lights[RUNNING_LIGHT].value = (running);
		lights[RESET_LIGHT].setBrightnessSmooth(resetTrigger.isHigh());
		lights[GATES_LIGHT].setBrightnessSmooth(gateIn);
		lights[ROW_LIGHTS].value = outputs[ROW1_OUTPUT].value / 10.0f;
		lights[ROW_LIGHTS + 1].value = outputs[ROW2_OUTPUT].value / 10.0f;
		lights[ROW_LIGHTS + 2].value = outputs[ROW3_OUTPUT].value / 10.0f;
	}
};


struct SEQ3Widget : ModuleWidget {
	SEQ3Widget(SEQ3 *module) : ModuleWidget(module) {
		setPanel(SVG::load(asset::plugin(plugin, "res/SEQ3.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParam<RoundBlackKnob>(Vec(18, 56), module, SEQ3::CLOCK_PARAM));
		addParam(createParam<LEDButton>(Vec(60, 61-1), module, SEQ3::RUN_PARAM));
		addChild(createLight<MediumLight<GreenLight>>(Vec(64.4f, 64.4f), module, SEQ3::RUNNING_LIGHT));
		addParam(createParam<LEDButton>(Vec(99, 61-1), module, SEQ3::RESET_PARAM));
		addChild(createLight<MediumLight<GreenLight>>(Vec(103.4f, 64.4f), module, SEQ3::RESET_LIGHT));
		addParam(createParam<RoundBlackSnapKnob>(Vec(132, 56), module, SEQ3::STEPS_PARAM));
		addChild(createLight<MediumLight<GreenLight>>(Vec(179.4f, 64.4f), module, SEQ3::GATES_LIGHT));
		addChild(createLight<MediumLight<GreenLight>>(Vec(218.4f, 64.4f), module, SEQ3::ROW_LIGHTS));
		addChild(createLight<MediumLight<GreenLight>>(Vec(256.4f, 64.4f), module, SEQ3::ROW_LIGHTS + 1));
		addChild(createLight<MediumLight<GreenLight>>(Vec(295.4f, 64.4f), module, SEQ3::ROW_LIGHTS + 2));

		static const float portX[8] = {20, 58, 96, 135, 173, 212, 250, 289};
		addInput(createInput<PJ301MPort>(Vec(portX[0]-1, 98), module, SEQ3::CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(Vec(portX[1]-1, 98), module, SEQ3::EXT_CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(Vec(portX[2]-1, 98), module, SEQ3::RESET_INPUT));
		addInput(createInput<PJ301MPort>(Vec(portX[3]-1, 98), module, SEQ3::STEPS_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(portX[4]-1, 98), module, SEQ3::GATES_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(portX[5]-1, 98), module, SEQ3::ROW1_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(portX[6]-1, 98), module, SEQ3::ROW2_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(portX[7]-1, 98), module, SEQ3::ROW3_OUTPUT));

		for (int i = 0; i < 8; i++) {
			addParam(createParam<RoundBlackKnob>(Vec(portX[i]-2, 157), module, SEQ3::ROW1_PARAM + i));
			addParam(createParam<RoundBlackKnob>(Vec(portX[i]-2, 198), module, SEQ3::ROW2_PARAM + i));
			addParam(createParam<RoundBlackKnob>(Vec(portX[i]-2, 240), module, SEQ3::ROW3_PARAM + i));
			addParam(createParam<LEDButton>(Vec(portX[i]+2, 278-1), module, SEQ3::GATE_PARAM + i));
			addChild(createLight<MediumLight<GreenLight>>(Vec(portX[i]+6.4f, 281.4f), module, SEQ3::GATE_LIGHTS + i));
			addOutput(createOutput<PJ301MPort>(Vec(portX[i]-1, 307), module, SEQ3::GATE_OUTPUT + i));
		}
	}
};



Model *modelSEQ3 = createModel<SEQ3, SEQ3Widget>("SEQ3");
