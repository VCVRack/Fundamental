#include "Fundamental.hpp"
#include "dsp/digital.hpp"


struct ADSR : Module {
	enum ParamIds {
		ATTACK_PARAM,
		DECAY_PARAM,
		SUSTAIN_PARAM,
		RELEASE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ATTACK_INPUT,
		DECAY_INPUT,
		SUSTAIN_INPUT,
		RELEASE_INPUT,
		GATE_INPUT,
		TRIG_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENVELOPE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ATTACK_LIGHT,
		DECAY_LIGHT,
		SUSTAIN_LIGHT,
		RELEASE_LIGHT,
		NUM_LIGHTS
	};

	bool decaying = false;
	float env = 0.0;
	SchmittTrigger trigger;

	ADSR() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		trigger.setThresholds(0.0, 1.0);
	}
	void step() override;
};


void ADSR::step() {
	float attack = clampf(params[ATTACK_INPUT].value + inputs[ATTACK_INPUT].value / 10.0, 0.0, 1.0);
	float decay = clampf(params[DECAY_PARAM].value + inputs[DECAY_INPUT].value / 10.0, 0.0, 1.0);
	float sustain = clampf(params[SUSTAIN_PARAM].value + inputs[SUSTAIN_INPUT].value / 10.0, 0.0, 1.0);
	float release = clampf(params[RELEASE_PARAM].value + inputs[RELEASE_PARAM].value / 10.0, 0.0, 1.0);

	// Gate and trigger
	bool gated = inputs[GATE_INPUT].value >= 1.0;
	if (trigger.process(inputs[TRIG_INPUT].value))
		decaying = false;

	const float base = 20000.0;
	const float maxTime = 10.0;
	if (gated) {
		if (decaying) {
			// Decay
			if (decay < 1e-4) {
				env = sustain;
			}
			else {
				env += powf(base, 1 - decay) / maxTime * (sustain - env) / engineGetSampleRate();
			}
		}
		else {
			// Attack
			// Skip ahead if attack is all the way down (infinitely fast)
			if (attack < 1e-4) {
				env = 1.0;
			}
			else {
				env += powf(base, 1 - attack) / maxTime * (1.01 - env) / engineGetSampleRate();
			}
			if (env >= 1.0) {
				env = 1.0;
				decaying = true;
			}
		}
	}
	else {
		// Release
		if (release < 1e-4) {
			env = 0.0;
		}
		else {
			env += powf(base, 1 - release) / maxTime * (0.0 - env) / engineGetSampleRate();
		}
		decaying = false;
	}

	bool sustaining = nearf(env, sustain, 1e-3);
	bool resting = nearf(env, 0.0, 1e-3);

	outputs[ENVELOPE_OUTPUT].value = 10.0 * env;

	// Lights
	lights[ATTACK_LIGHT].value = (gated && !decaying) ? 1.0 : 0.0;
	lights[DECAY_LIGHT].value = (gated && decaying && !sustaining) ? 1.0 : 0.0;
	lights[SUSTAIN_LIGHT].value = (gated && decaying && sustaining) ? 1.0 : 0.0;
	lights[RELEASE_LIGHT].value = (!gated && !resting) ? 1.0 : 0.0;
}


ADSRWidget::ADSRWidget() {
	ADSR *module = new ADSR();
	setModule(module);
	box.size = Vec(15*8, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/ADSR.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(createParam<RoundBlackKnob>(Vec(62, 57), module, ADSR::ATTACK_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<RoundBlackKnob>(Vec(62, 124), module, ADSR::DECAY_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<RoundBlackKnob>(Vec(62, 191), module, ADSR::SUSTAIN_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<RoundBlackKnob>(Vec(62, 257), module, ADSR::RELEASE_PARAM, 0.0, 1.0, 0.5));

	addInput(createInput<PJ301MPort>(Vec(9, 63), module, ADSR::ATTACK_INPUT));
	addInput(createInput<PJ301MPort>(Vec(9, 129), module, ADSR::DECAY_INPUT));
	addInput(createInput<PJ301MPort>(Vec(9, 196), module, ADSR::SUSTAIN_INPUT));
	addInput(createInput<PJ301MPort>(Vec(9, 263), module, ADSR::RELEASE_INPUT));

	addInput(createInput<PJ301MPort>(Vec(9, 320), module, ADSR::GATE_INPUT));
	addInput(createInput<PJ301MPort>(Vec(48, 320), module, ADSR::TRIG_INPUT));
	addOutput(createOutput<PJ301MPort>(Vec(87, 320), module, ADSR::ENVELOPE_OUTPUT));

	addChild(createLight<SmallLight<RedLight>>(Vec(94, 41), module, ADSR::ATTACK_LIGHT));
	addChild(createLight<SmallLight<RedLight>>(Vec(94, 109), module, ADSR::DECAY_LIGHT));
	addChild(createLight<SmallLight<RedLight>>(Vec(94, 175), module, ADSR::SUSTAIN_LIGHT));
	addChild(createLight<SmallLight<RedLight>>(Vec(94, 242), module, ADSR::RELEASE_LIGHT));
}
