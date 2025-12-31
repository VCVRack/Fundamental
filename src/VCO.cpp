#include "plugin.hpp"


// TODO: Remove these DSP classes after released in Rack SDK

/** Evaluates sin(pi x) for x in [-1, 1].
9th order polynomial with roots at 0, -1, and 1.
Optimized coefficients for best THD (-103.7 dB) and max absolute error (6.68e-06).
*/
template <typename T>
inline T sin_pi_9(T x) {
	T x2 = x * x;
	return x * (T(1) - x2) * (T(3.141521108990) + x2 * (T(-2.024773594411) + x2 * (T(0.517493161073) + x2 * T(-0.063691343423))));
}

/** Catmull-Rom cubic interpolation
t is the fractional position between y1 and y2.
*/
template <typename T>
T cubicInterp(T y0, T y1, T y2, T y3, T t) {
	T t2 = t * t;
	T t3 = t2 * t;
	return y1 + T(0.5) * t * (y2 - y0)
		+ t2 * (y0 - T(2.5)*y1 + T(2)*y2 - T(0.5)*y3)
		+ t3 * (T(-0.5)*y0 + T(1.5)*y1 - T(1.5)*y2 + T(0.5)*y3);
}

/** One-pole low-pass filter, exponential moving average.
-6 dB/octave slope.
Useful for leaky integrators and smoothing control signals.
Has a pole at (1 - alpha) and a zero at 0.
*/
struct OnePoleLowpass {
	float alpha = 0.f;

	/** Sets the cutoff frequency where gain is -3 dB.
	f = f_c / f_s, the normalized frequency in the range [0, 0.5].
	*/
	void setCutoff(float f) {
		alpha = 1.f - std::exp(-2.f * M_PI * f);
	}

	template <typename T = float>
	struct State {
		T y = 0.f;
	};

	/** Advances the state with input x. Returns the output. */
	template <typename T>
	T process(State<T>& s, T x) const {
		s.y += alpha * (x - s.y);
		return s.y;
	}

	/** Computes the frequency response at normalized frequency f. */
	std::complex<float> getResponse(float f) const {
		float omega = 2.f * M_PI * f;
		std::complex<float> z = std::exp(std::complex<float>(0.f, omega));
		return alpha * z / (z - (1.f - alpha));
	}
	float getMagnitude(float f) const {
		return std::abs(getResponse(f));
	}
	float getPhase(float f) const {
		return std::arg(getResponse(f));
	}
};


/** One-pole high-pass filter.
6 dB/octave slope.
Useful for DC-blocking.
Has a pole at (1 - alpha) and a zero at 1.
*/
struct OnePoleHighpass : OnePoleLowpass {
	template <typename T>
	T process(State<T>& s, T x) const {
		return x - OnePoleLowpass::process(s, x);
	}

	std::complex<float> getResponse(float f) const {
		return 1.f - OnePoleLowpass::getResponse(f);
	}
	float getMagnitude(float f) const {
		return std::abs(getResponse(f));
	}
	float getPhase(float f) const {
		return std::arg(getResponse(f));
	}
};


template <int Z, int O>
struct MinBlep {
	/** Reordered impulse response for cubic interpolation, minus 1.0.
	Z dimension has +1 padding at start (for z-1 wrap) and +4 at end (for SIMD + z+1 wrap).
	Access via getImpulse() which handles o wrapping and z offset.
	*/
	float impulseReordered[O][1 + 2 * Z + 4] = {};

	MinBlep() {
		float impulse[2 * Z][O];
		dsp::minBlepImpulse(Z, O, &impulse[0][0]);

		// Fill impulseReordered with transposed data, minus 1.
		// Storage index = z + 1 (to allow z = -1 access at index 0)
		for (int o = 0; o < O; o++) {
			// z = -1: before impulse starts
			impulseReordered[o][0] = -1.f;
			// z = 0 to 2*Z-1: actual impulse data
			for (int z = 0; z < 2 * Z; z++) {
				impulseReordered[o][1 + z] = impulse[z][o] - 1.f;
			}
			// z = 2*Z to 2*Z+3: after impulse ends (for SIMD padding)
			for (int z = 2 * Z; z < 2 * Z + 4; z++) {
				impulseReordered[o][1 + z] = 0.f;
			}
		}
	}

	/** Get pointer to impulse data, handling o wrapping and z offset. */
	const float* getImpulse(int o, int z) const {
		int index = z * O + o;
		z = index / O;
		o = index % O;
		if (o < 0) {
			z -= 1;
			o += O;
		}
		return &impulseReordered[o][z + 1];
	}

	template <typename T = float>
	struct State {
		// Double buffer of length 2 * Z
		T buffer[4 * Z] = {};
		int32_t bufferIndex = 0;
	};

	/** Places a discontinuity with magnitude `x` at 0 < p <= 1 relative to the current frame.
	For example if a square wave will jump from 1 to -1 in 0.1 frames, use insertDiscontinuity(0.1, -2.0).

	Note: In the deprecated MinBlepGenerator, `p` was in the range (-1, 0], so add 1 to p if updating to MinBlep.
	*/
	template <typename T>
	void insertDiscontinuity(State<T>& s, float p, T x) const {
		if (!(0 < p && p <= 1))
			return;

		// Calculate impulse array index and fractional part
		float subsample = (1 - p) * O;
		int o = (int) subsample;
		float t = subsample - o;

		// For each zero crossing, cubic interpolate impulse response
		for (int z = 0; z < 2 * Z; z += 4) {
			simd::float_4 y0 = simd::float_4::load(getImpulse(o - 1, z));
			simd::float_4 y1 = simd::float_4::load(getImpulse(o, z));
			simd::float_4 y2 = simd::float_4::load(getImpulse(o + 1, z));
			simd::float_4 y3 = simd::float_4::load(getImpulse(o + 2, z));
			simd::float_4 ir = cubicInterp(y0, y1, y2, y3, T(t));

			// Write to double buffer
			for (int zz = 0; zz < 4; zz++) {
				s.buffer[s.bufferIndex + z + zz] += ir[zz] * x;
			}
		}
	}

	/** Should be called every frame after inserting any discontinuities. */
	template <typename T>
	T process(State<T>& s) const {
		T v = s.buffer[s.bufferIndex];
		s.buffer[s.bufferIndex] = T(0);
		s.bufferIndex++;
		if (s.bufferIndex >= 2 * Z) {
			// Move second half of buffer to beginning
			std::memcpy(s.buffer, s.buffer + 2 * Z, 2 * Z * sizeof(T));
			std::memset(s.buffer + 2 * Z, 0, 2 * Z * sizeof(T));
			s.bufferIndex = 0;
		}
		return v;
	}
};


static MinBlep<16, 16> minBlep;


template <typename T>
struct VCOProcessor {
	T phase = 0.f;
	T lastSyncValue = 0.f;
	T syncDirection = 1.f;
	T sqrState = 1.f;
	T triFilterState = 0.f;

	OnePoleHighpass dcFilter;
	OnePoleHighpass::State<T> dcFilterStateSqr;
	OnePoleHighpass::State<T> dcFilterStateSaw;
	OnePoleHighpass::State<T> dcFilterStateTri;
	OnePoleHighpass::State<T> dcFilterStateSin;

	MinBlep<16, 16>::State<T> sqrMinBlep;
	MinBlep<16, 16>::State<T> sawMinBlep;
	MinBlep<16, 16>::State<T> sinMinBlep;

	void setSampleTime(float sampleTime) {
		dcFilter.setCutoff(std::min(0.4f, 20.f * sampleTime));
	}

	struct Frame {
		/** Number of channels valid in SIMD type
		For optimizing serial operations.
		*/
		uint8_t channels = 0;
		bool soft = false;
		bool syncEnabled = false;
		bool sqrEnabled = false;
		bool sawEnabled = false;
		bool triEnabled = false;
		bool sinEnabled = false;
		T pulseWidth = 0.5f;
		T sync = 0.f;
		T freq = 0.f;

		// Outputs
		T sqr = 0.f;
		T saw = 0.f;
		T tri = 0.f;
		T sin = 0.f;
	};

	void process(Frame& frame, float deltaTime) {
		// Advance phase
		T deltaPhase = simd::clamp(frame.freq * deltaTime, 0.f, 0.49f);
		if (frame.soft) {
			// Reverse direction
			deltaPhase *= syncDirection;
		}
		else {
			// Reset back to forward
			syncDirection = 1.f;
		}
		phase += deltaPhase;

		// Wrap phase
		T phaseFloor = simd::floor(phase);
		phase -= phaseFloor;

		if (frame.sqrEnabled || frame.triEnabled) {
			// Jump sqr when phase crosses 1, or crosses 0 if running backwards
			T wrapMask = (phaseFloor != 0.f);
			int wrapM = simd::movemask(wrapMask);
			if (wrapM) {
				T wrapPhase = (syncDirection == -1.f) & 1.f;
				T wrapCrossing = (wrapPhase - (phase - deltaPhase)) / deltaPhase;
				for (int i = 0; i < frame.channels; i++) {
					if (wrapM & (1 << i)) {
						T mask = simd::movemaskInverse<T>(1 << i);
						// TODO: MinBlepGenerator::insertDiscontinuity() should handle subframes outside the range -1 < p <= 0 instead of failing silently.
						float p = clamp(wrapCrossing[i], 0.f, 1.f);
						T x = mask & (2.f * syncDirection);
						minBlep.insertDiscontinuity(sqrMinBlep, p, x);
					}
				}
			}
			sqrState = simd::ifelse(wrapMask, syncDirection, sqrState);

			// Pulse width
			const float pwMin = 0.01f;
			T pulseWidth = simd::clamp(frame.pulseWidth, pwMin, 1.f - pwMin);

			// Jump sqr when crossing `pulseWidth`
			T pwMask = (syncDirection == sqrState) & ((syncDirection == 1.f) ^ (phase < pulseWidth));
			int pw = simd::movemask(pwMask);
			if (pw) {
				T pulseCrossing = (pulseWidth - (phase - deltaPhase)) / deltaPhase;
				for (int i = 0; i < frame.channels; i++) {
					if (pw & (1 << i)) {
						T mask = simd::movemaskInverse<T>(1 << i);
						float p = clamp(pulseCrossing[i], 0.f, 1.f);
						T x = mask & (-2.f * syncDirection);
						minBlep.insertDiscontinuity(sqrMinBlep, p, x);
					}
				}
			}
			sqrState = simd::ifelse(pwMask, -syncDirection, sqrState);
		}

		if (frame.sawEnabled) {
			// Jump saw when crossing 0.5
			T halfCrossing = (0.5f - (phase - deltaPhase)) / deltaPhase;
			int halfMask = simd::movemask((0 < halfCrossing) & (halfCrossing <= 1.f));
			if (halfMask) {
				for (int i = 0; i < frame.channels; i++) {
					if (halfMask & (1 << i)) {
						T mask = simd::movemaskInverse<T>(1 << i);
						float p = halfCrossing[i];
						T x = mask & (-2.f * syncDirection);
						minBlep.insertDiscontinuity(sawMinBlep, p, x);
					}
				}
			}
		}

		// Detect sync
		if (frame.syncEnabled) {
			T deltaSync = frame.sync - lastSyncValue;
			// Sample position where sync crosses 0.
			// Might be NAN or outside of (0, 1] range
			T syncCrossing = -lastSyncValue / deltaSync;
			lastSyncValue = frame.sync;
			T sync = (0.f < syncCrossing) & (syncCrossing <= 1.f) & (frame.sync >= 0.f);
			int syncMask = simd::movemask(sync);
			if (syncMask) {
				if (frame.soft) {
					syncDirection = simd::ifelse(sync, -syncDirection, syncDirection);
				}
				else {
					T newPhase = simd::ifelse(sync, (1.f - syncCrossing) * deltaPhase, phase);
					// Insert minBLEP for sync
					for (int i = 0; i < frame.channels; i++) {
						if (syncMask & (1 << i)) {
							T mask = simd::movemaskInverse<T>(1 << i);
							float p = syncCrossing[i];
							if (frame.sqrEnabled || frame.triEnabled) {
								// Assume that hard-syncing a square always resets it to HIGH
								T x = mask & (1.f - sqrState);
								sqrState = simd::ifelse(mask, 1.f, sqrState);
								minBlep.insertDiscontinuity(sqrMinBlep, p, x);
								DEBUG("%f %f %f", p, x[i], sqrState[i]);
							}
							if (frame.sawEnabled) {
								T x = mask & (saw(newPhase) - saw(phase));
								minBlep.insertDiscontinuity(sawMinBlep, p, x);
							}
							if (frame.sinEnabled) {
								T x = mask & (sin(newPhase) - sin(phase));
								minBlep.insertDiscontinuity(sinMinBlep, p, x);
							}
						}
					}
					phase = newPhase;
				}
			}
		}

		// Saw
		if (frame.sawEnabled) {
			frame.saw = saw(phase);
			frame.saw += minBlep.process(sawMinBlep);
			frame.saw = dcFilter.process(dcFilterStateSaw, frame.saw);
		}

		// Square
		if (frame.sqrEnabled || frame.triEnabled) {
			frame.sqr = sqrState;
			frame.sqr += minBlep.process(sqrMinBlep);
			T triSqr = frame.sqr;
			frame.sqr = dcFilter.process(dcFilterStateSqr, frame.sqr);

			// Tri
			if (frame.triEnabled) {
				// Integrate square wave
				const float triShape = 0.2f;
				T triFreq = deltaTime * triShape * frame.freq;
				// T alpha = 1.f - simd::exp(-2.f * M_PI * triFreq);
				// Use bilinear transform to derive alpha
				T alpha = 1 / (1 + 1 / (M_PI * triFreq));
				triFilterState += alpha * (triSqr - triFilterState);
				// Apply gain to roughly have unit amplitude at 0.5 pulseWidth. Depends on triShape.
				frame.tri = triFilterState * 6.6f;
				frame.tri = dcFilter.process(dcFilterStateTri, frame.tri);
			}
		}

		// Sin
		if (frame.sinEnabled) {
			frame.sin = sin(phase);
			frame.sin += minBlep.process(sinMinBlep);
			frame.sin = dcFilter.process(dcFilterStateSin, frame.sin);
		}
	}

	T light() const {
		return sin(phase);
	}

	static T saw(T phase) {
		T x = phase + 0.5f;
		x -= simd::trunc(x);
		return 2 * x - 1;
	}
	static T tri(T phase) {
		return 1 - 4 * simd::fmin(simd::fabs(phase - 0.25f), simd::fabs(phase - 1.25f));
	}
	static T sin(T phase) {
		return sin_pi_9(2 * phase - 1);
	}
};


using simd::float_4;


struct VCO : Module {
	enum ParamIds {
		MODE_PARAM, // removed
		SYNC_PARAM,
		FREQ_PARAM,
		FINE_PARAM, // removed
		FM_PARAM,
		PW_PARAM,
		PW_CV_PARAM,
		// new in 2.0
		LINEAR_PARAM,
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
		LINEAR_LIGHT,
		SOFT_LIGHT,
		NUM_LIGHTS
	};

	VCOProcessor<float_4> processors[4];
	dsp::ClockDivider lightDivider;

	VCO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(LINEAR_PARAM, 0.f, 1.f, 0.f, "FM mode", {"1V/octave", "Linear"});
		configSwitch(SYNC_PARAM, 0.f, 1.f, 1.f, "Sync mode", {"Soft", "Hard"});
		configParam(FREQ_PARAM, -75.f, 75.f, 0.f, "Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(FM_PARAM, -1.f, 1.f, 0.f, "Frequency modulation", "%", 0.f, 100.f);
		getParamQuantity(FM_PARAM)->randomizeEnabled = false;
		configParam(PW_PARAM, 0.01f, 0.99f, 0.5f, "Pulse width", "%", 0.f, 100.f);
		configParam(PW_CV_PARAM, -1.f, 1.f, 0.f, "Pulse width modulation", "%", 0.f, 100.f);
		getParamQuantity(PW_CV_PARAM)->randomizeEnabled = false;

		configInput(PITCH_INPUT, "1V/octave pitch");
		configInput(FM_INPUT, "Frequency modulation");
		configInput(SYNC_INPUT, "Sync");
		configInput(PW_INPUT, "Pulse width modulation");

		configOutput(SIN_OUTPUT, "Sine");
		configOutput(TRI_OUTPUT, "Triangle");
		configOutput(SAW_OUTPUT, "Sawtooth");
		configOutput(SQR_OUTPUT, "Square");

		lightDivider.setDivision(16);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		for (int c = 0; c < 16; c += 4) {
			processors[c / 4].setSampleTime(e.sampleTime);
		}
	}

	void process(const ProcessArgs& args) override {
		VCOProcessor<float_4>::Frame frame;
		float freqParam = params[FREQ_PARAM].getValue() / 12.f;
		float fmParam = params[FM_PARAM].getValue();
		float pwParam = params[PW_PARAM].getValue();
		float pwCvParam = params[PW_CV_PARAM].getValue();
		bool linear = params[LINEAR_PARAM].getValue() > 0.f;
		frame.soft = params[SYNC_PARAM].getValue() <= 0.f;
		frame.syncEnabled = inputs[SYNC_INPUT].isConnected();
		frame.sqrEnabled = outputs[SQR_OUTPUT].isConnected();
		frame.sawEnabled = outputs[SAW_OUTPUT].isConnected();
		frame.triEnabled = outputs[TRI_OUTPUT].isConnected();
		frame.sinEnabled = outputs[SIN_OUTPUT].isConnected();
		int channels = std::max(inputs[PITCH_INPUT].getChannels(), 1);

		for (int c = 0; c < channels; c += 4) {
			frame.channels = std::min(channels - c, 4);

			// Get frequency
			float_4 pitch = freqParam + inputs[PITCH_INPUT].getPolyVoltageSimd<float_4>(c);
			float_4 freq;
			if (!linear) {
				pitch += inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam;
				freq = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch);
			}
			else {
				freq = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch);
				freq += dsp::FREQ_C4 * inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam;
			}
			frame.freq = clamp(freq, 0.f, args.sampleRate / 2.f);

			// Get pulse width
			frame.pulseWidth = pwParam + inputs[PW_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f * pwCvParam;

			frame.sync = inputs[SYNC_INPUT].getPolyVoltageSimd<float_4>(c);
			processors[c / 4].process(frame, args.sampleTime);

			// Set output
			outputs[SQR_OUTPUT].setVoltageSimd(5.f * frame.sqr, c);
			outputs[SAW_OUTPUT].setVoltageSimd(5.f * frame.saw, c);
			outputs[TRI_OUTPUT].setVoltageSimd(5.f * frame.tri, c);
			outputs[SIN_OUTPUT].setVoltageSimd(5.f * frame.sin, c);
		}

		outputs[SIN_OUTPUT].setChannels(channels);
		outputs[TRI_OUTPUT].setChannels(channels);
		outputs[SAW_OUTPUT].setChannels(channels);
		outputs[SQR_OUTPUT].setChannels(channels);

		// Light
		if (lightDivider.process()) {
			if (channels == 1) {
				float lightValue = processors[0].light()[0];
				lights[PHASE_LIGHT + 0].setSmoothBrightness(-lightValue, args.sampleTime * lightDivider.getDivision());
				lights[PHASE_LIGHT + 1].setSmoothBrightness(lightValue, args.sampleTime * lightDivider.getDivision());
				lights[PHASE_LIGHT + 2].setBrightness(0.f);
			}
			else {
				lights[PHASE_LIGHT + 0].setBrightness(0.f);
				lights[PHASE_LIGHT + 1].setBrightness(0.f);
				lights[PHASE_LIGHT + 2].setBrightness(1.f);
			}
			lights[LINEAR_LIGHT].setBrightness(linear);
			lights[SOFT_LIGHT].setBrightness(frame.soft);
		}
	}
};


struct VCOWidget : ModuleWidget {
	VCOWidget(VCO* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/VCO.svg"), asset::plugin(pluginInstance, "res/VCO-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(22.905, 29.808)), module, VCO::FREQ_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(22.862, 56.388)), module, VCO::PW_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.607, 80.603)), module, VCO::FM_PARAM));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(17.444, 80.603)), module, VCO::LINEAR_PARAM, VCO::LINEAR_LIGHT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(28.282, 80.603)), module, VCO::SYNC_PARAM, VCO::SOFT_LIGHT));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(39.118, 80.603)), module, VCO::PW_CV_PARAM));

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(6.607, 96.859)), module, VCO::FM_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(17.444, 96.859)), module, VCO::PITCH_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(28.282, 96.859)), module, VCO::SYNC_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(39.15, 96.859)), module, VCO::PW_INPUT));

		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(6.607, 113.115)), module, VCO::SIN_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(17.444, 113.115)), module, VCO::TRI_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(28.282, 113.115)), module, VCO::SAW_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(39.119, 113.115)), module, VCO::SQR_OUTPUT));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(31.089, 16.428)), module, VCO::PHASE_LIGHT));
	}
};


Model* modelVCO = createModel<VCO, VCOWidget>("VCO");
