#include "plugin.hpp"


BINARY(src_sawTable_bin);
BINARY(src_triTable_bin);


template <int OVERSAMPLE, int QUALITY>
struct VoltageControlledOscillator {
	bool analog = false;
	bool soft = false;
	float lastSyncValue = 0.f;
	float phase = 0.f;
	float freq;
	float pw = 0.5f;
	float pitch;
	bool syncEnabled = false;
	bool syncDirection = false;

	dsp::Decimator<OVERSAMPLE, QUALITY> sinDecimator;
	dsp::Decimator<OVERSAMPLE, QUALITY> triDecimator;
	dsp::Decimator<OVERSAMPLE, QUALITY> sawDecimator;
	dsp::Decimator<OVERSAMPLE, QUALITY> sqrDecimator;
	dsp::RCFilter sqrFilter;

	// For analog detuning effect
	float pitchSlew = 0.f;
	int pitchSlewIndex = 0;

	float sinBuffer[OVERSAMPLE] = {};
	float triBuffer[OVERSAMPLE] = {};
	float sawBuffer[OVERSAMPLE] = {};
	float sqrBuffer[OVERSAMPLE] = {};

	void setPitch(float pitchKnob, float pitchCv) {
		// Compute frequency
		pitch = pitchKnob;
		if (analog) {
			// Apply pitch slew
			const float pitchSlewAmount = 3.f;
			pitch += pitchSlew * pitchSlewAmount;
		}
		pitch += pitchCv;
		// Note C4
		freq = dsp::FREQ_C4 * std::pow(2.f, pitch / 12.f);
	}
	void setPulseWidth(float pulseWidth) {
		const float pwMin = 0.01f;
		pw = clamp(pulseWidth, pwMin, 1.f - pwMin);
	}

	void process(float deltaTime, float syncValue) {
		if (analog) {
			// Adjust pitch slew
			if (++pitchSlewIndex > 32) {
				const float pitchSlewTau = 100.f; // Time constant for leaky integrator in seconds
				pitchSlew += ((2.f * random::uniform() - 1.f) - pitchSlew / pitchSlewTau) * deltaTime;
				pitchSlewIndex = 0;
			}
		}

		// Advance phase
		float deltaPhase = clamp(freq * deltaTime, 1e-6f, 0.5f);

		// Detect sync
		int syncIndex = -1; // Index in the oversample loop where sync occurs [0, OVERSAMPLE)
		float syncCrossing = 0.f; // Offset that sync occurs [0.f, 1.f)
		if (syncEnabled) {
			syncValue -= 0.01f;
			if (syncValue > 0.f && lastSyncValue <= 0.f) {
				float deltaSync = syncValue - lastSyncValue;
				syncCrossing = 1.f - syncValue / deltaSync;
				syncCrossing *= OVERSAMPLE;
				syncIndex = (int)syncCrossing;
				syncCrossing -= syncIndex;
			}
			lastSyncValue = syncValue;
		}

		if (syncDirection)
			deltaPhase *= -1.f;

		sqrFilter.setCutoff(40.f * deltaTime);

		for (int i = 0; i < OVERSAMPLE; i++) {
			if (syncIndex == i) {
				if (soft) {
					syncDirection = !syncDirection;
					deltaPhase *= -1.f;
				}
				else {
					// phase = syncCrossing * deltaPhase / OVERSAMPLE;
					phase = 0.f;
				}
			}

			if (analog) {
				// Quadratic approximation of sine, slightly richer harmonics
				if (phase < 0.5f)
					sinBuffer[i] = 1.f - 16.f * std::pow(phase - 0.25f, 2);
				else
					sinBuffer[i] = -1.f + 16.f * std::pow(phase - 0.75f, 2);
				sinBuffer[i] *= 1.08f;
			}
			else {
				sinBuffer[i] = std::sin(2.f*M_PI * phase);
			}
			if (analog) {
				triBuffer[i] = 1.25f * interpolateLinear((const float*) BINARY_START(src_triTable_bin), phase * 2047.f);
			}
			else {
				if (phase < 0.25f)
					triBuffer[i] = 4.f * phase;
				else if (phase < 0.75f)
					triBuffer[i] = 2.f - 4.f * phase;
				else
					triBuffer[i] = -4.f + 4.f * phase;
			}
			if (analog) {
				sawBuffer[i] = 1.66f * interpolateLinear((const float*) BINARY_START(src_sawTable_bin), phase * 2047.f);
			}
			else {
				if (phase < 0.5f)
					sawBuffer[i] = 2.f * phase;
				else
					sawBuffer[i] = -2.f + 2.f * phase;
			}
			sqrBuffer[i] = (phase < pw) ? 1.f : -1.f;
			if (analog) {
				// Simply filter here
				sqrFilter.process(sqrBuffer[i]);
				sqrBuffer[i] = 0.71f * sqrFilter.highpass();
			}

			// Advance phase
			phase += deltaPhase / OVERSAMPLE;
			phase = eucMod(phase, 1.f);
		}
	}

	float sin() {
		return sinDecimator.process(sinBuffer);
	}
	float tri() {
		return triDecimator.process(triBuffer);
	}
	float saw() {
		return sawDecimator.process(sawBuffer);
	}
	float sqr() {
		return sqrDecimator.process(sqrBuffer);
	}
	float light() {
		return std::sin(2*M_PI * phase);
	}
};


struct VCO : Module {
	enum ParamIds {
		MODE_PARAM,
		SYNC_PARAM,
		FREQ_PARAM,
		FINE_PARAM,
		FM_PARAM,
		PW_PARAM,
		PWM_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,
		FM_INPUT,
		SYNC_INPUT,
		PW_INPUT,
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
		PHASE_POS_LIGHT,
		PHASE_NEG_LIGHT,
		NUM_LIGHTS
	};

	VoltageControlledOscillator<16, 16> oscillator;

	VCO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(MODE_PARAM, 0.f, 1.f, 1.f, "Analog mode");
		configParam(SYNC_PARAM, 0.f, 1.f, 1.f, "Hard sync");
		configParam(FREQ_PARAM, -54.f, 54.f, 0.f, "Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency");
		configParam(FM_PARAM, 0.f, 1.f, 0.f, "Frequency modulation", "%", 0.f, 100.f);
		configParam(PW_PARAM, 0.f, 1.f, 0.5f, "Pulse width", "%", 0.f, 100.f);
		configParam(PWM_PARAM, 0.f, 1.f, 0.f, "Pulse width modulation", "%", 0.f, 100.f);
	}

	void process(const ProcessArgs &args) override {
		oscillator.analog = params[MODE_PARAM].getValue() > 0.f;
		oscillator.soft = params[SYNC_PARAM].getValue() <= 0.f;

		float pitchFine = 3.f * dsp::quadraticBipolar(params[FINE_PARAM].getValue());
		float pitchCv = 12.f * inputs[PITCH_INPUT].getVoltage();
		if (inputs[FM_INPUT].isConnected()) {
			pitchCv += dsp::quadraticBipolar(params[FM_PARAM].getValue()) * 12.f * inputs[FM_INPUT].getVoltage();
		}
		oscillator.setPitch(params[FREQ_PARAM].getValue(), pitchFine + pitchCv);
		oscillator.setPulseWidth(params[PW_PARAM].getValue() + params[PWM_PARAM].getValue() * inputs[PW_INPUT].getVoltage() / 10.f);
		oscillator.syncEnabled = inputs[SYNC_INPUT].isConnected();

		oscillator.process(args.sampleTime, inputs[SYNC_INPUT].getVoltage());

		// Set output
		if (outputs[SIN_OUTPUT].isConnected())
			outputs[SIN_OUTPUT].setVoltage(5.f * oscillator.sin());
		if (outputs[TRI_OUTPUT].isConnected())
			outputs[TRI_OUTPUT].setVoltage(5.f * oscillator.tri());
		if (outputs[SAW_OUTPUT].isConnected())
			outputs[SAW_OUTPUT].setVoltage(5.f * oscillator.saw());
		if (outputs[SQR_OUTPUT].isConnected())
			outputs[SQR_OUTPUT].setVoltage(5.f * oscillator.sqr());

		lights[PHASE_POS_LIGHT].setSmoothBrightness(oscillator.light(), args.sampleTime);
		lights[PHASE_NEG_LIGHT].setSmoothBrightness(-oscillator.light(), args.sampleTime);
	}
};


struct VCOWidget : ModuleWidget {
	VCOWidget(VCO *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/VCO-1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParam<CKSS>(Vec(15, 77), module, VCO::MODE_PARAM));
		addParam(createParam<CKSS>(Vec(119, 77), module, VCO::SYNC_PARAM));

		addParam(createParam<RoundHugeBlackKnob>(Vec(47, 61), module, VCO::FREQ_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(23, 143), module, VCO::FINE_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(91, 143), module, VCO::PW_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(23, 208), module, VCO::FM_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(91, 208), module, VCO::PWM_PARAM));

		addInput(createInput<PJ301MPort>(Vec(11, 276), module, VCO::PITCH_INPUT));
		addInput(createInput<PJ301MPort>(Vec(45, 276), module, VCO::FM_INPUT));
		addInput(createInput<PJ301MPort>(Vec(80, 276), module, VCO::SYNC_INPUT));
		addInput(createInput<PJ301MPort>(Vec(114, 276), module, VCO::PW_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(11, 320), module, VCO::SIN_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(45, 320), module, VCO::TRI_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(80, 320), module, VCO::SAW_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(114, 320), module, VCO::SQR_OUTPUT));

		addChild(createLight<SmallLight<GreenRedLight>>(Vec(99, 42.5f), module, VCO::PHASE_POS_LIGHT));
	}
};


Model *modelVCO = createModel<VCO, VCOWidget>("VCO");


struct VCO2 : Module {
	enum ParamIds {
		MODE_PARAM,
		SYNC_PARAM,
		FREQ_PARAM,
		WAVE_PARAM,
		FM_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FM_INPUT,
		SYNC_INPUT,
		WAVE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		PHASE_POS_LIGHT,
		PHASE_NEG_LIGHT,
		NUM_LIGHTS
	};

	VoltageControlledOscillator<8, 8> oscillator;

	VCO2() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(MODE_PARAM, 0.f, 1.f, 1.f, "Analog mode");
		configParam(SYNC_PARAM, 0.f, 1.f, 1.f, "Hard sync");
		configParam(FREQ_PARAM, -54.f, 54.f, 0.f, "Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(WAVE_PARAM, 0.f, 3.f, 1.5f, "Wave");
		configParam(FM_PARAM, 0.f, 1.f, 0.f, "Frequency modulation", "%", 0.f, 100.f);
	}

	void process(const ProcessArgs &args) override {
		float deltaTime = args.sampleTime;
		oscillator.analog = params[MODE_PARAM].getValue() > 0.f;
		oscillator.soft = params[SYNC_PARAM].getValue() <= 0.f;

		float pitchCv = params[FREQ_PARAM].getValue() + dsp::quadraticBipolar(params[FM_PARAM].getValue()) * 12.f * inputs[FM_INPUT].getVoltage();
		oscillator.setPitch(0.f, pitchCv);
		oscillator.syncEnabled = inputs[SYNC_INPUT].isConnected();

		oscillator.process(deltaTime, inputs[SYNC_INPUT].getVoltage());

		// Set output
		float wave = clamp(params[WAVE_PARAM].getValue() + inputs[WAVE_INPUT].getVoltage(), 0.f, 3.f);
		float out;
		if (wave < 1.f)
			out = crossfade(oscillator.sin(), oscillator.tri(), wave);
		else if (wave < 2.f)
			out = crossfade(oscillator.tri(), oscillator.saw(), wave - 1.f);
		else
			out = crossfade(oscillator.saw(), oscillator.sqr(), wave - 2.f);
		outputs[OUT_OUTPUT].setVoltage(5.f * out);

		lights[PHASE_POS_LIGHT].setSmoothBrightness(oscillator.light(), deltaTime);
		lights[PHASE_NEG_LIGHT].setSmoothBrightness(-oscillator.light(), deltaTime);
	}
};




struct VCO2Widget : ModuleWidget {
	VCO2Widget(VCO2 *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/VCO-2.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParam<CKSS>(Vec(62, 150), module, VCO2::MODE_PARAM));
		addParam(createParam<CKSS>(Vec(62, 215), module, VCO2::SYNC_PARAM));

		addParam(createParam<RoundHugeBlackKnob>(Vec(17, 60), module, VCO2::FREQ_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(12, 143), module, VCO2::WAVE_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(12, 208), module, VCO2::FM_PARAM));

		addInput(createInput<PJ301MPort>(Vec(11, 276), module, VCO2::FM_INPUT));
		addInput(createInput<PJ301MPort>(Vec(54, 276), module, VCO2::SYNC_INPUT));
		addInput(createInput<PJ301MPort>(Vec(11, 320), module, VCO2::WAVE_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(54, 320), module, VCO2::OUT_OUTPUT));

		addChild(createLight<SmallLight<GreenRedLight>>(Vec(68, 42.5f), module, VCO2::PHASE_POS_LIGHT));
	}
};


Model *modelVCO2 = createModel<VCO2, VCO2Widget>("VCO2");
