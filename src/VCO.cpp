#include "plugin.hpp"


// TODO: Remove these DSP classes after released in Rack SDK

/** Approximates sin(2pi x) over [0, 1] with the form
$ t (t^2 - 1) (c_0 + t^2 (c_1 + t^2 (c_2 + t^2 c_3))) $ where $ t = 2x - 1 $
Optimized coefficients for lowest max absolute error 6.5e-06, THD -103.7 dB.
*/
template <typename T>
inline T sin_2pi_9(T x) {
	// Shift argument to [-1, 1]
	x = T(2) * x - T(1);
	T x2 = x * x;
	return x * (x2 - T(1)) * (T(3.1415211942925003) + x2 * (T(-2.0247734792732333) + x2 * (T(0.51749274223813413) + x2 * T(-0.063691093590695858))));
}


/** One-pole low-pass filter, exponential moving average.
-6 dB/octave slope.
Useful for leaky integrators and smoothing control signals.
Has a pole at (1 - alpha) and a zero at 0.
*/
struct OnePoleLowpass {
	float alpha = 0.f;

	/** Sets alpha using the matched-z / impulse invariance transform.
	Preserves the analog time constant, so step response reaches ~63.2% after t = 1/(2pi f) seconds.
	The -3 dB point is approximately f for low frequencies.
	f = f_c / f_s, the normalized frequency in the range [0, 0.5].
	*/
	void setCutoffMatchedZ(float f) {
		alpha = 1.f - std::exp(float(-2 * M_PI) * f);
	}

	/** Sets alpha using the bilinear transform.
	Guarantees exact -3 dB gain at frequency f, but warps other frequencies.
	f = f_c / f_s, the normalized frequency in the range [0, 0.5].
	*/
	void setCutoffBilinear(float f) {
		float w = std::tan(float(M_PI) * f);
		alpha = w / (1.f + w);
	}

	/** Sets alpha using a rational approximation.
	Approximates the bilinear transform for low frequencies (f << 0.1).
	f = f_c / f_s, the normalized frequency in the range [0, 0.5].
	*/
	void setCutoffApprox(float f) {
		float w = float(2 * M_PI) * f;
		alpha = w / (1.f + w);
	}

	/** Computes the frequency response at normalized frequency f. */
	std::complex<float> getResponse(float f) const {
		float omega = float(2 * M_PI) * f;
		std::complex<float> z = std::exp(std::complex<float>(0.f, omega));
		return alpha * z / (z - (1.f - alpha));
	}
	float getMagnitude(float f) const {
		return std::abs(getResponse(f));
	}
	float getPhase(float f) const {
		return std::arg(getResponse(f));
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
};


/** One-pole high-pass filter.
6 dB/octave slope.
Useful for DC-blocking.
Has a pole at (1 - alpha) and a zero at 1.
*/
struct OnePoleHighpass : OnePoleLowpass {
	std::complex<float> getResponse(float f) const {
		return 1.f - OnePoleLowpass::getResponse(f);
	}
	float getMagnitude(float f) const {
		return std::abs(getResponse(f));
	}
	float getPhase(float f) const {
		return std::arg(getResponse(f));
	}

	template <typename T>
	T process(State<T>& s, T x) const {
		return x - OnePoleLowpass::process(s, x);
	}
};


/** Simple radix-2 Cooley-Tukey FFT. n must be a power of 2. */
template <typename T>
void fft(std::complex<T>* x, int n, bool inverse = false) {
	// Bit reversal permutation
	for (int i = 1, j = 0; i < n; i++) {
		int bit = n >> 1;
		for (; j & bit; bit >>= 1)
			j ^= bit;
		j ^= bit;
		if (i < j)
			std::swap(x[i], x[j]);
	}

	// Cooley-Tukey butterflies
	for (int len = 2; len <= n; len <<= 1) {
		T ang = (inverse ? 1 : -1) * 2 * M_PI / len;
		std::complex<T> wn(std::cos(ang), std::sin(ang));
		for (int i = 0; i < n; i += len) {
			std::complex<T> w(1);
			for (int j = 0; j < len / 2; j++) {
				std::complex<T> u = x[i + j];
				std::complex<T> v = x[i + j + len / 2] * w;
				x[i + j] = u + v;
				x[i + j + len / 2] = u - v;
				w *= wn;
			}
		}
	}

	if (inverse) {
		for (int i = 0; i < n; i++)
			x[i] /= T(n);
	}
}


template <typename T>
inline T blackmanHarris(T p) {
	return
	  + T(0.35875)
	  - T(0.48829) * simd::cos(T(2 * M_PI) * p)
	  + T(0.14128) * simd::cos(T(4 * M_PI) * p)
	  - T(0.01168) * simd::cos(T(6 * M_PI) * p);
}


/** Computes the minimum-phase bandlimited step (minBLEP), an impulse step response that rises from 0 to 1.
Z: number of zero-crossings on each side of the original symmetric sync signal
O: oversample factor
output: must be length `(2 * Z) * O`.

Algorithm from "Hard Sync Without Aliasing" by Eli Brandt (2001).
https://www.cs.cmu.edu/~eli/papers/icmc01-hardsync.pdf
*/
inline void minBlepImpulse(int z, int o, float* output) {
	// Symmetric sinc impulse with `z` zero-crossings on each side
	int n = 2 * z * o;
	std::complex<double>* x = new std::complex<double>[n];
	for (int i = 0; i < n; i++) {
		double p = (double) i / o - z;
		x[i] = (p == 0.0) ? 1.0 : std::sin(M_PI * p) / (M_PI * p);
	}

	// Apply window
	for (int i = 0; i < n; i++) {
		x[i] *= blackmanHarris(i / (double) (n - 1));
	}

#if 1
	// Reconstruct impulse response with minimum phase
	fft(x, n);

	// Take log of magnitude and set phase to zero.
	// Limit frequency bin to e^-10
	for (int i = 0; i < n; i++) {
		x[i] = std::log(std::abs(x[i]));
	}


	// Transform to cepstral domain.
	fft(x, n, true);

	// Apply minimum-phase window in cepstral domain
	// Double positive quefrencies, zero negative quefrencies
	for (int i = 1; i < n / 2; i++) {
		x[i] *= 2.0;
	}
	for (int i = n / 2; i < n; i++) {
		x[i] = 0.0;
	}

	// Transform back to frequency domain
	fft(x, n);

	// Take complex exponential of each bin
	for (int i = 0; i < n; i++) {
		x[i] = std::exp(x[i]);
	}

	// Transform to time domain
	fft(x, n, true);
#endif

	// Integrate. First sample x[0] should be 0.
	double total = 0.0;
	for (int i = 0; i < n; i++) {
		output[i] = total;
		total += x[i].real() / o;
	}

	// Renormalize so last virtual sample x[n] should be exactly 1
	for (int i = 0; i < n; i++) {
		output[i] /= total;
	}

	delete[] x;
}


/** Holds a precomputed minBLEP impulse response, reordered to efficiently insert into an audio buffer.
*/
template <int Z, int O>
struct MinBlep {
	/** Reordered impulse response for linear interpolation, minus 1.0.
	Z dimension has +4 padding at end for SIMD and o+1 wrap.
	*/
	float impulseReordered[O][2 * Z + 4] = {};
	float rampReordered[O][2 * Z + 4] = {};

	MinBlep() {
		float impulse[2 * Z * O];
		minBlepImpulse(Z, O, impulse);

		// Subtract 1 so our step impulse goes from -1 to 0.
		for (int i = 0; i < 2*Z*O; i++) {
			impulse[i] -= 1.f;
		}

		// Integrate minBLEP to obtain minBLAMP
		float ramp[2 * Z * O];
		double total = 0.0;
		for (int i = 0; i < 2*Z*O; i++) {
			ramp[i] = total;
			total += impulse[i] / O;
		}
		// Subtract ideal ramp so ramp[0] and the virtual ramp[n] are 0
		for (int i = 0; i < 2*Z*O; i++) {
			ramp[i] -= (float) i / (2*Z*O) * total;
		}

		// FILE* f = fopen("plugins/Fundamental/minblep.txt", "w");
		// for (int i = 0; i < 2*Z*O; i++) {
		// 	fprintf(f, "%.12f\n", ramp[i]);
		// }
		// fclose(f);

		// Transpose samples by making z values contiguous for each o
		for (int o = 0; o < O; o++) {
			for (int z = 0; z < 2 * Z; z++) {
				impulseReordered[o][z] = impulse[z * O + o];
			}
		}
		for (int o = 0; o < O; o++) {
			for (int z = 0; z < 2 * Z; z++) {
				rampReordered[o][z] = ramp[z * O + o];
			}
		}
	}

	/** Places a discontinuity at 0 < subsample <= 1 relative to the current frame.
	`out` must have enough space to write 2*Z floats, spaced by `stride` floats.
	`subsample` is the subsample position to insert a discontinuity of `magnitude`.
	For example if a square wave will jump from 1 to -1 in 0.1 frames, use insertDiscontinuity(out, 1, 0.1f, -2.f).

	Note: In the deprecated MinBlepGenerator, `subsample` was in the range (-1, 0], so add 1 to subsample if updating to MinBlep.
	*/
	void insertDiscontinuity(float subsample, float magnitude, float* out, int stride = 1) const {
		insert(impulseReordered, subsample, magnitude, out, stride);
	}

	void insertSlopeDiscontinuity(float subsample, float magnitude, float* out, int stride = 1) const {
		insert(rampReordered, subsample, magnitude, out, stride);
	}

private:
	void insert(const float table[O][2 * Z + 4], float subsample, float magnitude, float* out, int stride = 1) const {
		if (!(0.f < subsample && subsample <= 1.f))
			return;

		// Calculate impulse array index and fractional part
		float t = (1.f - subsample) * O;
		int o = (int) t;
		t -= o;

		// For each zero crossing, linearly interpolate impulse response from oversample points
		for (int z = 0; z < 2 * Z; z += 4) {
			using simd::float_4;
			float_4 y1 = float_4::load(&table[o][z]);
			int o2 = (o + 1) % O;
			int z2 = z + (o + 1) / O;
			float_4 y2 = float_4::load(&table[o2][z2]);
			float_4 y = y1 + t * (y2 - y1);
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


static const MinBlep<16, 16>& getMinBlep() {
	static MinBlep<16, 16> minBlep;
	return minBlep;
}


template <typename T>
struct VCOProcessor {
	T phase = 0.f;
	/** 1 for forward, -1 for backward. */
	T syncDirection = 1.f;
	T lastSync = 0.f;
	/** 1 or -1 */
	T lastSqrState = 1.f;

	OnePoleHighpass dcFilter;
	OnePoleHighpass::State<T> dcFilterStateSqr;
	OnePoleHighpass::State<T> dcFilterStateSaw;
	OnePoleHighpass::State<T> dcFilterStateTri;
	OnePoleHighpass::State<T> dcFilterStateSin;

	MinBlepBuffer<16*2, T> sqrMinBlep;
	MinBlepBuffer<16*2, T> sawMinBlep;
	MinBlepBuffer<16*2, T> triMinBlep;
	MinBlepBuffer<16*2, T> sinMinBlep;

	void setSampleTime(float sampleTime) {
		dcFilter.setCutoffApprox(std::min(0.4f, 20.f * sampleTime));
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
		auto insertDiscontinuity = [&](T mask, T subsample, T magnitude, MinBlepBuffer<16*2, T>& buffer) {
			int m = simd::movemask(mask);
			if (!m)
				return;
			for (int i = 0; i < frame.channels; i++) {
				if (m & (1 << i)) {
					float* x = (float*) buffer.startData();
					getMinBlep().insertDiscontinuity(subsample[i], magnitude[i], &x[i], 4);
				}
			}
		};

		auto insertSlopeDiscontinuity = [&](T mask, T subsample, T magnitude, MinBlepBuffer<16*2, T>& buffer) {
			int m = simd::movemask(mask);
			if (!m)
				return;
			for (int i = 0; i < frame.channels; i++) {
				if (m & (1 << i)) {
					float* x = (float*) buffer.startData();
					getMinBlep().insertSlopeDiscontinuity(subsample[i], magnitude[i], &x[i], 4);
				}
			}
		};

		// Computes subsample time where phase crosses threshold.
		auto getCrossing = [](T thresholdPhase, T startPhase, T endPhase, T startSubsample, T endSubsample) -> T {
			T delta = endPhase - startPhase;
			T diff = thresholdPhase - startPhase;
			// Forward: wrap thresholdPhase to (startPhase, startPhase+1]
			// Backward: wrap thresholdPhase to [startPhase-1, startPhase)
			thresholdPhase -= simd::ifelse(delta >= 0.f, simd::floor(diff), simd::ceil(diff));
			T p = (thresholdPhase - startPhase) / delta;
			return startSubsample + p * (endSubsample - startSubsample);
		};

		// Processes wrap/pulse/saw crossings between startPhase and endPhase.
		// startSubsample and endSubsample define the time range within the frame.
		// channelMask limits processing to specific channels.
		auto processCrossings = [&](T startPhase, T endPhase, T startSubsample, T endSubsample, T channelMask) {
			if (frame.sqrEnabled) {
				// Insert minBLEP to square when phase crosses 0 (mod 1)
				T wrapSubsample = getCrossing(1.f, startPhase, endPhase, startSubsample, endSubsample);
				T mask = channelMask & (startSubsample < wrapSubsample) & (wrapSubsample <= endSubsample);
				insertDiscontinuity(mask, wrapSubsample, 2.f * syncDirection, sqrMinBlep);

				// Insert minBLEP to square when phase crosses pulse width
				T pulseSubsample = getCrossing(pulseWidth, startPhase, endPhase, startSubsample, endSubsample);
				mask = channelMask & (startSubsample < pulseSubsample) & (pulseSubsample <= endSubsample);
				insertDiscontinuity(mask, pulseSubsample, -2.f * syncDirection, sqrMinBlep);
			}

			if (frame.sawEnabled) {
				// Insert minBLEP to saw when crossing 0.5
				T sawSubsample = getCrossing(0.5f, startPhase, endPhase, startSubsample, endSubsample);
				T mask = channelMask & (startSubsample < sawSubsample) & (sawSubsample <= endSubsample);
				insertDiscontinuity(mask, sawSubsample, -2.f * syncDirection, sawMinBlep);
			}

			if (frame.triEnabled) {
				// Insert minBLAMP to tri when crossing 0.25
				T triSubsample = getCrossing(0.25f, startPhase, endPhase, startSubsample, endSubsample);
				T mask = channelMask & (startSubsample < triSubsample) & (triSubsample <= endSubsample);
				// Slope goes from +4 to -4, so slope jump is -8 * abs(deltaPhase)
				insertSlopeDiscontinuity(mask, triSubsample, -8.f * deltaPhase * syncDirection, triMinBlep);

				// Insert minBLAMP to tri when crossing 0.75, slope from -4 to +4
				triSubsample = getCrossing(0.75f, startPhase, endPhase, startSubsample, endSubsample);
				mask = channelMask & (startSubsample < triSubsample) & (triSubsample <= endSubsample);
				// Slope goes from -4 to +4, so slope jump is 8 * abs(deltaPhase)
				insertSlopeDiscontinuity(mask, triSubsample, 8.f * deltaPhase * syncDirection, triMinBlep);
			}
		};

		// Check if square value changed due to pulseWidth changing since last frame
		if (frame.sqrEnabled) {
			T sqrState = sqr(prevPhase, pulseWidth);
			T magnitude = sqrState - lastSqrState;
			T changed = (magnitude != 0.f);
			insertDiscontinuity(changed, 1e-6f, magnitude, sqrMinBlep);
		}

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
			// Check if sync rises through 0
			T syncOccurred = (0.f < syncSubsample) & (syncSubsample <= 1.f) & (deltaSync >= 0.f);
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
				// Wrap sync phase
				syncPhase -= simd::floor(syncPhase);

				if (frame.soft) {
					// Soft sync: Reverse direction, continue from syncPhase

					if (frame.sawEnabled) {
						// Saw slope reverses
						// +2 slope becomes -2 if deltaPhase > 0
						// -2 slope becomes +2 if deltaPhase < 0
						insertSlopeDiscontinuity(syncOccurred, syncSubsample, -4.f * deltaPhase, sawMinBlep);
					}
					if (frame.triEnabled) {
						// Tri slope reverses
						// -4 slope becomes +4 if 0.25 < phase < 0.75
						// -4 slope becomes +4 otherwise
						T descending = (0.25f < syncPhase) & (syncPhase < 0.75f);
						T slopeJump = simd::ifelse(descending, 8.f, -8.f);
						insertSlopeDiscontinuity(syncOccurred, syncSubsample, slopeJump * deltaPhase, triMinBlep);
					}

					syncDirection = simd::ifelse(syncOccurred, -syncDirection, syncDirection);
					deltaPhase = simd::ifelse(syncOccurred, -deltaPhase, deltaPhase);
					T endPhase = syncPhase + deltaPhase * (1.f - syncSubsample);
					processCrossings(syncPhase, endPhase, syncSubsample, 1.f, syncOccurred);
					phase = simd::ifelse(syncOccurred, endPhase, phase);
				}
				else {
					// Hard sync: Reset phase from syncPhase to 0 at syncSubsample, insert discontinuities

					if (frame.sqrEnabled) {
						// Check if square jumps from -1 to +1
						T sqrJump = (syncPhase >= pulseWidth);
						insertDiscontinuity(syncOccurred & sqrJump, syncSubsample, 2.f, sqrMinBlep);
					}
					if (frame.sawEnabled) {
						// Saw jumps from saw(syncPhase) to saw(0) = 0
						insertDiscontinuity(syncOccurred, syncSubsample, -saw(syncPhase), sawMinBlep);
					}
					if (frame.triEnabled) {
						// Tri jumps from tri(syncPhase) to tri(0) = 0
						insertDiscontinuity(syncOccurred, syncSubsample, -tri(syncPhase), triMinBlep);
						// If descending slope (-4), reset to ascending slope (+4)
						T wasDescending = (0.25f < syncPhase) & (syncPhase < 0.75f);
						insertSlopeDiscontinuity(syncOccurred & wasDescending, syncSubsample, 8.f * deltaPhase, triMinBlep);
					}
					if (frame.sinEnabled) {
						// sin jumps from sin(syncPhase) to sin(0) = 0
						insertDiscontinuity(syncOccurred, syncSubsample, -sin(syncPhase), sinMinBlep);
						// Slope changes from sinDerivative(syncPhase) to sinDerivative(0) = 2pi
						// T slopeJump = T(2 * M_PI) - sinDerivative(syncPhase);
						// insertSlopeDiscontinuity(syncOccurred, syncSubsample, slopeJump * deltaPhase, sinMinBlep);
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

		if (frame.sqrEnabled) {
			frame.sqr = sqr(phase, pulseWidth);
			lastSqrState = frame.sqr;
			frame.sqr += sqrMinBlep.shift();
			frame.sqr = dcFilter.process(dcFilterStateSqr, frame.sqr);
		}

		if (frame.triEnabled) {
			frame.tri = tri(phase);
			frame.tri += triMinBlep.shift();
			frame.tri = dcFilter.process(dcFilterStateTri, frame.tri);
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

	static T sqr(T phase, T pulseWidth) {
		return simd::ifelse(phase < pulseWidth, 1.f, -1.f);
	}
	static T saw(T phase) {
		T x = phase + 0.5f;
		x -= simd::trunc(x);
		return 2 * x - 1;
	}
	static T tri(T phase) {
		return 1 - 4 * simd::fmin(simd::fabs(phase - 0.25f), 1.25f - phase);
	}
	static T sin(T phase) {
		return sin_2pi_9(phase);
	}
	static T sinDerivative(T phase) {
		// Shift and wrap cosine
		phase += 0.25f;
		phase -= simd::floor(phase);
		return T(2 * M_PI) * sin_2pi_9(phase);
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
		configParam(FREQ_PARAM, -76.f, 76.f, 0.f, "Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
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
