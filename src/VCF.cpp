#include "Fundamental.hpp"
#include "dsp/functions.hpp"
#include "dsp/resampler.hpp"


/**
This code is derived from the algorithm described in "Modeling and Measuring a
Moog Voltage-Controlled Filter, by Effrosyni Paschou, Fabián Esqueda, Vesa Välimäki
and John Mourjopoulos, for APSIPA Annual Summit and Conference 2017.

It is a 0df algorithm using Newton-Raphson's method (4 iterations) for solving
implicit equations, and midpoint integration method.

Derived from the article and adapted to VCV Rack by Ivan COHEN.
*/
struct LadderFilter0dfMidPoint {
	float cutoff = 1000.f;
	float resonance = 1.0f;
	float gainFactor = 0.2f;
	float A;
	/** State variables */
	float lastInput = 0.f;
	float lastV[4] = {};
	/** Outputs */
	float lowpass;
	float highpass;


	LadderFilter0dfMidPoint() {
		float VT = 26e-3f;
		A = gainFactor / (2.f * VT);
	}

	inline float clip(float x) {
		return tanhf(x);
	}

	void process(float input, float dt) {
		float F[4];
		float V[4];
		float wc = 2 * M_PI * cutoff / A;

		input *= gainFactor;

		for (int i = 0; i < 4; i++) {
			V[i] = lastV[i];
		}

		for (int i = 0; i < 4; i++) {
			float S0 = A * (input + lastInput + resonance * (V[3] + lastV[3])) * 0.5f;
			float S1 = A * (V[0] + lastV[0]) * 0.5f;
			float S2 = A * (V[1] + lastV[1]) * 0.5f;
			float S3 = A * (V[2] + lastV[2]) * 0.5f;
			float S4 = A * (V[3] + lastV[3]) * 0.5f;

			float t0 = clip(S0);
			float t1 = clip(S1);
			float t2 = clip(S2);
			float t3 = clip(S3);
			float t4 = clip(S4);

			// F function (the one for which we need to find the roots with NR)
			F[0] = V[0] - lastV[0] + wc * dt * (t0 + t1);
			F[1] = V[1] - lastV[1] - wc * dt * (t1 - t2);
			F[2] = V[2] - lastV[2] - wc * dt * (t2 - t3);
			F[3] = V[3] - lastV[3] - wc * dt * (t3 - t4);

			float g = wc * dt * A * 0.5f;

			// derivatives of ti
			float J0 = 1 - t0 * t0;
			float J1 = 1 - t1 * t1;
			float J2 = 1 - t2 * t2;
			float J3 = 1 - t3 * t3;
			float J4 = 1 - t4 * t4;

			// Jacobian matrix elements
			float a11 = 1 + g * J1;
			float a14 = resonance * g * J0;
			float a21 = -g * J1;
			float a22 = 1 + g * J2;
			float a32 = -g * J2;
			float a33 = 1 + g * J3;
			float a43 = -g * J3;
			float a44 = 1 + g * J4;

			// Newton-Raphson algorithm with Jacobian inverting
			float deninv = 1.f / (a11 * a22 * a33 * a44 - a14 * a21 * a32 * a43);

			float delta0 = ( F[0] * a22 * a33 * a44 - F[1] * a14 * a32 * a43 + F[2] * a14 * a22 * a43 - F[3] * a14 * a22 * a33) * deninv;
			float delta1 = (-F[0] * a21 * a33 * a44 + F[1] * a11 * a33 * a44 - F[2] * a14 * a21 * a43 + F[3] * a14 * a21 * a33) * deninv;
			float delta2 = ( F[0] * a21 * a32 * a44 - F[1] * a11 * a32 * a44 + F[2] * a11 * a22 * a44 - F[3] * a14 * a21 * a32) * deninv;
			float delta3 = (-F[0] * a21 * a32 * a43 + F[1] * a11 * a32 * a43 - F[2] * a11 * a22 * a43 + F[3] * a11 * a22 * a33) * deninv;

			delta0 = isfinite(delta0) ? delta0 : 0.f;
			delta1 = isfinite(delta1) ? delta1 : 0.f;
			delta2 = isfinite(delta2) ? delta2 : 0.f;
			delta3 = isfinite(delta3) ? delta3 : 0.f;

			V[0] -= delta0;
			V[1] -= delta1;
			V[2] -= delta2;
			V[3] -= delta3;
		}

		lastInput = input;
		for (int i = 0; i < 4; i++)
			lastV[i] = V[i];

		// outputs are inverted
		lowpass = -clip(V[3] / gainFactor);
		highpass = -clip(((input + resonance * V[3]) + 4 * V[0] - 6 * V[1] + 4 * V[2] - V[3]) / gainFactor);
	}

	void reset() {
		for (int i = 0; i < 4; i++) {
			lastV[i] = 0.f;
		}
		lastInput = 0.f;
	}
};


struct VCF : Module {
	enum ParamIds {
		FREQ_PARAM,
		FINE_PARAM,
		RES_PARAM,
		FREQ_CV_PARAM,
		DRIVE_PARAM,
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

	LadderFilter0dfMidPoint filter;

	VCF() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}

	void onReset() override {
		filter.reset();
	}

	void step() override {
		if (!outputs[LPF_OUTPUT].active && !outputs[HPF_OUTPUT].active) {
			outputs[LPF_OUTPUT].value = 0.f;
			outputs[HPF_OUTPUT].value = 0.f;
			return;
		}

		float input = inputs[IN_INPUT].value / 5.f;
		float drive = clamp(params[DRIVE_PARAM].value + inputs[DRIVE_INPUT].value / 10.f, 0.f, 1.f);
		float gain = powf(1.f + drive, 5);
		input *= gain;

		// Add -60dB noise to bootstrap self-oscillation
		input += 1e-6f * (2.f * randomUniform() - 1.f);

		// Set resonance
		float res = clamp(params[RES_PARAM].value + inputs[RES_INPUT].value / 10.f, 0.f, 1.f);
		filter.resonance = powf(res, 2) * 10.f;

		// Set cutoff frequency
		float pitch = inputs[FREQ_INPUT].value * quadraticBipolar(params[FREQ_CV_PARAM].value);
		pitch += params[FREQ_PARAM].value * 10.f - 3.f;
		pitch += quadraticBipolar(params[FINE_PARAM].value * 2.f - 1.f) * 7.f / 12.f;
		float cutoff = 261.626f * powf(2.f, pitch);
		filter.cutoff = clamp(cutoff, 1.f, 20000.f);

		// Step the filter
		filter.process(input, engineGetSampleTime());

		// Set outputs
		outputs[LPF_OUTPUT].value = 5.f * filter.lowpass;
		outputs[HPF_OUTPUT].value = 5.f * filter.highpass;
	}
};


struct VCFWidget : ModuleWidget {
	VCFWidget(VCF *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/VCF.svg")));

		addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 30, 365)));

		addParam(ParamWidget::create<RoundHugeBlackKnob>(Vec(33, 61), module, VCF::FREQ_PARAM, 0.f, 1.f, 0.5f));
		addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(12, 143), module, VCF::FINE_PARAM, 0.f, 1.f, 0.5f));
		addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(71, 143), module, VCF::RES_PARAM, 0.f, 1.f, 0.f));
		addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(12, 208), module, VCF::FREQ_CV_PARAM, -1.f, 1.f, 0.f));
		addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(71, 208), module, VCF::DRIVE_PARAM, 0.f, 1.f, 0.f));

		addInput(Port::create<PJ301MPort>(Vec(10, 276), Port::INPUT, module, VCF::FREQ_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(48, 276), Port::INPUT, module, VCF::RES_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(85, 276), Port::INPUT, module, VCF::DRIVE_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(10, 320), Port::INPUT, module, VCF::IN_INPUT));

		addOutput(Port::create<PJ301MPort>(Vec(48, 320), Port::OUTPUT, module, VCF::LPF_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(85, 320), Port::OUTPUT, module, VCF::HPF_OUTPUT));
	}
};



Model *modelVCF = Model::create<VCF, VCFWidget>("Fundamental", "VCF", "VCF", FILTER_TAG);
