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
		DELAY_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		RESET_LIGHT,
		ENUMS(RISE_LIGHT, 2),
		ENUMS(FALL_LIGHT, 2),
		ENUMS(FLIP_LIGHT, 2),
		ENUMS(FLOP_LIGHT, 2),
		ENUMS(GATE_LIGHT, 2),
		ENUMS(DELAY_LIGHT, 2),
		LIGHTS_LEN
	};

	double time = 0.0;
	dsp::BooleanTrigger resetParamTrigger;
	dsp::ClockDivider lightDivider;

	struct Engine {
		bool state = false;
		dsp::SchmittTrigger resetTrigger;
		dsp::PulseGenerator risePulse;
		dsp::PulseGenerator fallPulse;
		bool flop = false;
		float gateTime = INFINITY;
		// TODO Change this to a circular buffer with binary search, to avoid allocations when events are pushed.
		std::map<double, bool> stateEvents;
	};
	Engine engines[16];

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
		configOutput(DELAY_OUTPUT, "Gate delay");

		lightDivider.setDivision(32);
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(1, inputs[IN_INPUT].getChannels());

		bool anyRise = false;
		bool anyFall = false;
		bool anyFlip = false;
		bool anyFlop = false;
		bool anyGate = false;
		bool anyDelay = false;

		// Reset
		bool resetButton = false;
		if (resetParamTrigger.process(params[RESET_PARAM].getValue() > 0.f)) {
			resetButton = true;
		}

		// Process channels
		for (int c = 0; c < channels; c++) {
			Engine& e = engines[c];
			float in = inputs[IN_INPUT].getVoltage(c);

			bool newState = false;
			if (e.state) {
				// HIGH to LOW
				if (in <= 0.1f) {
					e.state = false;
					e.fallPulse.trigger(1e-3f);
					newState = true;
				}
			}
			else {
				// LOW to HIGH
				if (in >= 2.f) {
					e.state = true;
					e.risePulse.trigger(1e-3f);
					// Flip flop
					e.flop ^= true;
					// Gate
					e.gateTime = 0.f;
					newState = true;
				}
			}

			// Reset
			bool reset = resetButton;
			if (e.resetTrigger.process(inputs[RESET_INPUT].getVoltage(c), 0.1f, 2.f)) {
				reset = true;
			}
			if (reset) {
				e.flop = false;
				e.stateEvents.clear();
			}

			// Outputs
			bool rise = e.risePulse.process(args.sampleTime);
			outputs[RISE_OUTPUT].setVoltage(rise ? 10.f : 0.f, c);
			anyRise = anyRise || rise;

			bool fall = e.fallPulse.process(args.sampleTime);
			outputs[FALL_OUTPUT].setVoltage(fall ? 10.f : 0.f, c);
			anyFall = anyFall || fall;

			outputs[FLIP_OUTPUT].setVoltage(!e.flop ? 10.f : 0.f, c);
			anyFlip = anyFlip || !e.flop;

			outputs[FLOP_OUTPUT].setVoltage(e.flop ? 10.f : 0.f, c);
			anyFlop = anyFlop || e.flop;

			// Gate output
			float gatePitch = params[LENGTH_PARAM].getValue() + inputs[LENGTH_INPUT].getPolyVoltage(c);
			float gateLength = dsp::exp2_taylor5(gatePitch + 30.f) / 1073741824;
			if (std::isfinite(e.gateTime)) {
				e.gateTime += args.sampleTime;
				if (reset || e.gateTime >= gateLength) {
					e.gateTime = INFINITY;
				}
			}

			bool gate = std::isfinite(e.gateTime);
			outputs[GATE_OUTPUT].setVoltage(gate ? 10.f : 0.f, c);
			anyGate = anyGate || gate;

			// Gate delay output
			if (outputs[DELAY_OUTPUT].isConnected()) {
				bool delayGate = false;
				// Timestamp of past gate
				double delayTime = time - gateLength;
				// Find event less than or equal to delayTime.
				// If not found, gate will be off.
				auto eventIt = e.stateEvents.upper_bound(delayTime);
				if (eventIt != e.stateEvents.begin()) {
					eventIt--;
					delayGate = eventIt->second;
				}

				if (newState) {
					// Keep buffer a reasonable size
					if (e.stateEvents.size() >= (1 << 10) - 1) {
						e.stateEvents.erase(e.stateEvents.begin());
					}
					// Insert current state at current time
					e.stateEvents[time] = e.state;
				}

				outputs[DELAY_OUTPUT].setVoltage(delayGate ? 10.f : 0.f, c);
				anyDelay = anyDelay || delayGate;
			}
		}

		time += args.sampleTime;

		outputs[RISE_OUTPUT].setChannels(channels);
		outputs[FALL_OUTPUT].setChannels(channels);
		outputs[FLIP_OUTPUT].setChannels(channels);
		outputs[FLOP_OUTPUT].setChannels(channels);
		outputs[GATE_OUTPUT].setChannels(channels);
		outputs[DELAY_OUTPUT].setChannels(channels);

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
			lights[DELAY_LIGHT + 0].setBrightnessSmooth(anyDelay && channels <= 1, lightTime);
			lights[DELAY_LIGHT + 1].setBrightnessSmooth(anyDelay && channels > 1, lightTime);
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
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 113.115)), module, Gates::DELAY_OUTPUT));

		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(11.027, 79.007)), module, Gates::RISE_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(21.864, 79.007)), module, Gates::FALL_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(11.027, 94.233)), module, Gates::FLIP_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(21.864, 94.233)), module, Gates::FLOP_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(11.027, 109.393)), module, Gates::GATE_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(21.864, 109.393)), module, Gates::DELAY_LIGHT));
	}
};


Model* modelGates = createModel<Gates, GatesWidget>("Gates");