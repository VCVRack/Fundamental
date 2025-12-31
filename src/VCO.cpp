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
T catmullRomInterpolate(T y0, T y1, T y2, T y3, T t) {
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


/** Holds a precomputed minBLEP impulse response, reordered to efficiently insert into an audio buffer.
*/
template <int Z, int O>
struct MinBlep {
	/** Reordered impulse response for cubic interpolation, minus 1.0.
	Z dimension has +1 padding at start (for z-1 wrap) and +4 at end (for SIMD + z+1 wrap).
	Access via getImpulse() which handles O wrapping and Z offset.
	*/
	float impulseReordered[O][1 + 2 * Z + 4] = {};

	MinBlep() {
		float impulse[2 * Z][O];
		dsp::minBlepImpulse(Z, O, &impulse[0][0]);

		// Fill impulseReordered with transposed and pre-subtracted data
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
		z += o / O;
		o = o % O;
		if (o < 0) {
			z -= 1;
			o += O;
		}
		// assert(0 <= o && o < O);
		// assert(-1 <= z && z < 2 * Z + 4);
		return &impulseReordered[o][z + 1];
	}

	/** Places a discontinuity at 0 < p <= 1 relative to the current frame.
	`out` must have enough space to write 2*Z floats, spaced by `stride` floats.
	`p` is the subsample position to insert a discontinuity of `magnitude`.
	For example if a square wave will jump from 1 to -1 in 0.1 frames, use insertDiscontinuity(out, 1, 0.1f, -2.f).

	Note: In the deprecated MinBlepGenerator, `p` was in the range (-1, 0], so add 1 to p if updating to MinBlep.
	*/
	void insertDiscontinuity(float* out, int stride, float p, float magnitude) const {
		if (!(0 < p && p <= 1))
			return;

		// Calculate impulse array index and fractional part
		float subsample = (1 - p) * O;
		int o = (int) subsample;
		float t = subsample - o;

		// For each zero crossing, interpolate impulse response from oversample points
		using simd::float_4;
		for (int z = 0; z < 2 * Z; z += 4) {
			float_4 y0 = float_4::load(getImpulse(o - 1, z));
			float_4 y1 = float_4::load(getImpulse(o, z));
			float_4 y2 = float_4::load(getImpulse(o + 1, z));
			float_4 y3 = float_4::load(getImpulse(o + 2, z));
			float_4 y = catmullRomInterpolate(y0, y1, y2, y3, float_4(t));
			y *= magnitude;

			// Write all 4 samples to buffer
			for (int zi = 0; zi < 4; zi++) {
				out[(z + zi) * stride] += y[zi];
			}
		}
	}
};


/** Buffer that allows reading/writing up to N future elements contiguously.
*/
template <int N, typename T>
struct MinBlepBuffer {
	T buffer[2 * N] = {};
	int32_t bufferIndex = 0;

	T* startData() {
		return &buffer[bufferIndex];
	}

	/** Returns the current element and advances the buffer.
	*/
	T shift() {
		T v = buffer[bufferIndex];
		bufferIndex++;
		if (bufferIndex >= N) {
			// Move second half of buffer to beginning
			std::memcpy(buffer, buffer + N, N * sizeof(T));
			std::memset(buffer + N, 0, N * sizeof(T));
			bufferIndex = 0;
		}
		return v;
	}

	/** Copies `n` elements to `out` and advances the buffer.
	*/
	void shiftBuffer(T* out, size_t n) {
		std::memcpy(out, buffer + bufferIndex, n * sizeof(T));
		bufferIndex += n;
		if (bufferIndex >= N) {
			std::memcpy(buffer, buffer + N, N * sizeof(T));
			std::memset(buffer + N, 0, N * sizeof(T));
			bufferIndex = 0;
		}
	}
};


static MinBlep<16, 16> minBlep;


template <typename T>
struct VCOProcessor {
	T phase = 0.f;
	T lastSync = 0.f;
	T syncDirection = 1.f;
	T triFilterState = 0.f;

	OnePoleHighpass dcFilter;
	OnePoleHighpass::State<T> dcFilterStateSqr;
	OnePoleHighpass::State<T> dcFilterStateSaw;
	OnePoleHighpass::State<T> dcFilterStateTri;
	OnePoleHighpass::State<T> dcFilterStateSin;

	MinBlepBuffer<16*2, T> sqrMinBlep;
	MinBlepBuffer<16*2, T> sawMinBlep;
	MinBlepBuffer<16*2, T> sinMinBlep;

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

	void process(Frame& frame, float sampleTime) {
		// Compute deltaPhase for full frame
		T deltaPhase = simd::clamp(frame.freq * sampleTime, 0.f, 0.49f);
		if (frame.soft) {
			deltaPhase *= syncDirection;
		}
		else {
			syncDirection = 1.f;
		}

		T prevPhase = phase;
		const float pwMin = 0.01f;
		T pulseWidth = simd::clamp(frame.pulseWidth, pwMin, 1.f - pwMin);

		// Inserts minBLEP for each channel where mask is true.
		// Serial but does nothing if mask is all zero.
		auto insertMinBlep = [&](T mask, T p, T magnitude, MinBlepBuffer<16*2, T>& minBlepBuffer) {
			int m = simd::movemask(mask);
			if (!m)
				return;
			float* buffer = (float*) minBlepBuffer.startData();
			for (int i = 0; i < frame.channels; i++) {
				if (m & (1 << i)) {
					minBlep.insertDiscontinuity(&buffer[i], 4, p[i], magnitude[i]);
				}
			}
		};

		// Computes subsample time where phase crosses threshold.
		auto getCrossing = [](T threshold, T startPhase, T endPhase, T startSubsample, T endSubsample) -> T {
			// Wrap threshold mod 1 to be in (startPhase, startPhase + 1]
			threshold -= simd::floor(threshold - startPhase);
			// Map to (0, 1]
			T p = (threshold - startPhase) / (endPhase - startPhase);
			// Map to (startSubsample, endSubsample]
			return startSubsample + p * (endSubsample - startSubsample);
		};

		// Processes wrap/pulse/saw crossings between startPhase and endPhase.
		// startSubsample and endSubsample define the time range within the frame.
		// channelMask limits processing to specific channels.
		auto processCrossings = [&](T startPhase, T endPhase, T startSubsample, T endSubsample, T channelMask) {
			if (frame.sqrEnabled || frame.triEnabled) {
				// Wrap crossing (phase crosses 0 mod 1)
				T wrapSubsample = getCrossing(1.f, startPhase, endPhase, startSubsample, endSubsample);
				T wrapMask = channelMask & (startSubsample < wrapSubsample) & (wrapSubsample <= endSubsample);
				insertMinBlep(wrapMask, wrapSubsample, 2.f * syncDirection, sqrMinBlep);

				// Pulse width crossing
				T pulseSubsample = getCrossing(pulseWidth, startPhase, endPhase, startSubsample, endSubsample);
				T pulseMask = channelMask & (startSubsample < pulseSubsample) & (pulseSubsample <= endSubsample);
				insertMinBlep(pulseMask, pulseSubsample, -2.f * syncDirection, sqrMinBlep);
			}

			if (frame.sawEnabled) {
				// Saw crossing at 0.5
				T sawSubsample = getCrossing(0.5f, startPhase, endPhase, startSubsample, endSubsample);
				T sawMask = channelMask & (startSubsample < sawSubsample) & (sawSubsample <= endSubsample);
				insertMinBlep(sawMask, sawSubsample, -2.f * syncDirection, sawMinBlep);
			}
		};

		// Main logic
		if (!frame.syncEnabled) {
			// No sync. Process full frame
			T endPhase = prevPhase + deltaPhase;
			processCrossings(prevPhase, endPhase, 0.f, 1.f, T::mask());
			phase = endPhase;
		}
		else {
			// Compute sync subsample position
			T deltaSync = frame.sync - lastSync;
			T syncSubsample = -lastSync / deltaSync;
			lastSync = frame.sync;
			T syncOccurred = (0.f < syncSubsample) & (syncSubsample <= 1.f) & (frame.sync >= 0.f);
			T noSync = ~syncOccurred;

			if (simd::movemask(noSync)) {
				// No sync for these channels. Process full frame
				T endPhase = prevPhase + deltaPhase;
				processCrossings(prevPhase, endPhase, 0.f, 1.f, noSync);
				phase = simd::ifelse(noSync, endPhase, phase);
			}

			if (simd::movemask(syncOccurred)) {
				// Process crossings before sync
				T syncPhase = prevPhase + deltaPhase * syncSubsample;
				processCrossings(prevPhase, syncPhase, 0.f, syncSubsample, syncOccurred);

				if (frame.soft) {
					// Soft sync: Reverse direction, continue from syncPhase
					syncDirection = simd::ifelse(syncOccurred, -syncDirection, syncDirection);
					T endPhase = syncPhase + (-deltaPhase) * (1.f - syncSubsample);
					processCrossings(syncPhase, endPhase, syncSubsample, 1.f, syncOccurred);
					phase = simd::ifelse(syncOccurred, endPhase, phase);
				}
				else {
					// Hard sync: reset to 0, insert discontinuities
					if (frame.sqrEnabled || frame.triEnabled) {
						// Sqr jumps from current state to +1
						T sqrJump = (syncPhase - simd::floor(syncPhase) < pulseWidth);
						insertMinBlep(syncOccurred & sqrJump, syncSubsample, 2.f, sqrMinBlep);
					}
					if (frame.triEnabled) {
						// TODO Insert minBLEP to Tri
					}
					if (frame.sawEnabled) {
						// Saw jumps from saw(syncPhase) to saw(0) = 0
						insertMinBlep(syncOccurred, syncSubsample, -saw(syncPhase), sawMinBlep);
					}
					if (frame.sinEnabled) {
						// sin jumps from sin(syncPhase) to sin(0) = 0
						insertMinBlep(syncOccurred, syncSubsample, -sin(syncPhase), sinMinBlep);
					}

					// Process crossings after sync (starting from phase 0)
					T endPhase = deltaPhase * (1.f - syncSubsample);
					processCrossings(0.f, endPhase, syncSubsample, 1.f, syncOccurred);
					phase = simd::ifelse(syncOccurred, endPhase, phase);
				}
			}
		}

		// Wrap phase to [0, 1)
		phase -= simd::floor(phase);

		// Generate outputs
		if (frame.sawEnabled) {
			frame.saw = saw(phase);
			frame.saw += sawMinBlep.shift();
			frame.saw = dcFilter.process(dcFilterStateSaw, frame.saw);
		}

		if (frame.sqrEnabled || frame.triEnabled) {
			T sqrHigh = (phase < pulseWidth) ^ syncDirection;
			frame.sqr = simd::ifelse(sqrHigh, 1.f, -1.f);
			frame.sqr += sqrMinBlep.shift();
			T triSqr = frame.sqr;
			frame.sqr = dcFilter.process(dcFilterStateSqr, frame.sqr);

			if (frame.triEnabled) {
				// Integrate square wave
				const float triShape = 0.2f;
				T triFreq = sampleTime * triShape * frame.freq;
				// Use bilinear transform to derive alpha
				T alpha = 1 / (1 + 1 / (M_PI * triFreq));
				// T alpha = 1.f - simd::exp(-2.f * M_PI * triFreq);
				triFilterState += alpha * (triSqr - triFilterState);
				// Apply gain to roughly have unit amplitude at 0.5 pulseWidth. Depends on triShape.
				frame.tri = triFilterState * 6.6f;
				frame.tri = dcFilter.process(dcFilterStateTri, frame.tri);
			}
		}

		if (frame.sinEnabled) {
			frame.sin = sin(phase);
			frame.sin += sinMinBlep.shift();
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
