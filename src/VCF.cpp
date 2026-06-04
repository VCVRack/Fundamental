#include "plugin.hpp"


/** Approximates 1/x using the rcp() instruction with one Newton-Raphson refinement.
Max relative error 1.7e-7.
*/
template <typename T>
inline T rcp_newton1(T x) {
	T r = simd::rcp(x);
	return r * (T(2) - x * r);
}


/** Approximates 1/sqrt(x) using the rsqrt() instruction with one Newton-Raphson refinement.
Max relative error 2.9e-7.
*/
template <typename T>
inline T rsqrt_newton1(T x) {
	T y = simd::rsqrt(x);
	return y * (T(3) - x * y * y) * T(0.5);
}


/** Approximates tan(x) using (3,4) rational function.
For x in (-pi/2, pi/2), max relative error 2.8e-5.
*/
template <typename T>
inline T tan_3_4(T x) {
	T x2 = x * x;
	T num = T(1) + x2 * T(-0.09776575533683811);
	T den = T(1) + x2 * (T(-0.43119539396382) + x2 * T(0.0105011966117302));
	return x * num / den;
}


/** Approximates tanh(x)/x using (4,4) rational function.
For x in [-4, 4], max relative error 2.3e-3.
Approaches 1/15 as |x| -> infinity.
*/
template <typename T>
inline T tanhXdX_4_4(T x) {
	T x2 = x * x;
	T num = (x2 + T(105)) * x2 + T(945);
	T den = (T(15) * x2 + T(420)) * x2 + T(945);
	return num / den;
}


/** Approximates tanh(x)/x using (4,6) rational function.
For x in [-4, 4], max relative error 4.2e-6.
Approaches 0 as |x| -> infinity.
*/
template <typename T>
inline T tanhXdX_4_6(T x) {
	T x2 = x * x;
	T num = T(1) + x2 * (T(0.121953514066257) + x2 * T(0.00204623480007919));
	T den = T(1) + x2 * (T(0.455305674254515) + x2 * (T(0.0204552909446164) + x2 * T(9.48027717633287e-05)));
	return num / den;
}


/** Processes one input sample through a cascade of first-order allpass sections in transposed direct-form II.

Each section implements H(z) = (a + z^-1) / (1 + a * z^-1).
state[] and coefficients[] must both have at least `sections` elements.
state[] is updated in place.
*/
template <typename T>
static inline T firstOrderAllpassCascade(T x, T* state, const float* coefficients, int sections) {
	for (int i = 0; i < sections; i++) {
		T a = T(coefficients[i]);
		T y = a * x + state[i];
		state[i] = x - a * y;
		x = y;
	}
	return x;
}


/** Coefficients for a polyphase IIR halfband for 2x audio resampling.

The filter has two parallel branches, A and B.
Each branch is a chain of three first-order allpass sections of the form (a + z^-2) / (1 + a * z^-2).
In branch A, the input first passes through a section with a=_A[0], then a section with a=_A[1], then a section with a=_A[2].
Branch B does the same with _B.
The branches combine as H(z) = (1/2) * (A_a(z^2) + z^-1 * A_b(z^2)).
The z^-1 on A_b delays branch B by one sample at the oversampled (2x) rate.
*/
static constexpr float HALFBAND_2X_COEFFICIENTS_A[3] = {
	0.062822416060049985f, 0.4243808557204406f, 0.7818614603969013f,
};
static constexpr float HALFBAND_2X_COEFFICIENTS_B[3] = {
	0.22380733034648345f, 0.61653443504951111f, 0.92747359487482584f,
};


/** 2x upsampler using a polyphase halfband filter.

Each call writes two oversampled-rate samples to out from one input sample.
The first array element is the even-indexed sample and the second is the odd-indexed sample.
*/
template <typename T>
struct HalfbandUpsampler2x {
	T stateA[3] = {};
	T stateB[3] = {};

	void reset() {
		*this = HalfbandUpsampler2x{};
	}

	void process(T input, T* out) {
		out[0] = firstOrderAllpassCascade(input, stateA, HALFBAND_2X_COEFFICIENTS_A, 3);
		out[1] = firstOrderAllpassCascade(input, stateB, HALFBAND_2X_COEFFICIENTS_B, 3);
	}
};


/** 2x downsampler using a polyphase halfband filter.

Each call returns one output sample from two oversampled-rate samples.
The first array element is the even-indexed sample and the second is the odd-indexed sample.
*/
template <typename T>
struct HalfbandDownsampler2x {
	T stateA[3] = {};
	T stateB[3] = {};

	void reset() {
		*this = HalfbandDownsampler2x{};
	}

	T process(const T* in) {
		// Cascade A processes the odd-indexed input and cascade B the even-indexed.
		// The swap relative to the upsampler is what makes the chain unity gain in the passband without an extra delay buffer.
		T outputA = firstOrderAllpassCascade(in[1], stateA, HALFBAND_2X_COEFFICIENTS_A, 3);
		T outputB = firstOrderAllpassCascade(in[0], stateB, HALFBAND_2X_COEFFICIENTS_B, 3);
		return T(0.5) * (outputA + outputB);
	}
};


/** Resonant 4-pole ladder filter with internal 2x oversampling, producing lowpass and highpass outputs.

Four cascaded 1-pole TPT integrators with per-stage tanh saturation and an explicit softclip resonance feedback path.
The 2x oversampling (polyphase halfband resamplers) lets the per-pole cutoff reach nyquist without aliasing the resonance peak.
The cutoff is each per-stage 1-pole's -3 dB frequency, also the self-oscillation pitch at full resonance.
The four-pole cumulative -3 dB lands at sqrt(2^(1/4) - 1) ~= 0.435 times the cutoff.
Self-oscillation occurs near a resonance value of 4.

The input is softclipped before the first stage.
Per-stage saturator gains use Mystran's single-sample linearization of tanh at each stage's previous sample output.
The linearization gains are floored to a minimum so the integrators stay self-damping when state grows large.
The global feedback applies an explicit r * y / sqrt(1 + y*y) softclip on the previous lowpass output rather than a Mystran-coupled feedback, sidestepping the limit cycle that recursive coupling would otherwise produce near the input rate's nyquist.

Topology-preserving transform integrator framework:
Zavalishin, V. "The Art of VA Filter Design", 2018.
https://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_2.1.2.pdf

Mystran (Teemu Voipio) per-stage linearization:
https://www.kvraudio.com/forum/viewtopic.php?p=4925309#p4925309
*/
template <typename T>
struct LadderFilter {
	/** Polyphase halfband upsampler from the input rate to the oversampled rate. */
	HalfbandUpsampler2x<T> upsampler;
	/** Polyphase halfband downsampler for the lowpass output. */
	HalfbandDownsampler2x<T> downsamplerLowpass;
	/** Polyphase halfband downsampler for the highpass output. */
	HalfbandDownsampler2x<T> downsamplerHighpass;
	/** Trapezoidal integrator state, one element per pole. */
	T s[4] = {};
	/** Previous sample value of u0, the signal entering stage 0's saturator.
	Used as that saturator's Mystran linearization point.
	*/
	T previousU0 = 0;
	/** Previous sample outputs of the four ladder stages, indexed 0..3.
	previousY[i] for i in 0..2 is the Mystran linearization point for stage i+1's saturator.
	previousY[3] is the argument to the resonance feedback softclip.
	*/
	T previousY[4] = {};

	struct Frame {
		// Inputs
		T input;
		/** Per-pole -3 dB frequency, normalized to the sample rate.
		Each of the four stages uses this same cutoff, which equals the self-oscillation pitch at full resonance.
		The four-pole cumulative -3 dB sits at sqrt(2^(1/4) - 1) ~= 0.435 times this value.
		The caller must clamp to [0, 0.499].
		*/
		T cutoff;
		/** The filter self-oscillates near 4. */
		T resonance;
		bool computeLowpass = true;
		bool computeHighpass = true;

		// Outputs
		T lowpass = 0;
		T highpass = 0;
	};

	void reset() {
		*this = LadderFilter{};
	}

	void process(Frame& frame) {
		// Bilinear prewarp at the oversampled rate.
		T cutoffOversampled = frame.cutoff * T(0.5);
		T g = tan_3_4(T(M_PI) * cutoffOversampled);
		T onePlusG = T(1) + g;

		T xOversampled[2];
		upsampler.process(frame.input, xOversampled);

		// Ladder at the oversampled rate.
		T lowpassOversampled[2];
		T highpassOversampled[2];
		for (int n = 0; n < 2; n++) {
			// Softclip at the oversampled rate so the downsampler's lowpass attenuates the harmonics that would otherwise alias.
			T xSoftclipped = xOversampled[n] * rsqrt_newton1(T(1) + xOversampled[n] * xOversampled[n]);

			// Resonance feedback.
			T feedback = frame.resonance * previousY[3] * rsqrt_newton1(T(1) + previousY[3] * previousY[3]);
			T u0 = xSoftclipped - feedback;

			// Mystran per-stage linearization gains, floored so the integrators stay self-damping at large state.
			const T tMinimum = T(0.01);
			T t0 = simd::fmax(tanhXdX_4_6(previousU0), tMinimum);
			T t1 = simd::fmax(tanhXdX_4_6(previousY[0]), tMinimum);
			T t2 = simd::fmax(tanhXdX_4_6(previousY[1]), tMinimum);
			T t3 = simd::fmax(tanhXdX_4_6(previousY[2]), tMinimum);

			// TPT 1-pole integrators with linearized saturators in front.
			T y0 = (g * t0 * u0 + s[0]) / onePlusG;
			T y1 = (g * t1 * y0 + s[1]) / onePlusG;
			T y2 = (g * t2 * y1 + s[2]) / onePlusG;
			T y3 = (g * t3 * y2 + s[3]) / onePlusG;

			// Trapezoidal state update.
			s[0] = T(2) * y0 - s[0];
			s[1] = T(2) * y1 - s[1];
			s[2] = T(2) * y2 - s[2];
			s[3] = T(2) * y3 - s[3];

			previousU0 = u0;
			previousY[0] = y0;
			previousY[1] = y1;
			previousY[2] = y2;
			previousY[3] = y3;

			lowpassOversampled[n] = y3;
			// Highpass: binomial combination of the saturated stage-0 input and the partial lowpass outputs.
			T saturatedU0 = t0 * u0;
			highpassOversampled[n] = saturatedU0 - T(4) * y0 + T(6) * y1 - T(4) * y2 + y3;
		}

		if (frame.computeLowpass)
			frame.lowpass = downsamplerLowpass.process(lowpassOversampled);
		if (frame.computeHighpass)
			frame.highpass = downsamplerHighpass.process(highpassOversampled);
	}
};


using simd::float_4;


struct VCF : Module {
	enum ParamIds {
		FREQ_PARAM,
		FINE_PARAM, // removed in 2.0
		RES_PARAM,
		FREQ_CV_PARAM,
		DRIVE_PARAM,
		// Added in 2.0
		RES_CV_PARAM,
		DRIVE_CV_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FREQ_INPUT,
		RES_INPUT,
		DRIVE_INPUT,
		IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		LPF_OUTPUT,
		HPF_OUTPUT,
		NUM_OUTPUTS
	};

	LadderFilter<float_4> filters[4];

	VCF() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
		// To preserve backward compatibility with <2.0, FREQ_PARAM follows
		// freq = C4 * 2^(10 * param - 5)
		// or
		// param = (log2(freq / C4) + 5) / 10
		const float minFreq = (std::log2(8.f / dsp::FREQ_C4) + 5) / 10;
		const float maxFreq = (std::log2(22000.f / dsp::FREQ_C4) + 5) / 10;
		const float defaultFreq = 0.5f;
		configParam(FREQ_PARAM, minFreq, maxFreq, defaultFreq, "Cutoff frequency", " Hz", std::pow(2, 10.f), dsp::FREQ_C4 / std::pow(2, 5.f));
		configParam(RES_PARAM, 0.f, 1.f, 0.f, "Resonance", "%", 0.f, 100.f);
		configParam(RES_CV_PARAM, -1.f, 1.f, 0.f, "Resonance CV", "%", 0.f, 100.f);
		configParam(FREQ_CV_PARAM, -1.f, 1.f, 0.f, "Cutoff frequency CV", "%", 0.f, 100.f);
		// gain(drive) = (1 + drive)^5
		// gain(-1) = 0
		// gain(0) = 1
		// gain(1) = 32
		configParam(DRIVE_PARAM, -1.f, 1.f, 0.f, "Drive", "%", 0, 100, 100);
		configParam(DRIVE_CV_PARAM, -1.f, 1.f, 0.f, "Drive CV", "%", 0, 100);

		configInput(FREQ_INPUT, "Frequency");
		configInput(RES_INPUT, "Resonance");
		configInput(DRIVE_INPUT, "Drive");
		configInput(IN_INPUT, "Audio");

		configOutput(LPF_OUTPUT, "Lowpass filter");
		configOutput(HPF_OUTPUT, "Highpass filter");

		configBypass(IN_INPUT, LPF_OUTPUT);
		configBypass(IN_INPUT, HPF_OUTPUT);
	}

	void onReset() override {
		for (int i = 0; i < 4; i++) {
			filters[i].reset();
		}
	}

	void process(const ProcessArgs& args) override {
		if (!outputs[LPF_OUTPUT].isConnected() && !outputs[HPF_OUTPUT].isConnected()) {
			return;
		}

		float driveParam = params[DRIVE_PARAM].getValue();
		float driveCvParam = params[DRIVE_CV_PARAM].getValue();
		float resParam = params[RES_PARAM].getValue();
		float resCvParam = params[RES_CV_PARAM].getValue();
		float freqParam = params[FREQ_PARAM].getValue();
		// Rescale for backward compatibility
		freqParam = freqParam * 10.f - 5.f;
		float freqCvParam = params[FREQ_CV_PARAM].getValue();

		int channels = std::max(1, inputs[IN_INPUT].getChannels());

		for (int c = 0; c < channels; c += 4) {
			LadderFilter<float_4>::Frame frame;

			// Input
			float_4 input = inputs[IN_INPUT].getVoltageSimd<float_4>(c) / 5.f;

			// Drive
			float_4 drive = driveParam + inputs[DRIVE_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f * driveCvParam;
			drive = simd::clamp(drive, -1.f, 1.f);
			float_4 gain = simd::pow(1.f + drive, 5);
			input *= gain;

			// Add -120dB noise to bootstrap self-oscillation
			input += 1e-6f * (2.f * random::uniform() - 1.f);
			frame.input = input;

			// Resonance
			float_4 resonance = resParam + inputs[RES_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f * resCvParam;
			resonance = simd::clamp(resonance, 0.f, 1.f);
			frame.resonance = simd::pow(resonance, 2) * 10.f;

			// Cutoff frequency
			float_4 pitch = freqParam + inputs[FREQ_INPUT].getPolyVoltageSimd<float_4>(c) * freqCvParam;
			frame.cutoff = simd::clamp(dsp::FREQ_C4 * dsp::exp2_taylor5(pitch) * args.sampleTime, 0.f, 0.499f);

			frame.computeLowpass = outputs[LPF_OUTPUT].isConnected();
			frame.computeHighpass = outputs[HPF_OUTPUT].isConnected();

			// Process
			filters[c / 4].process(frame);

			// Outputs
			outputs[LPF_OUTPUT].setVoltageSimd(frame.lowpass * 5.f, c);
			outputs[HPF_OUTPUT].setVoltageSimd(frame.highpass * 5.f, c);
		}

		outputs[LPF_OUTPUT].setChannels(channels);
		outputs[HPF_OUTPUT].setChannels(channels);
	}

	void paramsFromJson(json_t* rootJ) override {
		// These attenuators didn't exist in version <2, so set to 1 in case they are not overwritten.
		params[RES_CV_PARAM].setValue(1.f);
		params[DRIVE_CV_PARAM].setValue(1.f);

		Module::paramsFromJson(rootJ);
	}
};


struct VCFWidget : ModuleWidget {
	VCFWidget(VCF* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/VCF.svg"), asset::plugin(pluginInstance, "res/VCF-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(17.587, 29.808)), module, VCF::FREQ_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(8.895, 56.388)), module, VCF::RES_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(26.665, 56.388)), module, VCF::DRIVE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.996, 80.603)), module, VCF::FREQ_CV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(17.833, 80.603)), module, VCF::RES_CV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(28.67, 80.603)), module, VCF::DRIVE_CV_PARAM));

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(6.996, 96.813)), module, VCF::FREQ_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(17.833, 96.813)), module, VCF::RES_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(28.67, 96.813)), module, VCF::DRIVE_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(6.996, 113.115)), module, VCF::IN_INPUT));

		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(17.833, 113.115)), module, VCF::LPF_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(28.67, 113.115)), module, VCF::HPF_OUTPUT));
	}
};


Model* modelVCF = createModel<VCF, VCFWidget>("VCF");
