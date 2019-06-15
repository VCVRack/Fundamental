#include "plugin.hpp"


using simd::float_4;


BINARY(src_sawTable_bin);
BINARY(src_triTable_bin);


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


template <int OVERSAMPLE, int QUALITY, typename T>
struct VoltageControlledOscillator {
	bool analog = false;
	bool soft = false;
	T lastSyncValue = 0.f;
	T phase = 0.f;
	T freq;
	T pw = 0.5f;
	bool syncEnabled = false;
	T syncDirection = T::zero();

	dsp::Decimator<OVERSAMPLE, QUALITY, T> sinDecimator;
	dsp::Decimator<OVERSAMPLE, QUALITY, T> triDecimator;
	dsp::Decimator<OVERSAMPLE, QUALITY, T> sawDecimator;
	dsp::Decimator<OVERSAMPLE, QUALITY, T> sqrDecimator;
	dsp::TRCFilter<T> sqrFilter;

	// For analog detuning effect
	T pitchSlew = 0.f;
	int pitchSlewIndex = 0;

	T sinBuffer[OVERSAMPLE];
	T triBuffer[OVERSAMPLE];
	T sawBuffer[OVERSAMPLE];
	T sqrBuffer[OVERSAMPLE];

	void setPitch(T pitchKnob, T pitchCv) {
		// Compute frequency
		T pitch = pitchKnob + pitchCv;
		if (analog) {
			// Apply pitch slew
			const float pitchSlewAmount = 3.f;
			pitch += pitchSlew * pitchSlewAmount;
		}
		freq = dsp::FREQ_C4 * simd::pow(2.f, pitch / 12.f);
	}
	void setPulseWidth(T pulseWidth) {
		const float pwMin = 0.01f;
		pw = simd::clamp(pulseWidth, pwMin, 1.f - pwMin);
	}

	void process(float deltaTime, T syncValue) {
		if (analog) {
			// Adjust pitch slew
			if (++pitchSlewIndex > 32) {
				const float pitchSlewTau = 100.f; // Time constant for leaky integrator in seconds
				T r;
				for (int i = 0; i < T::size; i++) {
					r.s[i] = random::uniform();
				}
				pitchSlew += ((2.f * r - 1.f) - pitchSlew / pitchSlewTau) * deltaTime;
				pitchSlewIndex = 0;
			}
		}

		// Advance phase
		T deltaPhase = simd::clamp(freq * deltaTime, 1e-6f, 0.5f);
		if (!soft) {
			syncDirection = T::zero();
		}

		// Detect sync
		// Might be NAN or outside of [0, 1) range
		T syncCrossing = lastSyncValue / (lastSyncValue - syncValue);
		lastSyncValue = syncValue;


		for (int i = 0; i < OVERSAMPLE; i++) {
			// Reset if synced in this time interval
			T reset = (T(i) / OVERSAMPLE <= syncCrossing) & (syncCrossing < T(i + 1) / OVERSAMPLE);
			if (simd::movemask(reset)) {
				if (soft) {
					syncDirection = simd::ifelse(reset, ~syncDirection, syncDirection);
				}
				else {
					phase = simd::ifelse(reset, (syncCrossing - T(i) / OVERSAMPLE) * deltaPhase, phase);
					// This also works as a good approximation.
					// phase = 0.f;
				}
			}

			// Sin
			if (analog) {
				// Quadratic approximation of sine, slightly richer harmonics
				T halfPhase = (phase < 0.5f);
				T x = phase - simd::ifelse(halfPhase, 0.25f, 0.75f);
				sinBuffer[i] = 1.f - 16.f * simd::pow(x, 2);
				sinBuffer[i] *= simd::ifelse(halfPhase, 1.f, -1.f);
				// sinBuffer[i] *= 1.08f;
			}
			else {
				sinBuffer[i] = sin2pi_pade_05_5_4(phase);
				// sinBuffer[i] = sin2pi_pade_05_7_6(phase);
				// sinBuffer[i] = simd::sin(2 * T(M_PI) * phase);
			}

			// Tri
			if (analog) {
				const float *triTable = (const float*) BINARY_START(src_triTable_bin);
				T p = phase * (BINARY_SIZE(src_triTable_bin) / sizeof(float) - 1);
				simd::Vector<int32_t, T::size> index = p;
				p -= index;

				T v0, v1;
				for (int i = 0; i < T::size; i++) {
					v0.s[i] = triTable[index.s[i]];
					v1.s[i] = triTable[index.s[i] + 1];
				}
				triBuffer[i] = 1.129f * simd::crossfade(v0, v1, p);
			}
			else {
				triBuffer[i] = 1 - 4 * simd::fmin(simd::fabs(phase - 0.25f), simd::fabs(phase - 1.25f));
			}

			// Saw
			if (analog) {
				const float *sawTable = (const float*) BINARY_START(src_sawTable_bin);
				T p = phase * (BINARY_SIZE(src_sawTable_bin) / sizeof(float) - 1);
				simd::Vector<int32_t, T::size> index = p;
				p -= index;

				T v0, v1;
				for (int i = 0; i < T::size; i++) {
					v0.s[i] = sawTable[index.s[i]];
					v1.s[i] = sawTable[index.s[i] + 1];
				}
				sawBuffer[i] = 1.376f * simd::crossfade(v0, v1, p);
			}
			else {
				sawBuffer[i] = simd::ifelse(phase < 0.5f, 0.f, -2.f) + 2.f * phase;
			}

			// Sqr
			sqrBuffer[i] = simd::ifelse(phase < pw, 1.f, -1.f);
			if (analog) {
				// Add a highpass filter here
				sqrFilter.setCutoff(10.f * deltaTime);
				sqrFilter.process(sqrBuffer[i]);
				sqrBuffer[i] = 0.771f * sqrFilter.highpass();
			}

			// Advance phase
			phase += simd::ifelse(syncDirection, -1.f, 1.f) * deltaPhase / OVERSAMPLE;
			// Wrap phase to [0, 1)
			phase -= simd::floor(phase);
		}
	}

	T sin() {
		return sinDecimator.process(sinBuffer);
		// sinBuffer[0];
	}
	T tri() {
		return triDecimator.process(triBuffer);
		// triBuffer[0];
	}
	T saw() {
		return sawDecimator.process(sawBuffer);
		// sawBuffer[0];
	}
	T sqr() {
		return sqrDecimator.process(sqrBuffer);
		// sqrBuffer[0];
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
		configParam(PW_PARAM, 0.f, 1.f, 0.5f, "Pulse width", "%", 0.f, 100.f);
		configParam(PWM_PARAM, 0.f, 1.f, 0.f, "Pulse width modulation", "%", 0.f, 100.f);
		lightDivider.setDivision(16);
	}

	void process(const ProcessArgs &args) override {
		int channels = std::max(inputs[PITCH_INPUT].getChannels(), 1);

		for (int c = 0; c < channels; c += 4) {
			auto *oscillator = &oscillators[c / 4];
			oscillator->analog = params[MODE_PARAM].getValue() > 0.f;
			oscillator->soft = params[SYNC_PARAM].getValue() <= 0.f;

			float pitchFine = 3.f * dsp::quadraticBipolar(params[FINE_PARAM].getValue());
			float_4 pitchCv = 12.f * inputs[PITCH_INPUT].getVoltageSimd<float_4>(c);
			if (inputs[FM_INPUT].isConnected()) {
				pitchCv += dsp::quadraticBipolar(params[FM_PARAM].getValue()) * 12.f * inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c);
			}
			oscillator->setPitch(params[FREQ_PARAM].getValue(), pitchFine + pitchCv);
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
				float lightValue = oscillators[0].light().s[0];
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
		float freqParam = params[FREQ_PARAM].getValue();
		float fmParam = params[FM_PARAM].getValue();
		float waveParam = params[WAVE_PARAM].getValue();

		int channels = std::max(inputs[FM_INPUT].getChannels(), 1);

		for (int c = 0; c < channels; c += 4) {
			auto *oscillator = &oscillators[c / 4];
			oscillator->analog = (params[MODE_PARAM].getValue() > 0.f);
			oscillator->soft = (params[SYNC_PARAM].getValue() <= 0.f);

			float_4 pitch = freqParam + dsp::quadraticBipolar(fmParam) * 12.f * inputs[FM_INPUT].getVoltageSimd<float_4>(c);
			oscillator->setPitch(0.f, pitch);

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
				float lightValue = oscillators[0].light().s[0];
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
