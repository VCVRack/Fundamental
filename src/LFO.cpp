#include "Fundamental.hpp"


struct LowFrequencyOscillator {
	float phase = 0.f;
	float pw = 0.5f;
	float freq = 1.f;
	bool offset = false;
	bool invert = false;
	dsp::SchmittTrigger resetTrigger;

	LowFrequencyOscillator() {}
	void setPitch(float pitch) {
		pitch = fminf(pitch, 10.f);
		freq = powf(2.f, pitch);
	}
	void setPulseWidth(float pw_) {
		const float pwMin = 0.01f;
		pw = clamp(pw_, pwMin, 1.f - pwMin);
	}
	void setReset(float reset) {
		if (resetTrigger.process(reset / 0.01f)) {
			phase = 0.f;
		}
	}
	void step(float dt) {
		float deltaPhase = fminf(freq * dt, 0.5f);
		phase += deltaPhase;
		if (phase >= 1.f)
			phase -= 1.f;
	}
	float sin() {
		if (offset)
			return 1.f - std::cos(2*M_PI * phase) * (invert ? -1.f : 1.f);
		else
			return std::sin(2*M_PI * phase) * (invert ? -1.f : 1.f);
	}
	float tri(float x) {
		return 4.f * std::abs(x - std::round(x));
	}
	float tri() {
		if (offset)
			return tri(invert ? phase - 0.5f : phase);
		else
			return -1.f + tri(invert ? phase - 0.25f : phase - 0.75f);
	}
	float saw(float x) {
		return 2.f * (x - std::round(x));
	}
	float saw() {
		if (offset)
			return invert ? 2.f * (1.f - phase) : 2.f * phase;
		else
			return saw(phase) * (invert ? -1.f : 1.f);
	}
	float sqr() {
		float sqr = (phase < pw) ^ invert ? 1.f : -1.f;
		return offset ? sqr + 1.f : sqr;
	}
	float light() {
		return std::sin(2*M_PI * phase);
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
		PHASE_POS_LIGHT,
		PHASE_NEG_LIGHT,
		NUM_LIGHTS
	};

	LowFrequencyOscillator oscillator;

	LFO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		params[OFFSET_PARAM].config(0.f, 1.f, 1.f);
		params[INVERT_PARAM].config(0.f, 1.f, 1.f);
		params[FREQ_PARAM].config(-8.f, 10.f, 1.f);
		params[FM1_PARAM].config(0.f, 1.f, 0.f);
		params[PW_PARAM].config(0.f, 1.f, 0.5f);
		params[FM2_PARAM].config(0.f, 1.f, 0.f);
		params[PWM_PARAM].config(0.f, 1.f, 0.f);
	}

	void step() override {
		oscillator.setPitch(params[FREQ_PARAM].value + params[FM1_PARAM].value * inputs[FM1_INPUT].value + params[FM2_PARAM].value * inputs[FM2_INPUT].value);
		oscillator.setPulseWidth(params[PW_PARAM].value + params[PWM_PARAM].value * inputs[PW_INPUT].value / 10.f);
		oscillator.offset = (params[OFFSET_PARAM].value > 0.f);
		oscillator.invert = (params[INVERT_PARAM].value <= 0.f);
		oscillator.step(APP->engine->getSampleTime());
		oscillator.setReset(inputs[RESET_INPUT].value);

		outputs[SIN_OUTPUT].value = 5.f * oscillator.sin();
		outputs[TRI_OUTPUT].value = 5.f * oscillator.tri();
		outputs[SAW_OUTPUT].value = 5.f * oscillator.saw();
		outputs[SQR_OUTPUT].value = 5.f * oscillator.sqr();

		lights[PHASE_POS_LIGHT].setBrightnessSmooth(std::max(0.f, oscillator.light()));
		lights[PHASE_NEG_LIGHT].setBrightnessSmooth(std::max(0.f, -oscillator.light()));
	}

};



struct LFOWidget : ModuleWidget {
	LFOWidget(LFO *module) {
		setModule(module);
		setPanel(SVG::load(asset::plugin(pluginInstance, "res/LFO-1.svg")));

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

		addChild(createLight<SmallLight<GreenRedLight>>(Vec(99, 42.5f), module, LFO::PHASE_POS_LIGHT));
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
		PHASE_POS_LIGHT,
		PHASE_NEG_LIGHT,
		NUM_LIGHTS
	};

	LowFrequencyOscillator oscillator;

	LFO2() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		params[OFFSET_PARAM].config(0.f, 1.f, 1.f);
		params[INVERT_PARAM].config(0.f, 1.f, 1.f);
		params[FREQ_PARAM].config(-8.f, 10.f, 1.f);
		params[WAVE_PARAM].config(0.f, 3.f, 1.5f);
		params[FM_PARAM].config(0.f, 1.f, 0.5f);
	}

	void step() override {
		oscillator.setPitch(params[FREQ_PARAM].value + params[FM_PARAM].value * inputs[FM_INPUT].value);
		oscillator.offset = (params[OFFSET_PARAM].value > 0.f);
		oscillator.invert = (params[INVERT_PARAM].value <= 0.f);
		oscillator.step(APP->engine->getSampleTime());
		oscillator.setReset(inputs[RESET_INPUT].value);

		float wave = params[WAVE_PARAM].value + inputs[WAVE_INPUT].value;
		wave = clamp(wave, 0.f, 3.f);
		float interp;
		if (wave < 1.f)
			interp = crossfade(oscillator.sin(), oscillator.tri(), wave);
		else if (wave < 2.f)
			interp = crossfade(oscillator.tri(), oscillator.saw(), wave - 1.f);
		else
			interp = crossfade(oscillator.saw(), oscillator.sqr(), wave - 2.f);
		outputs[INTERP_OUTPUT].value = 5.f * interp;

		lights[PHASE_POS_LIGHT].setBrightnessSmooth(std::max(0.f, oscillator.light()));
		lights[PHASE_NEG_LIGHT].setBrightnessSmooth(std::max(0.f, -oscillator.light()));
	}
};


struct LFO2Widget : ModuleWidget {
	LFO2Widget(LFO2 *module) {
		setModule(module);
		setPanel(SVG::load(asset::plugin(pluginInstance, "res/LFO-2.svg")));

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

		addChild(createLight<SmallLight<GreenRedLight>>(Vec(68, 42.5f), module, LFO2::PHASE_POS_LIGHT));
	}
};


Model *modelLFO2 = createModel<LFO2, LFO2Widget>("LFO2");
