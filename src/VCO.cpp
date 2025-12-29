#include "plugin.hpp"


// TODO: Remove these DSP classes after released in Rack SDK

inline simd::float_4 ifelse(simd::float_4 mask, simd::float_4 a, simd::float_4 b) {
	return simd::float_4(_mm_blendv_ps(b.v, a.v, mask.v));
}

/** Approximates sin(2pi x) in the domain [0, 1].
Evaluates a 7th order odd polynomial on the range [0, 0.25] after folding the argument.
Optimized for THD and max error.
THD -84 dB, max error 1e-04
*/
template <typename T>
T sin2pi_fast(T x) {
	// Get sign of result
	T sign = ::ifelse(x >= T(0.5), -1, 1);
	// Shift if negative
	x = ::ifelse(x >= T(0.5), x - T(0.5), x);
	// Flip if >0.25
	x = ::ifelse(x >= T(0.25), T(0.5) - x, x);
	T x2 = x * x;
	const T c1 = 6.281422578399;
	const T c3 = -41.122058514502;
	const T c5 = 74.010711837743;
	return sign * x * (c1 + x2 * (c3 + x2 * c5));
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
	T process(State<T>& s, T x) {
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
	T process(State<T>& s, T x) {
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


/** Computes the minimum-phase bandlimited step (MinBLEP)
Z: number of zero-crossings on each side of the original symmetric sync signal
O: oversample factor
output: must be length `(2 * Z) * O`.
First sample is >0. Last sample is 1.

Algorithm from "Hard Sync Without Aliasing" by Eli Brandt (2001).
https://www.cs.cmu.edu/~eli/papers/icmc01-hardsync.pdf
*/
void minBlepImpulse(int Z, int O, float* output);


template <int Z, int O, typename T = float>
struct MinBlepGenerator {
	T buffer[2 * Z] = {};
	int bufferIndex = 0;

	/** Reordered impulse response for adjacent access, minus 1.0.
	Padded at end with zeros so impulseReordered[o][2 * Z] can be loaded into a simd::float_4.
	*/
	alignas(16) float impulseReordered[O][2 * Z + 4] = {};

	MinBlepGenerator() {
		float impulse[2 * Z][O];
		dsp::minBlepImpulse(Z, O, &impulse[0][0]);

		// Transpose array and pre-subtract 1.
		for (int o = 0; o < O; o++) {
			for (int z = 0; z < 2 * Z; z++) {
				impulseReordered[o][z] = impulse[z][o] - 1.f;
			}
		}
	}

	/** Places a discontinuity with magnitude `x` at -1 < p <= 0 relative to the current frame.
	For example if a square wave jumps from 1 to -1 at the subsample time 0.1 frames ago, use insertDiscontinuity(-0.1, -2.0)
	*/
	void insertDiscontinuity(float p, T x) {
		if (!(-1 < p && p <= 0))
			return;

		// Calculate oversampling index and fractional part
		float subsample = -p * O;
		int o = (int) subsample;
		float frac = subsample - o;

		// For each zero crossing, interpolate impulse response between oversample indices
		// Compute 4 at a time with SIMD
		for (int z = 0; z < 2 * Z; z += 4) {
			simd::float_4 ir0 = simd::float_4::load(&impulseReordered[o][z]);
			// If oversample index reaches next zero crossing, wrap to next z
			simd::float_4 ir1 = simd::float_4::load((o + 1 < O) ? &impulseReordered[o + 1][z] : &impulseReordered[0][z + 1]);
			simd::float_4 ir = (ir0 + (ir1 - ir0) * frac);

			for (int zz = 0; zz < 4; zz++) {
				buffer[(bufferIndex + z + zz) % (2 * Z)] += ir[zz] * x;
			}
		}
		// for (int z = 0; z < 2 * Z; z++) {
		// 	float ir0 = impulseReordered[o][z];
		// 	float ir1 = (o + 1 < O) ? impulseReordered[o + 1][z] : impulseReordered[0][z + 1];
		// 	float ir = (ir0 + (ir1 - ir0) * frac);
		// 	buffer[(bufferIndex + z) % (2 * Z)] += ir * x;
		// }
	}

	/** Should be called every frame after inserting any discontinuities. */
	T process() {
		T v = buffer[bufferIndex];
		buffer[bufferIndex] = T(0);
		bufferIndex = (bufferIndex + 1) % (2 * Z);
		return v;
	}
};


template <typename T>
struct VCOProcessor {
	T phase = 0.f;
	T lastSyncValue = 0.f;
	T syncDirection = 1.f;
	T sqrState = 1.f;

	OnePoleHighpass dcFilter;
	OnePoleHighpass::State<T> dcFilterStateSqr;
	OnePoleHighpass::State<T> dcFilterStateSaw;
	OnePoleHighpass::State<T> dcFilterStateTri;
	OnePoleHighpass::State<T> dcFilterStateSin;

	MinBlepGenerator<16, 16, T> sqrMinBlep;
	MinBlepGenerator<16, 16, T> sawMinBlep;
	MinBlepGenerator<16, 16, T> triMinBlep;
	MinBlepGenerator<16, 16, T> sinMinBlep;

	void setSampleTime(float sampleTime) {
		dcFilter.setCutoff(std::min(0.4f, 40.f * sampleTime));
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
					float p = clamp(wrapCrossing[i] - 1.f, -1.f, 0.f);
					T x = mask & (2.f * syncDirection);
					sqrMinBlep.insertDiscontinuity(p, x);
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
					float p = clamp(pulseCrossing[i] - 1.f, -1.f, 0.f);
					T x = mask & (-2.f * syncDirection);
					sqrMinBlep.insertDiscontinuity(p, x);
				}
			}
		}
		sqrState = simd::ifelse(pwMask, -syncDirection, sqrState);

		// Jump saw when crossing 0.5
		T halfCrossing = (0.5f - (phase - deltaPhase)) / deltaPhase;
		int halfMask = simd::movemask((0 < halfCrossing) & (halfCrossing <= 1.f));
		if (halfMask) {
			for (int i = 0; i < frame.channels; i++) {
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
		if (frame.syncEnabled) {
			T deltaSync = frame.sync - lastSyncValue;
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
							float p = syncCrossing[i] - 1.f;
							T x;
							// Assume that hard-syncing a square always resets it to HIGH
							x = mask & (1.f - sqrState);
							sqrState = simd::ifelse(mask, 1.f, sqrState);
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
		if (frame.sqrEnabled) {
			frame.sqr = sqrState;
			frame.sqr += sqrMinBlep.process();
			frame.sqr = dcFilter.process(dcFilterStateSqr, frame.sqr);
		}

		// Saw
		if (frame.sawEnabled) {
			frame.saw = saw(phase);
			frame.saw += sawMinBlep.process();
			frame.saw = dcFilter.process(dcFilterStateSaw, frame.saw);
		}

		// Tri
		if (frame.triEnabled) {
			frame.tri = tri(phase);
			frame.tri += triMinBlep.process();
			frame.tri = dcFilter.process(dcFilterStateTri, frame.tri);
		}

		// Sin
		if (frame.sinEnabled) {
			frame.sin = sin(phase);
			frame.sin += sinMinBlep.process();
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
		return sin2pi_fast(phase);
		// return simd::sin(2.f * M_PI * phase);
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
