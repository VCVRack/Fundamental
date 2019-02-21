#include "plugin.hpp"


static float clip(float x) {
	return std::tanh(x);
}

struct LadderFilter {
	float omega0;
	float resonance = 1.f;
	float state[4];
	float input;
	float lowpass;
	float highpass;

	LadderFilter() {
		reset();
		setCutoff(0.f);
	}

	void reset() {
		for (int i = 0; i < 4; i++) {
			state[i] = 0.f;
		}
	}

	void setCutoff(float cutoff) {
		omega0 = 2.f*M_PI * cutoff;
	}

	void process(float input, float dt) {
		dsp::stepRK4(0.f, dt, state, 4, [&](float t, const float x[], float dxdt[]) {
			float inputc = clip(input - resonance * x[3]);
			float yc0 = clip(x[0]);
			float yc1 = clip(x[1]);
			float yc2 = clip(x[2]);
			float yc3 = clip(x[3]);

			dxdt[0] = omega0 * (inputc - yc0);
			dxdt[1] = omega0 * (yc0 - yc1);
			dxdt[2] = omega0 * (yc1 - yc2);
			dxdt[3] = omega0 * (yc2 - yc3);
		});

		lowpass = state[3];
		// TODO This is incorrect when `resonance > 0`. Is the math wrong?
		highpass = clip((input - resonance*state[3]) - 4 * state[0] + 6*state[1] - 4*state[2] + state[3]);
	}
};


static const int UPSAMPLE = 2;

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

	LadderFilter filter;
	// Upsampler<UPSAMPLE, 8> inputUpsampler;
	// Decimator<UPSAMPLE, 8> lowpassDecimator;
	// Decimator<UPSAMPLE, 8> highpassDecimator;

	VCF() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
		params[FREQ_PARAM].config(0.f, 1.f, 0.5f, "Frequency");
		params[FINE_PARAM].config(0.f, 1.f, 0.5f, "Fine frequency");
		params[RES_PARAM].config(0.f, 1.f, 0.f, "Resonance");
		params[FREQ_CV_PARAM].config(-1.f, 1.f, 0.f, "Frequency modulation");
		params[DRIVE_PARAM].config(0.f, 1.f, 0.f, "Drive");
	}

	void onReset() override {
		filter.reset();
	}

	void process(const ProcessArgs &args) override {
		if (!outputs[LPF_OUTPUT].isConnected() && !outputs[HPF_OUTPUT].isConnected()) {
			outputs[LPF_OUTPUT].setVoltage(0.f);
			outputs[HPF_OUTPUT].setVoltage(0.f);
			return;
		}

		float input = inputs[IN_INPUT].getVoltage() / 5.f;
		float drive = clamp(params[DRIVE_PARAM].getValue() + inputs[DRIVE_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		float gain = std::pow(1.f + drive, 5);
		input *= gain;

		// Add -60dB noise to bootstrap self-oscillation
		input += 1e-6f * (2.f * random::uniform() - 1.f);

		// Set resonance
		float res = clamp(params[RES_PARAM].getValue() + inputs[RES_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		filter.resonance = std::pow(res, 2) * 10.f;

		// Set cutoff frequency
		float pitch = 0.f;
		if (inputs[FREQ_INPUT].isConnected())
			pitch += inputs[FREQ_INPUT].getVoltage() * dsp::quadraticBipolar(params[FREQ_CV_PARAM].getValue());
		pitch += params[FREQ_PARAM].getValue() * 10.f - 5.f;
		pitch += dsp::quadraticBipolar(params[FINE_PARAM].getValue() * 2.f - 1.f) * 7.f / 12.f;
		float cutoff = 261.626f * std::pow(2.f, pitch);
		cutoff = clamp(cutoff, 1.f, 8000.f);
		filter.setCutoff(cutoff);

		/*
		// Process sample
		float dt = args.sampleTime / UPSAMPLE;
		float inputBuf[UPSAMPLE];
		float lowpassBuf[UPSAMPLE];
		float highpassBuf[UPSAMPLE];
		inputUpsampler.process(input, inputBuf);
		for (int i = 0; i < UPSAMPLE; i++) {
			// Step the filter
			filter.process(inputBuf[i], dt);
			lowpassBuf[i] = filter.lowpass;
			highpassBuf[i] = filter.highpass;
		}

		// Set outputs
		if (outputs[LPF_OUTPUT].isConnected()) {
			outputs[LPF_OUTPUT].setVoltage(5.f * lowpassDecimator.process(lowpassBuf));
		}
		if (outputs[HPF_OUTPUT].isConnected()) {
			outputs[HPF_OUTPUT].setVoltage(5.f * highpassDecimator.process(highpassBuf));
		}
		*/
		filter.process(input, args.sampleTime);
		outputs[LPF_OUTPUT].setVoltage(5.f * filter.lowpass);
		outputs[HPF_OUTPUT].setVoltage(5.f * filter.highpass);
	}
};


struct VCFWidget : ModuleWidget {
	VCFWidget(VCF *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/VCF.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

		addParam(createParam<RoundHugeBlackKnob>(Vec(33, 61), module, VCF::FREQ_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(12, 143), module, VCF::FINE_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(71, 143), module, VCF::RES_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(12, 208), module, VCF::FREQ_CV_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(71, 208), module, VCF::DRIVE_PARAM));

		addInput(createInput<PJ301MPort>(Vec(10, 276), module, VCF::FREQ_INPUT));
		addInput(createInput<PJ301MPort>(Vec(48, 276), module, VCF::RES_INPUT));
		addInput(createInput<PJ301MPort>(Vec(85, 276), module, VCF::DRIVE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(10, 320), module, VCF::IN_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(48, 320), module, VCF::LPF_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(85, 320), module, VCF::HPF_OUTPUT));
	}
};


Model *modelVCF = createModel<VCF, VCFWidget>("VCF");
