#include "plugin.hpp"


struct Gates : Module {
	enum ParamId {
		LENGTH_PARAM,
		RESET_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		LENGTH_INPUT,
		IN_INPUT,
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		RISE_OUTPUT,
		FALL_OUTPUT,
		FLIP_OUTPUT,
		FLOP_OUTPUT,
		GATE_OUTPUT,
		END_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		RESET_LIGHT,
		ENUMS(RISE_LIGHT, 2),
		ENUMS(FALL_LIGHT, 2),
		ENUMS(FLIP_LIGHT, 2),
		ENUMS(FLOP_LIGHT, 2),
		ENUMS(GATE_LIGHT, 2),
		ENUMS(END_LIGHT, 2),
		LIGHTS_LEN
	};

	dsp::BooleanTrigger resetParamTrigger;
	bool state[16] = {};
	dsp::SchmittTrigger resetTrigger[16];
	dsp::PulseGenerator risePulse[16];
	dsp::PulseGenerator fallPulse[16];
	dsp::PulseGenerator eogPulse[16];
	bool flop[16] = {};
	float gateTime[16] = {};
	dsp::ClockDivider lightDivider;

	Gates() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(LENGTH_PARAM, std::log2(1e-3f), std::log2(10.f), std::log2(0.1f), "Gate length", " ms", 2, 1000);
		configButton(RESET_PARAM, "Reset flip/flop");
		configInput(LENGTH_INPUT, "Gate length");
		configInput(IN_INPUT, "Gate");
		configInput(RESET_INPUT, "Reset flip/flop");
		configOutput(RISE_OUTPUT, "Rising edge trigger");
		configOutput(FALL_OUTPUT, "Falling edge trigger");
		configOutput(FLIP_OUTPUT, "Flip");
		configOutput(FLOP_OUTPUT, "Flop");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(END_OUTPUT, "End of gate");

		lightDivider.setDivision(32);
		for (int c = 0; c < 16; c++) {
			gateTime[c] = INFINITY;
		}
	}

	void process(const ProcessArgs& args) override {
		int channels = inputs[IN_INPUT].getChannels();

		bool anyRise = false;
		bool anyFall = false;
		bool anyFlip = false;
		bool anyFlop = false;
		bool anyGate = false;
		bool anyEnd = false;

		// Reset
		bool resetButton = false;
		if (resetParamTrigger.process(params[RESET_PARAM].getValue() > 0.f)) {
			resetButton = true;
		}

		// Process channels
		for (int c = 0; c < channels; c++) {
			float in = inputs[IN_INPUT].getVoltage(c);

			if (state[c]) {
				// HIGH to LOW
				if (in <= 0.1f) {
					state[c] = false;
					fallPulse[c].trigger(1e-3f);
				}
			}
			else {
				// LOW to HIGH
				if (in >= 2.f) {
					state[c] = true;
					risePulse[c].trigger(1e-3f);
					// Flip flop
					flop[c] ^= true;
					// Gate
					gateTime[c] = 0.f;
				}
			}

			// Reset
			bool reset = resetButton;
			if (resetTrigger[c].process(inputs[RESET_INPUT].getVoltage(c), 0.1f, 2.f)) {
				reset = true;
			}
			if (reset) {
				flop[c] = false;
			}

			// Gate
			float gatePitch = params[LENGTH_PARAM].getValue() + inputs[LENGTH_INPUT].getPolyVoltage(c);
			float gateLength = dsp::approxExp2_taylor5(gatePitch + 30.f) / 1073741824;
			if (std::isfinite(gateTime[c])) {
				gateTime[c] += args.sampleTime;
				if (reset || gateTime[c] >= gateLength) {
					gateTime[c] = INFINITY;
					eogPulse[c].trigger(1e-3f);
				}
			}

			// Outputs
			bool rise = risePulse[c].process(args.sampleTime);
			outputs[RISE_OUTPUT].setVoltage(rise ? 10.f : 0.f, c);
			anyRise = anyRise || rise;

			bool fall = fallPulse[c].process(args.sampleTime);
			outputs[FALL_OUTPUT].setVoltage(fall ? 10.f : 0.f, c);
			anyFall = anyFall || fall;

			outputs[FLIP_OUTPUT].setVoltage(!flop[c] ? 10.f : 0.f, c);
			anyFlip = anyFlip || !flop[c];

			outputs[FLOP_OUTPUT].setVoltage(flop[c] ? 10.f : 0.f, c);
			anyFlop = anyFlop || flop[c];

			bool gate = std::isfinite(gateTime[c]);
			outputs[GATE_OUTPUT].setVoltage(gate ? 10.f : 0.f, c);
			anyGate = anyGate || gate;

			bool end = eogPulse[c].process(args.sampleTime);
			outputs[END_OUTPUT].setVoltage(end ? 10.f : 0.f, c);
			anyEnd = anyEnd || end;
		}

		outputs[RISE_OUTPUT].setChannels(channels);
		outputs[FALL_OUTPUT].setChannels(channels);
		outputs[FLIP_OUTPUT].setChannels(channels);
		outputs[FLOP_OUTPUT].setChannels(channels);
		outputs[GATE_OUTPUT].setChannels(channels);
		outputs[END_OUTPUT].setChannels(channels);

		if (lightDivider.process()) {
			float lightTime = args.sampleTime * lightDivider.getDivision();
			lights[RESET_LIGHT].setBrightness(params[RESET_PARAM].getValue() > 0.f);
			lights[RISE_LIGHT + 0].setBrightnessSmooth(anyRise && channels <= 1, lightTime);
			lights[RISE_LIGHT + 1].setBrightnessSmooth(anyRise && channels > 1, lightTime);
			lights[FALL_LIGHT + 0].setBrightnessSmooth(anyFall && channels <= 1, lightTime);
			lights[FALL_LIGHT + 1].setBrightnessSmooth(anyFall && channels > 1, lightTime);
			lights[FLIP_LIGHT + 0].setBrightnessSmooth(anyFlip && channels <= 1, lightTime);
			lights[FLIP_LIGHT + 1].setBrightnessSmooth(anyFlip && channels > 1, lightTime);
			lights[FLOP_LIGHT + 0].setBrightnessSmooth(anyFlop && channels <= 1, lightTime);
			lights[FLOP_LIGHT + 1].setBrightnessSmooth(anyFlop && channels > 1, lightTime);
			lights[GATE_LIGHT + 0].setBrightnessSmooth(anyGate && channels <= 1, lightTime);
			lights[GATE_LIGHT + 1].setBrightnessSmooth(anyGate && channels > 1, lightTime);
			lights[END_LIGHT + 0].setBrightnessSmooth(anyEnd && channels <= 1, lightTime);
			lights[END_LIGHT + 1].setBrightnessSmooth(anyEnd && channels > 1, lightTime);
		}
	}
};


struct GatesWidget : ModuleWidget {
	GatesWidget(Gates* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Gates.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(12.646, 26.755)), module, Gates::LENGTH_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(18.146, 52.31)), module, Gates::RESET_PARAM, Gates::RESET_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.299, 52.31)), module, Gates::LENGTH_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.297, 67.53)), module, Gates::IN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.132, 67.53)), module, Gates::RESET_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.297, 82.732)), module, Gates::RISE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 82.732)), module, Gates::FALL_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.297, 97.958)), module, Gates::FLIP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 97.958)), module, Gates::FLOP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.297, 113.115)), module, Gates::GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 113.115)), module, Gates::END_OUTPUT));

		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(11.027, 79.007)), module, Gates::RISE_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(21.864, 79.007)), module, Gates::FALL_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(11.027, 94.233)), module, Gates::FLIP_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(21.864, 94.233)), module, Gates::FLOP_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(11.027, 109.393)), module, Gates::GATE_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(21.864, 109.393)), module, Gates::END_LIGHT));
	}
};


Model* modelGates = createModel<Gates, GatesWidget>("Gates");