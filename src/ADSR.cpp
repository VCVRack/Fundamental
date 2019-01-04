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
	float env = 0.0f;
	SchmittTrigger trigger;

	ADSR() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;
};


void ADSR::step() {
	float attack = clamp(params[ATTACK_PARAM].value + inputs[ATTACK_INPUT].value / 10.0f, 0.0f, 1.0f);
	float decay = clamp(params[DECAY_PARAM].value + inputs[DECAY_INPUT].value / 10.0f, 0.0f, 1.0f);
	float sustain = clamp(params[SUSTAIN_PARAM].value + inputs[SUSTAIN_INPUT].value / 10.0f, 0.0f, 1.0f);
	float release = clamp(params[RELEASE_PARAM].value + inputs[RELEASE_INPUT].value / 10.0f, 0.0f, 1.0f);

	// Gate and trigger
	bool gated = inputs[GATE_INPUT].value >= 1.0f;
	if (trigger.process(inputs[TRIG_INPUT].value))
		decaying = false;

	const float base = 20000.0f;
	const float maxTime = 10.0f;
	if (gated) {
		if (decaying) {
			// Decay
			if (decay < 1e-4) {
				env = sustain;
			}
			else {
				env += powf(base, 1 - decay) / maxTime * (sustain - env) * engineGetSampleTime();
			}
		}
		else {
			// Attack
			// Skip ahead if attack is all the way down (infinitely fast)
			if (attack < 1e-4) {
				env = 1.0f;
			}
			else {
				env += powf(base, 1 - attack) / maxTime * (1.01f - env) * engineGetSampleTime();
			}
			if (env >= 1.0f) {
				env = 1.0f;
				decaying = true;
			}
		}
	}
	else {
		// Release
		if (release < 1e-4) {
			env = 0.0f;
		}
		else {
			env += powf(base, 1 - release) / maxTime * (0.0f - env) * engineGetSampleTime();
		}
		decaying = false;
	}

	bool sustaining = isNear(env, sustain, 1e-3);
	bool resting = isNear(env, 0.0f, 1e-3);

	outputs[ENVELOPE_OUTPUT].value = 10.0f * env;

	// Lights
	lights[ATTACK_LIGHT].value = (gated && !decaying) ? 1.0f : 0.0f;
	lights[DECAY_LIGHT].value = (gated && decaying && !sustaining) ? 1.0f : 0.0f;
	lights[SUSTAIN_LIGHT].value = (gated && decaying && sustaining) ? 1.0f : 0.0f;
	lights[RELEASE_LIGHT].value = (!gated && !resting) ? 1.0f : 0.0f;
}


struct ADSRWidget : ModuleWidget {
	ADSRWidget(ADSR *module);
};

ADSRWidget::ADSRWidget(ADSR *module) : ModuleWidget(module) {
	setPanel(SVG::load(assetPlugin(plugin, "res/ADSR.svg")));

	addChild(createWidget<ScrewSilver>(Vec(15, 0)));
	addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createWidget<ScrewSilver>(Vec(15, 365)));
	addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(createParam<RoundLargeBlackKnob>(Vec(62, 57), module, ADSR::ATTACK_PARAM, 0.0f, 1.0f, 0.5f));
	addParam(createParam<RoundLargeBlackKnob>(Vec(62, 124), module, ADSR::DECAY_PARAM, 0.0f, 1.0f, 0.5f));
	addParam(createParam<RoundLargeBlackKnob>(Vec(62, 191), module, ADSR::SUSTAIN_PARAM, 0.0f, 1.0f, 0.5f));
	addParam(createParam<RoundLargeBlackKnob>(Vec(62, 257), module, ADSR::RELEASE_PARAM, 0.0f, 1.0f, 0.5f));

	addInput(createPort<PJ301MPort>(Vec(9, 63), PortWidget::INPUT, module, ADSR::ATTACK_INPUT));
	addInput(createPort<PJ301MPort>(Vec(9, 129), PortWidget::INPUT, module, ADSR::DECAY_INPUT));
	addInput(createPort<PJ301MPort>(Vec(9, 196), PortWidget::INPUT, module, ADSR::SUSTAIN_INPUT));
	addInput(createPort<PJ301MPort>(Vec(9, 263), PortWidget::INPUT, module, ADSR::RELEASE_INPUT));

	addInput(createPort<PJ301MPort>(Vec(9, 320), PortWidget::INPUT, module, ADSR::GATE_INPUT));
	addInput(createPort<PJ301MPort>(Vec(48, 320), PortWidget::INPUT, module, ADSR::TRIG_INPUT));
	addOutput(createPort<PJ301MPort>(Vec(87, 320), PortWidget::OUTPUT, module, ADSR::ENVELOPE_OUTPUT));

	addChild(createLight<SmallLight<RedLight>>(Vec(94, 41), module, ADSR::ATTACK_LIGHT));
	addChild(createLight<SmallLight<RedLight>>(Vec(94, 109), module, ADSR::DECAY_LIGHT));
	addChild(createLight<SmallLight<RedLight>>(Vec(94, 175), module, ADSR::SUSTAIN_LIGHT));
	addChild(createLight<SmallLight<RedLight>>(Vec(94, 242), module, ADSR::RELEASE_LIGHT));
}


Model *modelADSR = createModel<ADSR, ADSRWidget>("ADSR");
