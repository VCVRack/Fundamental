#include "plugin.hpp"


using simd::float_4;


struct LFO : Module {
	enum ParamIds {
		OFFSET_PARAM,
		INVERT_PARAM,
		FREQ_PARAM,
		FM_PARAM,
		FM2_PARAM, // removed
		PW_PARAM,
		PWM_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FM_INPUT,
		FM2_INPUT, // removed
		RESET_INPUT,
		PW_INPUT,
		// added in 2.0
		CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		SIN_OUTPUT,
		TRI_OUTPUT,
		SAW_OUTPUT,
		SQR_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(PHASE_LIGHT, 3),
		INVERT_LIGHT,
		OFFSET_LIGHT,
		NUM_LIGHTS
	};

	bool offset = false;
	bool invert = false;

	float_4 phases[4];
	dsp::TSchmittTrigger<float_4> clockTriggers[4];
	dsp::TSchmittTrigger<float_4> resetTriggers[4];
	dsp::SchmittTrigger clockTrigger;
	float clockFreq = 1.f;
	dsp::Timer clockTimer;

	dsp::BooleanTrigger offsetTrigger;
	dsp::BooleanTrigger invertTrigger;
	dsp::ClockDivider lightDivider;

	LFO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configButton(OFFSET_PARAM, "Offset 0-10V");
		configButton(INVERT_PARAM, "Invert");

		struct FrequencyQuantity : ParamQuantity {
			float getDisplayValue() override {
				LFO* module = reinterpret_cast<LFO*>(this->module);
				if (module->clockFreq == 2.f) {
					unit = " Hz";
					displayMultiplier = 1.f;
				}
				else {
					unit = "x";
					displayMultiplier = 1 / 2.f;
				}
				return ParamQuantity::getDisplayValue();
			}
		};
		configParam<FrequencyQuantity>(FREQ_PARAM, -8.f, 10.f, 1.f, "Frequency", " Hz", 2, 1);
		configParam(FM_PARAM, -1.f, 1.f, 0.f, "Frequency modulation", "%", 0.f, 100.f);
		getParamQuantity(FM_PARAM)->randomizeEnabled = false;
		configParam(PW_PARAM, 0.01f, 0.99f, 0.5f, "Pulse width", "%", 0.f, 100.f);
		configParam(PWM_PARAM, -1.f, 1.f, 0.f, "Pulse width modulation", "%", 0.f, 100.f);
		getParamQuantity(PWM_PARAM)->randomizeEnabled = false;

		configInput(FM_INPUT, "Frequency modulation");
		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(PW_INPUT, "Pulse width modulation");

		configOutput(SIN_OUTPUT, "Sine");
		configOutput(TRI_OUTPUT, "Triangle");
		configOutput(SAW_OUTPUT, "Sawtooth");
		configOutput(SQR_OUTPUT, "Square");

		configLight(PHASE_LIGHT, "Phase");

		lightDivider.setDivision(16);
		onReset();
	}

	void onReset() override {
		offset = false;
		invert = false;
		for (int c = 0; c < 16; c += 4) {
			phases[c / 4] = 0.f;
		}
		clockFreq = 1.f;
		clockTimer.reset();
	}

	void process(const ProcessArgs& args) override {
		float freqParam = params[FREQ_PARAM].getValue();
		float fmParam = params[FM_PARAM].getValue();
		float pwParam = params[PW_PARAM].getValue();
		float pwmParam = params[PWM_PARAM].getValue();

		// Buttons
		if (offsetTrigger.process(params[OFFSET_PARAM].getValue() > 0.f)) {
			offset ^= true;
		}
		if (invertTrigger.process(params[INVERT_PARAM].getValue() > 0.f)) {
			invert ^= true;
		}

		// Clock
		if (inputs[CLOCK_INPUT].isConnected()) {
			clockTimer.process(args.sampleTime);

			if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f)) {
				float clockFreq = 1.f / clockTimer.getTime();
				clockTimer.reset();
				if (0.001f <= clockFreq && clockFreq <= 1000.f) {
					this->clockFreq = clockFreq;
				}
			}
		}
		else {
			// Default frequency when clock is unpatched
			clockFreq = 2.f;
		}

		int channels = std::max(1, inputs[FM_INPUT].getChannels());

		for (int c = 0; c < channels; c += 4) {
			// Pitch and frequency
			float_4 pitch = freqParam;
			pitch += inputs[FM_INPUT].getVoltageSimd<float_4>(c) * fmParam;
			float_4 freq = clockFreq / 2.f * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);

			// Pulse width
			float_4 pw = pwParam;
			pw += inputs[PW_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f * pwmParam;
			pw = clamp(pw, 0.01f, 0.99f);

			// Advance phase
			float_4 deltaPhase = simd::fmin(freq * args.sampleTime, 0.5f);
			phases[c / 4] += deltaPhase;
			phases[c / 4] -= simd::trunc(phases[c / 4]);

			// Reset
			float_4 reset = inputs[RESET_INPUT].getPolyVoltageSimd<float_4>(c);
			float_4 resetTriggered = resetTriggers[c / 4].process(reset, 0.1f, 2.f);
			phases[c / 4] = simd::ifelse(resetTriggered, 0.f, phases[c / 4]);

			// Sine
			if (outputs[SIN_OUTPUT].isConnected()) {
				float_4 p = phases[c / 4];
				if (offset)
					p -= 0.25f;
				float_4 v = simd::sin(2 * M_PI * p);
				if (invert)
					v *= -1.f;
				if (offset)
					v += 1.f;
				outputs[SIN_OUTPUT].setVoltageSimd(5.f * v, c);
			}
			// Triangle
			if (outputs[TRI_OUTPUT].isConnected()) {
				float_4 p = phases[c / 4];
				if (!offset)
					p += 0.25f;
				float_4 v = 4.f * simd::fabs(p - simd::round(p)) - 1.f;
				if (invert)
					v *= -1.f;
				if (offset)
					v += 1.f;
				outputs[TRI_OUTPUT].setVoltageSimd(5.f * v, c);
			}
			// Sawtooth
			if (outputs[SAW_OUTPUT].isConnected()) {
				float_4 p = phases[c / 4];
				if (offset)
					p -= 0.5f;
				float_4 v = 2.f * (p - simd::round(p));
				if (invert)
					v *= -1.f;
				if (offset)
					v += 1.f;
				outputs[SAW_OUTPUT].setVoltageSimd(5.f * v, c);
			}
			// Square
			if (outputs[SQR_OUTPUT].isConnected()) {
				float_4 v = simd::ifelse(phases[c / 4] < pw, 1.f, -1.f);
				if (invert)
					v *= -1.f;
				if (offset)
					v += 1.f;
				outputs[SQR_OUTPUT].setVoltageSimd(5.f * v, c);
			}
		}

		outputs[SIN_OUTPUT].setChannels(channels);
		outputs[TRI_OUTPUT].setChannels(channels);
		outputs[SAW_OUTPUT].setChannels(channels);
		outputs[SQR_OUTPUT].setChannels(channels);

		// Light
		if (lightDivider.process()) {
			if (channels == 1) {
				float b = 1.f - phases[0][0];
				lights[PHASE_LIGHT + 0].setSmoothBrightness(b, args.sampleTime * lightDivider.getDivision());
				lights[PHASE_LIGHT + 1].setSmoothBrightness(b, args.sampleTime * lightDivider.getDivision());
				lights[PHASE_LIGHT + 2].setBrightness(0.f);
			}
			else {
				lights[PHASE_LIGHT + 0].setBrightness(0.f);
				lights[PHASE_LIGHT + 1].setBrightness(0.f);
				lights[PHASE_LIGHT + 2].setBrightness(1.f);
			}
			lights[OFFSET_LIGHT].setBrightness(offset);
			lights[INVERT_LIGHT].setBrightness(invert);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		// offset
		json_object_set_new(rootJ, "offset", json_boolean(offset));
		// invert
		json_object_set_new(rootJ, "invert", json_boolean(invert));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// offset
		json_t* offsetJ = json_object_get(rootJ, "offset");
		if (offsetJ)
			offset = json_boolean_value(offsetJ);
		// invert
		json_t* invertJ = json_object_get(rootJ, "invert");
		if (invertJ)
			invert = json_boolean_value(invertJ);
	}

	void paramsFromJson(json_t* rootJ) override {
		Module::paramsFromJson(rootJ);
		// In <2.0, OFFSET_PARAM and INVERT_PARAM were toggle switches instead of momentary buttons, so if params are on after deserializing, set boolean states instead.
		if (params[OFFSET_PARAM].getValue() > 0.f) {
			offset = true;
			params[OFFSET_PARAM].setValue(0.f);
		}
		if (params[INVERT_PARAM].getValue() > 0.f) {
			invert = true;
			params[INVERT_PARAM].setValue(0.f);
		}
	}
};


struct LFOWidget : ModuleWidget {
	LFOWidget(LFO* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/LFO.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(22.902, 29.803)), module, LFO::FREQ_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(22.861, 56.388)), module, LFO::PW_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.604, 80.603)), module, LFO::FM_PARAM));
		addParam(createLightParamCentered<LEDLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(17.441, 80.603)), module, LFO::INVERT_PARAM, LFO::INVERT_LIGHT));
		addParam(createLightParamCentered<LEDLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(28.279, 80.603)), module, LFO::OFFSET_PARAM, LFO::OFFSET_LIGHT));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(39.116, 80.603)), module, LFO::PWM_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.604, 96.859)), module, LFO::FM_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.441, 96.859)), module, LFO::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(28.279, 96.819)), module, LFO::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(39.116, 96.819)), module, LFO::PW_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.604, 113.115)), module, LFO::SIN_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(17.441, 113.115)), module, LFO::TRI_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(28.279, 113.115)), module, LFO::SAW_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.116, 113.115)), module, LFO::SQR_OUTPUT));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(31.085, 16.428)), module, LFO::PHASE_LIGHT));
	}
};


Model* modelLFO = createModel<LFO, LFOWidget>("LFO");
