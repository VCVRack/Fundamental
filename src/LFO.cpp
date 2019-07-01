#include "plugin.hpp"


using simd::float_4;


template <typename T>
struct LowFrequencyOscillator {
	T phase = 0.f;
	T pw = 0.5f;
	T freq = 1.f;
	bool invert = false;
	bool bipolar = false;
	T resetState = T::mask();

	void setPitch(T pitch) {
		pitch = simd::fmin(pitch, 10.f);
		freq = simd::pow(2.f, pitch);
	}
	void setPulseWidth(T pw) {
		const T pwMin = 0.01f;
		this->pw = clamp(pw, pwMin, 1.f - pwMin);
	}
	void setReset(T reset) {
		reset = simd::rescale(reset, 0.1f, 2.f, 0.f, 1.f);
		T on = (reset >= 1.f);
		T off = (reset <= 0.f);
		T triggered = ~resetState & on;
		resetState = simd::ifelse(off, 0.f, resetState);
		resetState = simd::ifelse(on, T::mask(), resetState);
		phase = simd::ifelse(triggered, 0.f, phase);
	}
	void step(float dt) {
		T deltaPhase = simd::fmin(freq * dt, 0.5f);
		phase += deltaPhase;
		phase -= (phase >= 1.f) & 1.f;
	}
	T sin() {
		T p = phase;
		if (!bipolar) p -= 0.25f;
		T v = simd::sin(2*M_PI * p);
		if (invert) v *= -1.f;
		if (!bipolar) v += 1.f;
		return v;
	}
	T tri() {
		T p = phase;
		if (bipolar) p += 0.25f;
		T v = 4.f * simd::fabs(p - simd::round(p)) - 1.f;
		if (invert) v *= -1.f;
		if (!bipolar) v += 1.f;
		return v;
	}
	T saw() {
		T p = phase;
		if (!bipolar) p -= 0.5f;
		T v = 2.f * (p - simd::round(p));
		if (invert) v *= -1.f;
		if (!bipolar) v += 1.f;
		return v;
	}
	T sqr() {
		T v = simd::ifelse(phase < pw, 1.f, -1.f);
		if (invert) v *= -1.f;
		if (!bipolar) v += 1.f;
		return v;
	}
	T light() {
		return simd::sin(2 * T(M_PI) * phase);
	}
};


struct LFO : Module {
	enum ParamIds {
		OFFSET_PARAM,
		INVERT_PARAM,
		FREQ_PARAM,
		FM1_PARAM,
		FM2_PARAM,
		PW_PARAM,
		PWM_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FM1_INPUT,
		FM2_INPUT,
		RESET_INPUT,
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

	LowFrequencyOscillator<float_4> oscillators[4];
	dsp::ClockDivider lightDivider;

	LFO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(OFFSET_PARAM, 0.f, 1.f, 1.f, "Offset");
		configParam(INVERT_PARAM, 0.f, 1.f, 1.f, "Invert");
		configParam(FREQ_PARAM, -8.f, 10.f, 1.f, "Frequency", " Hz", 2, 1);
		configParam(FM1_PARAM, 0.f, 1.f, 0.f, "Frequency modulation 1", "%", 0.f, 100.f);
		configParam(PW_PARAM, 0.01f, 0.99f, 0.5f, "Pulse width", "%", 0.f, 100.f);
		configParam(FM2_PARAM, 0.f, 1.f, 0.f, "Frequency modulation 2", "%", 0.f, 100.f);
		configParam(PWM_PARAM, 0.f, 1.f, 0.f, "Pulse width modulation", "%", 0.f, 100.f);
		lightDivider.setDivision(16);
	}

	void process(const ProcessArgs &args) override {
		float freqParam = params[FREQ_PARAM].getValue();
		float fm1Param = params[FM1_PARAM].getValue();
		float fm2Param = params[FM2_PARAM].getValue();
		float pwParam = params[PW_PARAM].getValue();
		float pwmParam = params[PWM_PARAM].getValue();

		int channels = std::max(1, inputs[FM1_INPUT].getChannels());

		for (int c = 0; c < channels; c += 4) {
			auto *oscillator = &oscillators[c / 4];
			oscillator->invert = (params[INVERT_PARAM].getValue() == 0.f);
			oscillator->bipolar = (params[OFFSET_PARAM].getValue() == 0.f);

			float_4 pitch = freqParam;
			// FM1, polyphonic
			pitch += inputs[FM1_INPUT].getVoltageSimd<float_4>(c) * fm1Param;
			// FM2, polyphonic or monophonic
			pitch += inputs[FM2_INPUT].getPolyVoltageSimd<float_4>(c) * fm2Param;
			oscillator->setPitch(pitch);

			// Pulse width
			float_4 pw = pwParam + inputs[PW_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f * pwmParam;
			oscillator->setPulseWidth(pw);

			oscillator->step(args.sampleTime);
			oscillator->setReset(inputs[RESET_INPUT].getPolyVoltageSimd<float_4>(c));

			// Outputs
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



struct LFOWidget : ModuleWidget {
	LFOWidget(LFO *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LFO-1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParam<CKSS>(Vec(15, 77), module, LFO::OFFSET_PARAM));
		addParam(createParam<CKSS>(Vec(119, 77), module, LFO::INVERT_PARAM));

		addParam(createParam<RoundHugeBlackKnob>(Vec(47, 61), module, LFO::FREQ_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(23, 143), module, LFO::FM1_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(91, 143), module, LFO::PW_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(23, 208), module, LFO::FM2_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(91, 208), module, LFO::PWM_PARAM));

		addInput(createInput<PJ301MPort>(Vec(11, 276), module, LFO::FM1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(45, 276), module, LFO::FM2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(80, 276), module, LFO::RESET_INPUT));
		addInput(createInput<PJ301MPort>(Vec(114, 276), module, LFO::PW_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(11, 320), module, LFO::SIN_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(45, 320), module, LFO::TRI_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(80, 320), module, LFO::SAW_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(114, 320), module, LFO::SQR_OUTPUT));

		addChild(createLight<SmallLight<RedGreenBlueLight>>(Vec(99, 42.5f), module, LFO::PHASE_LIGHT));
	}
};


Model *modelLFO = createModel<LFO, LFOWidget>("LFO");


struct LFO2 : Module {
	enum ParamIds {
		OFFSET_PARAM,
		INVERT_PARAM,
		FREQ_PARAM,
		WAVE_PARAM,
		FM_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FM_INPUT,
		RESET_INPUT,
		WAVE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		INTERP_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(PHASE_LIGHT, 3),
		NUM_LIGHTS
	};

	LowFrequencyOscillator<float_4> oscillators[4];
	dsp::ClockDivider lightDivider;

	LFO2() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(OFFSET_PARAM, 0.f, 1.f, 1.f, "Offset");
		configParam(INVERT_PARAM, 0.f, 1.f, 1.f, "Invert");
		configParam(FREQ_PARAM, -8.f, 10.f, 1.f, "Frequency", " Hz", 2, 1);
		configParam(WAVE_PARAM, 0.f, 3.f, 1.5f, "Wave");
		configParam(FM_PARAM, 0.f, 1.f, 0.5f, "Frequency modulation", "%", 0.f, 100.f);
		lightDivider.setDivision(16);
	}

	void process(const ProcessArgs &args) override {
		float freqParam = params[FREQ_PARAM].getValue();
		float fmParam = params[FM_PARAM].getValue();
		float waveParam = params[WAVE_PARAM].getValue();

		int channels = std::max(1, inputs[FM_INPUT].getChannels());

		for (int c = 0; c < channels; c += 4) {
			auto *oscillator = &oscillators[c / 4];
			oscillator->invert = (params[INVERT_PARAM].getValue() == 0.f);
			oscillator->bipolar = (params[OFFSET_PARAM].getValue() == 0.f);

			float_4 pitch = freqParam + inputs[FM_INPUT].getVoltageSimd<float_4>(c) * fmParam;
			oscillator->setPitch(pitch);

			oscillator->step(args.sampleTime);
			oscillator->setReset(inputs[RESET_INPUT].getPolyVoltageSimd<float_4>(c));

			// Outputs
			if (outputs[INTERP_OUTPUT].isConnected()) {
				float_4 wave = simd::clamp(waveParam + inputs[WAVE_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f * 3.f, 0.f, 3.f);
				float_4 v = 0.f;
				v += oscillator->sin() * simd::fmax(0.f, 1.f - simd::fabs(wave - 0.f));
				v += oscillator->tri() * simd::fmax(0.f, 1.f - simd::fabs(wave - 1.f));
				v += oscillator->saw() * simd::fmax(0.f, 1.f - simd::fabs(wave - 2.f));
				v += oscillator->sqr() * simd::fmax(0.f, 1.f - simd::fabs(wave - 3.f));
				outputs[INTERP_OUTPUT].setVoltageSimd(5.f * v, c);
			}
		}

		outputs[INTERP_OUTPUT].setChannels(channels);

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


struct LFO2Widget : ModuleWidget {
	LFO2Widget(LFO2 *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LFO-2.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParam<CKSS>(Vec(62, 150), module, LFO2::OFFSET_PARAM));
		addParam(createParam<CKSS>(Vec(62, 215), module, LFO2::INVERT_PARAM));

		addParam(createParam<RoundHugeBlackKnob>(Vec(18, 60), module, LFO2::FREQ_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(11, 142), module, LFO2::WAVE_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(11, 207), module, LFO2::FM_PARAM));

		addInput(createInput<PJ301MPort>(Vec(11, 276), module, LFO2::FM_INPUT));
		addInput(createInput<PJ301MPort>(Vec(54, 276), module, LFO2::RESET_INPUT));
		addInput(createInput<PJ301MPort>(Vec(11, 319), module, LFO2::WAVE_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(54, 319), module, LFO2::INTERP_OUTPUT));

		addChild(createLight<SmallLight<RedGreenBlueLight>>(Vec(68, 42.5f), module, LFO2::PHASE_LIGHT));
	}
};


Model *modelLFO2 = createModel<LFO2, LFO2Widget>("LFO2");
