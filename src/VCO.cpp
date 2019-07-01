#include "plugin.hpp"


using simd::float_4;


// Accurate only on [0, 1]
template <typename T>
T sin2pi_pade_05_7_6(T x) {
	x -= 0.5f;
	return (T(-6.28319) * x + T(35.353) * simd::pow(x, 3) - T(44.9043) * simd::pow(x, 5) + T(16.0951) * simd::pow(x, 7))
		/ (1 + T(0.953136) * simd::pow(x, 2) + T(0.430238) * simd::pow(x, 4) + T(0.0981408) * simd::pow(x, 6));
}

template <typename T>
T sin2pi_pade_05_5_4(T x) {
	x -= 0.5f;
	return (T(-6.283185307) * x + T(33.19863968) * simd::pow(x, 3) - T(32.44191367) * simd::pow(x, 5))
		/ (1 + T(1.296008659) * simd::pow(x, 2) + T(0.7028072946) * simd::pow(x, 4));
}

template <typename T>
T expCurve(T x) {
	return (3 + x * (-13 + 5 * x)) / (3 + 2 * x);
}


template <int OVERSAMPLE, int QUALITY, typename T>
struct VoltageControlledOscillator {
	bool analog = false;
	bool soft = false;
	bool syncEnabled = false;
	// For optimizing in serial code
	int channels = 0;

	T lastSyncValue = 0.f;
	T phase = 0.f;
	T freq;
	T pulseWidth = 0.5f;
	T syncDirection = 1.f;

	dsp::TRCFilter<T> sqrFilter;

	dsp::MinBlepGenerator<QUALITY, OVERSAMPLE, T> sqrMinBlep;
	dsp::MinBlepGenerator<QUALITY, OVERSAMPLE, T> sawMinBlep;
	dsp::MinBlepGenerator<QUALITY, OVERSAMPLE, T> triMinBlep;
	dsp::MinBlepGenerator<QUALITY, OVERSAMPLE, T> sinMinBlep;

	T sqrValue = 0.f;
	T sawValue = 0.f;
	T triValue = 0.f;
	T sinValue = 0.f;

	void setPitch(T pitch) {
		freq = dsp::FREQ_C4 * simd::pow(2.f, pitch);
	}

	void setPulseWidth(T pulseWidth) {
		const float pwMin = 0.01f;
		this->pulseWidth = simd::clamp(pulseWidth, pwMin, 1.f - pwMin);
	}

	void process(float deltaTime, T syncValue) {
		// Advance phase
		T deltaPhase = simd::clamp(freq * deltaTime, 1e-6f, 0.35f);
		if (soft) {
			// Reverse direction
			deltaPhase *= syncDirection;
		}
		else {
			// Reset back to forward
			syncDirection = 1.f;
		}
		phase += deltaPhase;
		// Wrap phase
		phase -= simd::floor(phase);

		// Jump sqr when crossing 0, or 1 if backwards
		T wrapPhase = (syncDirection == -1.f) & 1.f;
		T wrapCrossing = (wrapPhase - (phase - deltaPhase)) / deltaPhase;
		int wrapMask = simd::movemask((0 < wrapCrossing) & (wrapCrossing <= 1.f));
		if (wrapMask) {
			for (int i = 0; i < channels; i++) {
				if (wrapMask & (1 << i)) {
					T mask = simd::movemaskInverse<T>(1 << i);
					float p = wrapCrossing[i] - 1.f;
					T x = mask & (2.f * syncDirection);
					sqrMinBlep.insertDiscontinuity(p, x);
				}
			}
		}

		// Jump sqr when crossing `pulseWidth`
		T pulseCrossing = (pulseWidth - (phase - deltaPhase)) / deltaPhase;
		int pulseMask = simd::movemask((0 < pulseCrossing) & (pulseCrossing <= 1.f));
		if (pulseMask) {
			for (int i = 0; i < channels; i++) {
				if (pulseMask & (1 << i)) {
					T mask = simd::movemaskInverse<T>(1 << i);
					float p = pulseCrossing[i] - 1.f;
					T x = mask & (-2.f * syncDirection);
					sqrMinBlep.insertDiscontinuity(p, x);
				}
			}
		}

		// Jump saw when crossing 0.5
		T halfCrossing = (0.5f - (phase - deltaPhase)) / deltaPhase;
		int halfMask = simd::movemask((0 < halfCrossing) & (halfCrossing <= 1.f));
		if (halfMask) {
			for (int i = 0; i < channels; i++) {
				if (halfMask & (1 << i)) {
					T mask = simd::movemaskInverse<T>(1 << i);
					float p = halfCrossing[i] - 1.f;
					T x = mask & (-2.f * syncDirection);
					sawMinBlep.insertDiscontinuity(p, x);
				}
			}
		}

		// Detect sync
		// Might be NAN or outside of [0, 1) range
		if (syncEnabled) {
			T deltaSync = syncValue - lastSyncValue;
			T syncCrossing = -lastSyncValue / deltaSync;
			lastSyncValue = syncValue;
			T sync = (0.f < syncCrossing) & (syncCrossing <= 1.f) & (syncValue >= 0.f);
			int syncMask = simd::movemask(sync);
			if (syncMask) {
				if (soft) {
					syncDirection = simd::ifelse(sync, -syncDirection, syncDirection);
				}
				else {
					T newPhase = simd::ifelse(sync, syncCrossing * deltaPhase, phase);
					// Insert minBLEP for sync
					for (int i = 0; i < channels; i++) {
						if (syncMask & (1 << i)) {
							T mask = simd::movemaskInverse<T>(1 << i);
							float p = syncCrossing[i] - 1.f;
							T x;
							x = mask & (sqr(newPhase) - sqr(phase));
							sqrMinBlep.insertDiscontinuity(p, x);
							x = mask & (saw(newPhase) - saw(phase));
							sawMinBlep.insertDiscontinuity(p, x);
							x = mask & (tri(newPhase) - tri(phase));
							triMinBlep.insertDiscontinuity(p, x);
							x = mask & (sin(newPhase) - sin(phase));
							sinMinBlep.insertDiscontinuity(p, x);
						}
					}
					phase = newPhase;
				}
			}
		}

		// Square
		sqrValue = sqr(phase);
		sqrValue += sqrMinBlep.process();

		if (analog) {
			sqrFilter.setCutoffFreq(20.f * deltaTime);
			sqrFilter.process(sqrValue);
			sqrValue = sqrFilter.highpass() * 0.95f;
		}

		// Saw
		sawValue = saw(phase);
		sawValue += sawMinBlep.process();

		// Tri
		triValue = tri(phase);
		triValue += triMinBlep.process();

		// Sin
		sinValue = sin(phase);
		sinValue += sinMinBlep.process();
	}

	T sin(T phase) {
		T v;
		if (analog) {
			// Quadratic approximation of sine, slightly richer harmonics
			T halfPhase = (phase < 0.5f);
			T x = phase - simd::ifelse(halfPhase, 0.25f, 0.75f);
			v = 1.f - 16.f * simd::pow(x, 2);
			v *= simd::ifelse(halfPhase, 1.f, -1.f);
		}
		else {
			v = sin2pi_pade_05_5_4(phase);
			// v = sin2pi_pade_05_7_6(phase);
			// v = simd::sin(2 * T(M_PI) * phase);
		}
		return v;
	}
	T sin() {
		return sinValue;
	}

	T tri(T phase) {
		T v;
		if (analog) {
			T x = phase + 0.25f;
			x -= simd::trunc(x);
			T halfX = (x < 0.5f);
			x = 2 * x - simd::ifelse(halfX, 0.f, 1.f);
			v = expCurve(x) * simd::ifelse(halfX, 1.f, -1.f);
		}
		else {
			v = 1 - 4 * simd::fmin(simd::fabs(phase - 0.25f), simd::fabs(phase - 1.25f));
		}
		return v;
	}
	T tri() {
		return triValue;
	}

	T saw(T phase) {
		T v;
		T x = phase + 0.5f;
		x -= simd::trunc(x);
		if (analog) {
			v = -expCurve(x);
		}
		else {
			v = 2 * x - 1;
		}
		return v;
	}
	T saw() {
		return sawValue;
	}

	T sqr(T phase) {
		T v = simd::ifelse(phase < pulseWidth, 1.f, -1.f);
		return v;
	}
	T sqr() {
		return sqrValue;
	}

	T light() {
		return simd::sin(2 * T(M_PI) * phase);
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
		ENUMS(PHASE_LIGHT, 3),
		NUM_LIGHTS
	};

	VoltageControlledOscillator<16, 16, float_4> oscillators[4];
	dsp::ClockDivider lightDivider;

	VCO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(MODE_PARAM, 0.f, 1.f, 1.f, "Analog mode");
		configParam(SYNC_PARAM, 0.f, 1.f, 1.f, "Hard sync");
		configParam(FREQ_PARAM, -54.f, 54.f, 0.f, "Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency");
		configParam(FM_PARAM, 0.f, 1.f, 0.f, "Frequency modulation", "%", 0.f, 100.f);
		configParam(PW_PARAM, 0.01f, 0.99f, 0.5f, "Pulse width", "%", 0.f, 100.f);
		configParam(PWM_PARAM, 0.f, 1.f, 0.f, "Pulse width modulation", "%", 0.f, 100.f);
		lightDivider.setDivision(16);
	}

	void process(const ProcessArgs &args) override {
		float freqParam = params[FREQ_PARAM].getValue() / 12.f;
		freqParam += dsp::quadraticBipolar(params[FINE_PARAM].getValue()) * 3.f / 12.f;
		float fmParam = dsp::quadraticBipolar(params[FM_PARAM].getValue());

		int channels = std::max(inputs[PITCH_INPUT].getChannels(), 1);

		for (int c = 0; c < channels; c += 4) {
			auto *oscillator = &oscillators[c / 4];
			oscillator->channels = std::min(channels - c, 4);
			oscillator->analog = params[MODE_PARAM].getValue() > 0.f;
			oscillator->soft = params[SYNC_PARAM].getValue() <= 0.f;

			float_4 pitch = freqParam;
			pitch += inputs[PITCH_INPUT].getVoltageSimd<float_4>(c);
			if (inputs[FM_INPUT].isConnected()) {
				pitch += fmParam * inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c);
			}
			oscillator->setPitch(pitch);
			oscillator->setPulseWidth(params[PW_PARAM].getValue() + params[PWM_PARAM].getValue() * inputs[PW_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f);

			oscillator->syncEnabled = inputs[SYNC_INPUT].isConnected();
			oscillator->process(args.sampleTime, inputs[SYNC_INPUT].getPolyVoltageSimd<float_4>(c));

			// Set output
			if (outputs[SIN_OUTPUT].isConnected())
				outputs[SIN_OUTPUT].setVoltageSimd(5.f * oscillator->sin(), c);
			if (outputs[TRI_OUTPUT].isConnected())
				outputs[TRI_OUTPUT].setVoltageSimd(5.f * oscillator->tri(), c);
			if (outputs[SAW_OUTPUT].isConnected())
				outputs[SAW_OUTPUT].setVoltageSimd(5.f * oscillator->saw(), c);
			if (outputs[SQR_OUTPUT].isConnected())
				outputs[SQR_OUTPUT].setVoltageSimd(5.f * oscillator->sqr(), c);
		}

		outputs[SIN_OUTPUT].setChannels(channels);
		outputs[TRI_OUTPUT].setChannels(channels);
		outputs[SAW_OUTPUT].setChannels(channels);
		outputs[SQR_OUTPUT].setChannels(channels);

		// Light
		if (lightDivider.process()) {
			if (channels == 1) {
				float lightValue = oscillators[0].light()[0];
				lights[PHASE_LIGHT + 0].setSmoothBrightness(-lightValue, args.sampleTime * lightDivider.getDivision());
				lights[PHASE_LIGHT + 1].setSmoothBrightness(lightValue, args.sampleTime * lightDivider.getDivision());
				lights[PHASE_LIGHT + 2].setBrightness(0.f);
			}
			else {
				lights[PHASE_LIGHT + 0].setBrightness(0.f);
				lights[PHASE_LIGHT + 1].setBrightness(0.f);
				lights[PHASE_LIGHT + 2].setBrightness(1.f);
			}
		}
	}
};


struct VCOWidget : ModuleWidget {
	VCOWidget(VCO *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/VCO-1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

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

		addChild(createLight<SmallLight<RedGreenBlueLight>>(Vec(99, 42.5f), module, VCO::PHASE_LIGHT));
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
		ENUMS(PHASE_LIGHT, 3),
		NUM_LIGHTS
	};

	VoltageControlledOscillator<8, 8, float_4> oscillators[4];
	dsp::ClockDivider lightDivider;

	VCO2() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(MODE_PARAM, 0.f, 1.f, 1.f, "Analog mode");
		configParam(SYNC_PARAM, 0.f, 1.f, 1.f, "Hard sync");
		configParam(FREQ_PARAM, -54.f, 54.f, 0.f, "Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(WAVE_PARAM, 0.f, 3.f, 1.5f, "Wave");
		configParam(FM_PARAM, 0.f, 1.f, 0.f, "Frequency modulation", "%", 0.f, 100.f);
		lightDivider.setDivision(16);
	}

	void process(const ProcessArgs &args) override {
		float freqParam = params[FREQ_PARAM].getValue() / 12.f;
		float fmParam = dsp::quadraticBipolar(params[FM_PARAM].getValue());
		float waveParam = params[WAVE_PARAM].getValue();

		int channels = std::max(inputs[FM_INPUT].getChannels(), 1);

		for (int c = 0; c < channels; c += 4) {
			auto *oscillator = &oscillators[c / 4];
			oscillator->channels = std::min(channels - c, 4);
			oscillator->analog = (params[MODE_PARAM].getValue() > 0.f);
			oscillator->soft = (params[SYNC_PARAM].getValue() <= 0.f);

			float_4 pitch = freqParam;
			pitch += fmParam * inputs[FM_INPUT].getVoltageSimd<float_4>(c);
			oscillator->setPitch(pitch);

			oscillator->syncEnabled = inputs[SYNC_INPUT].isConnected();
			oscillator->process(args.sampleTime, inputs[SYNC_INPUT].getPolyVoltageSimd<float_4>(c));

			// Outputs
			if (outputs[OUT_OUTPUT].isConnected()) {
				float_4 wave = simd::clamp(waveParam + inputs[WAVE_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f * 3.f, 0.f, 3.f);
				float_4 v = 0.f;
				v += oscillator->sin() * simd::fmax(0.f, 1.f - simd::fabs(wave - 0.f));
				v += oscillator->tri() * simd::fmax(0.f, 1.f - simd::fabs(wave - 1.f));
				v += oscillator->saw() * simd::fmax(0.f, 1.f - simd::fabs(wave - 2.f));
				v += oscillator->sqr() * simd::fmax(0.f, 1.f - simd::fabs(wave - 3.f));
				outputs[OUT_OUTPUT].setVoltageSimd(5.f * v, c);
			}
		}

		outputs[OUT_OUTPUT].setChannels(channels);

		// Light
		if (lightDivider.process()) {
			if (channels == 1) {
				float lightValue = oscillators[0].light()[0];
				lights[PHASE_LIGHT + 0].setSmoothBrightness(-lightValue, args.sampleTime * lightDivider.getDivision());
				lights[PHASE_LIGHT + 1].setSmoothBrightness(lightValue, args.sampleTime * lightDivider.getDivision());
				lights[PHASE_LIGHT + 2].setBrightness(0.f);
			}
			else {
				lights[PHASE_LIGHT + 0].setBrightness(0.f);
				lights[PHASE_LIGHT + 1].setBrightness(0.f);
				lights[PHASE_LIGHT + 2].setBrightness(1.f);
			}
		}
	}
};




struct VCO2Widget : ModuleWidget {
	VCO2Widget(VCO2 *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/VCO-2.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

		addParam(createParam<CKSS>(Vec(62, 150), module, VCO2::MODE_PARAM));
		addParam(createParam<CKSS>(Vec(62, 215), module, VCO2::SYNC_PARAM));

		addParam(createParam<RoundHugeBlackKnob>(Vec(17, 60), module, VCO2::FREQ_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(12, 143), module, VCO2::WAVE_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(12, 208), module, VCO2::FM_PARAM));

		addInput(createInput<PJ301MPort>(Vec(11, 276), module, VCO2::FM_INPUT));
		addInput(createInput<PJ301MPort>(Vec(54, 276), module, VCO2::SYNC_INPUT));
		addInput(createInput<PJ301MPort>(Vec(11, 320), module, VCO2::WAVE_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(54, 320), module, VCO2::OUT_OUTPUT));

		addChild(createLight<SmallLight<RedGreenBlueLight>>(Vec(68, 42.5f), module, VCO2::PHASE_LIGHT));
	}
};


Model *modelVCO2 = createModel<VCO2, VCO2Widget>("VCO2");
