#include "plugin.hpp"


/** Approximates 1/x, using the rcp() instruction and 1 Newton-Raphson refinement */
template <typename T>
inline T rcp_newton1(T x) {
	T r = simd::rcp(x);
	return r * (T(2) - x * r);
}


/** Approximates 1/sqrt(x), using the rsqrt() instruction and 1 Newton-Raphson refinement */
template <typename T>
inline T rsqrt_newton1(T x) {
	T y = simd::rsqrt(x);
	return y * (T(3) - x * y * y) * T(0.5);
}


/** Approximates tan(x) for x in [0, pi*0.5).
Optimized coefficients for max relative error: 2.78e-05.
*/
template <typename T>
inline T tan_1_2(T x) {
	T x2 = x * x;
	T num = T(1) + x2 * T(-0.09776575533683811);
	T den = T(1) + x2 * (T(-0.43119539396382) + x2 * T(0.0105011966117302));
	return x * num / den;
}


/** 1st-order ADAA for softclip: f(x) = x/sqrt(x²+1), F(x) = sqrt(x²+1). */
template <typename T>
struct SoftclipADAA1 {
	T xPrev = T(0);
	T fPrev = T(0); // f(0) = 0
	T FPrev = T(1); // F(0) = sqrt(0+1) = 1

	void reset() {
		xPrev = T(0);
		fPrev = T(0);
		FPrev = T(1);
	}

	T process(T x) {
		const T eps = T(1e-5);
		T diff = x - xPrev;

		// f = x/sqrt(x^2+1), F = sqrt(x^2+1)
		T x2p1 = x * x + T(1);
		T r = rsqrt_newton1(x2p1);
		T f = x * r;
		T F = x2p1 * r;

		T adaaResult = (F - FPrev) * rcp_newton1(diff);
		T fallbackResult = (f + fPrev) * T(0.5);

		T y = simd::ifelse(simd::abs(diff) > eps, adaaResult, fallbackResult);

		xPrev = x;
		fPrev = f;
		FPrev = F;
		return y;
	}
};


/** 4-pole transistor ladder filter using TPT (Topology-Preserving Transform).
Feedback uses previous sample (i.e. 1 sample delayed) to avoid iterative solving.

For the equivalent state / trapezoidal integrator approach:
Zavalishin, V. "The Art of VA Filter Design". 2018
https://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_2.1.2.pdf

For Cytomic's formulation:
https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf

For antiderivative anti-aliasing (ADAA):
Parker, J., Zavalishin, V., Le Bihan, E. "Reducing the Aliasing of Nonlinear Waveshaping Using Continuous-Time Convolution". DAFx 2016
https://dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf
*/
template <typename T>
struct LadderFilter {
	T state[4];
	SoftclipADAA1<T> adaa[5];

	struct Frame {
		T input;
		/** Normalized cutoff frequency fc/fs in [0, 0.5) */
		T cutoff;
		T resonance;

		/** 4-pole lowpass output. */
		T lowpass4;
		/** 4-pole highpass output. */
		T highpass4;
	};

	LadderFilter() {
		reset();
	}

	void reset() {
		for (int i = 0; i < 4; i++) {
			state[i] = T(0);
		}
		for (int i = 0; i < 5; i++) {
			adaa[i].reset();
		}
	}

	void process(Frame& frame) {
		// Pre-warped frequency
		T g = tan_1_2(T(M_PI) * frame.cutoff);
		// Integrator gain
		T G = g / (T(1) + g);

		// Feedback path
		T feedback = adaa[4].process(frame.resonance * state[3]);
		T u = frame.input - feedback;

		// Stage 0
		T sat0 = adaa[0].process(u);
		T v0 = G * (sat0 - state[0]);
		T y0 = v0 + state[0];
		state[0] = y0 + v0;

		// Stage 1
		T sat1 = adaa[1].process(y0);
		T v1 = G * (sat1 - state[1]);
		T y1 = v1 + state[1];
		state[1] = y1 + v1;

		// Stage 2
		T sat2 = adaa[2].process(y1);
		T v2 = G * (sat2 - state[2]);
		T y2 = v2 + state[2];
		state[2] = y2 + v2;

		// Stage 3
		T sat3 = adaa[3].process(y2);
		T v3 = G * (sat3 - state[3]);
		T y3 = v3 + state[3];
		state[3] = y3 + v3;

		frame.lowpass4 = y3;
		frame.highpass4 = sat0 - T(4) * y0 + T(6) * y1 - T(4) * y2 + y3;
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
		const float defaultFreq = (minFreq + maxFreq) / 2.f;
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
			float_4 cutoff = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch);
			frame.cutoff = simd::clamp(cutoff * args.sampleTime, 0.f, 0.499f);

			// Process
			filters[c / 4].process(frame);

			// Outputs
			outputs[LPF_OUTPUT].setVoltageSimd(frame.lowpass4 * 5.f, c);
			outputs[HPF_OUTPUT].setVoltageSimd(frame.highpass4 * 5.f, c);
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
