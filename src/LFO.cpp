#include "Fundamental.hpp"
#include "dsp/digital.hpp"


struct LowFrequencyOscillator {
	float phase = 0.0f;
	float pw = 0.5f;
	float freq = 1.0f;
	bool offset = false;
	bool invert = false;
	SchmittTrigger resetTrigger;
	LowFrequencyOscillator() {
		resetTrigger.setThresholds(0.0f, 0.01f);
	}
	void setPitch(float pitch) {
		pitch = fminf(pitch, 8.0f);
		freq = powf(2.0f, pitch);
	}
	void setPulseWidth(float pw_) {
		const float pwMin = 0.01f;
		pw = clamp(pw_, pwMin, 1.0f - pwMin);
	}
	void setReset(float reset) {
		if (resetTrigger.process(reset)) {
			phase = 0.0f;
		}
	}
	void step(float dt) {
		float deltaPhase = fminf(freq * dt, 0.5f);
		phase += deltaPhase;
		if (phase >= 1.0f)
			phase -= 1.0f;
	}
	float sin() {
		if (offset)
			return 1.0f - cosf(2*M_PI * phase) * (invert ? -1.0f : 1.0f);
		else
			return sinf(2*M_PI * phase) * (invert ? -1.0f : 1.0f);
	}
	float tri(float x) {
		return 4.0f * fabsf(x - roundf(x));
	}
	float tri() {
		if (offset)
			return tri(invert ? phase - 0.5f : phase);
		else
			return -1.0f + tri(invert ? phase - 0.25f : phase - 0.75f);
	}
	float saw(float x) {
		return 2.0f * (x - roundf(x));
	}
	float saw() {
		if (offset)
			return invert ? 2.0f * (1.0f - phase) : 2.0f * phase;
		else
			return saw(phase) * (invert ? -1.0f : 1.0f);
	}
	float sqr() {
		float sqr = (phase < pw) ^ invert ? 1.0f : -1.0f;
		return offset ? sqr + 1.0f : sqr;
	}
	float light() {
		return sinf(2*M_PI * phase);
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

	LFO() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;
};


void LFO::step() {
	oscillator.setPitch(params[FREQ_PARAM].value + params[FM1_PARAM].value * inputs[FM1_INPUT].value + params[FM2_PARAM].value * inputs[FM2_INPUT].value);
	oscillator.setPulseWidth(params[PW_PARAM].value + params[PWM_PARAM].value * inputs[PW_INPUT].value / 10.0f);
	oscillator.offset = (params[OFFSET_PARAM].value > 0.0f);
	oscillator.invert = (params[INVERT_PARAM].value <= 0.0f);
	oscillator.step(engineGetSampleTime());
	oscillator.setReset(inputs[RESET_INPUT].value);

	outputs[SIN_OUTPUT].value = 5.0f * oscillator.sin();
	outputs[TRI_OUTPUT].value = 5.0f * oscillator.tri();
	outputs[SAW_OUTPUT].value = 5.0f * oscillator.saw();
	outputs[SQR_OUTPUT].value = 5.0f * oscillator.sqr();

	lights[PHASE_POS_LIGHT].setBrightnessSmooth(fmaxf(0.0f, oscillator.light()));
	lights[PHASE_NEG_LIGHT].setBrightnessSmooth(fmaxf(0.0f, -oscillator.light()));
}


LFOWidget::LFOWidget() {
	LFO *module = new LFO();
	setModule(module);
	box.size = Vec(15*10, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/LFO-1.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(createParam<CKSS>(Vec(15, 77), module, LFO::OFFSET_PARAM, 0.0f, 1.0f, 1.0f));
	addParam(createParam<CKSS>(Vec(119, 77), module, LFO::INVERT_PARAM, 0.0f, 1.0f, 1.0f));

	addParam(createParam<RoundHugeBlackKnob>(Vec(47, 61), module, LFO::FREQ_PARAM, -8.0f, 6.0f, -1.0f));
	addParam(createParam<RoundBlackKnob>(Vec(23, 143), module, LFO::FM1_PARAM, 0.0f, 1.0f, 0.0f));
	addParam(createParam<RoundBlackKnob>(Vec(91, 143), module, LFO::PW_PARAM, 0.0f, 1.0f, 0.5f));
	addParam(createParam<RoundBlackKnob>(Vec(23, 208), module, LFO::FM2_PARAM, 0.0f, 1.0f, 0.0f));
	addParam(createParam<RoundBlackKnob>(Vec(91, 208), module, LFO::PWM_PARAM, 0.0f, 1.0f, 0.0f));

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

	LFO2() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;
};


void LFO2::step() {
	oscillator.setPitch(params[FREQ_PARAM].value + params[FM_PARAM].value * inputs[FM_INPUT].value);
	oscillator.offset = (params[OFFSET_PARAM].value > 0.0f);
	oscillator.invert = (params[INVERT_PARAM].value <= 0.0f);
	oscillator.step(engineGetSampleTime());
	oscillator.setReset(inputs[RESET_INPUT].value);

	float wave = params[WAVE_PARAM].value + inputs[WAVE_INPUT].value;
	wave = clamp(wave, 0.0f, 3.0f);
	float interp;
	if (wave < 1.0f)
		interp = crossfade(oscillator.sin(), oscillator.tri(), wave);
	else if (wave < 2.0f)
		interp = crossfade(oscillator.tri(), oscillator.saw(), wave - 1.0f);
	else
		interp = crossfade(oscillator.saw(), oscillator.sqr(), wave - 2.0f);
	outputs[INTERP_OUTPUT].value = 5.0f * interp;

	lights[PHASE_POS_LIGHT].setBrightnessSmooth(fmaxf(0.0f, oscillator.light()));
	lights[PHASE_NEG_LIGHT].setBrightnessSmooth(fmaxf(0.0f, -oscillator.light()));
}


LFO2Widget::LFO2Widget() {
	LFO2 *module = new LFO2();
	setModule(module);
	box.size = Vec(15*6, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/LFO-2.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(createParam<CKSS>(Vec(62, 150), module, LFO2::OFFSET_PARAM, 0.0f, 1.0f, 1.0f));
	addParam(createParam<CKSS>(Vec(62, 215), module, LFO2::INVERT_PARAM, 0.0f, 1.0f, 1.0f));

	addParam(createParam<RoundHugeBlackKnob>(Vec(18, 60), module, LFO2::FREQ_PARAM, -8.0f, 6.0f, -1.0f));
	addParam(createParam<RoundBlackKnob>(Vec(11, 142), module, LFO2::WAVE_PARAM, 0.0f, 3.0f, 1.5f));
	addParam(createParam<RoundBlackKnob>(Vec(11, 207), module, LFO2::FM_PARAM, 0.0f, 1.0f, 0.5f));

	addInput(createInput<PJ301MPort>(Vec(11, 276), module, LFO2::FM_INPUT));
	addInput(createInput<PJ301MPort>(Vec(54, 276), module, LFO2::RESET_INPUT));
	addInput(createInput<PJ301MPort>(Vec(11, 319), module, LFO2::WAVE_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(54, 319), module, LFO2::INTERP_OUTPUT));

	addChild(createLight<SmallLight<GreenRedLight>>(Vec(68, 42.5f), module, LFO2::PHASE_POS_LIGHT));
}
