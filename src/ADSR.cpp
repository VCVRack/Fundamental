#include "plugin.hpp"


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
	float env = 0.f;
	dsp::SchmittTrigger trigger;

	ADSR() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		params[ATTACK_PARAM].config(0.f, 1.f, 0.5f, "Attack");
		params[DECAY_PARAM].config(0.f, 1.f, 0.5f, "Decay");
		params[SUSTAIN_PARAM].config(0.f, 1.f, 0.5f, "Sustain");
		params[RELEASE_PARAM].config(0.f, 1.f, 0.5f, "Release");
	}

	void step() override {
		float attack = clamp(params[ATTACK_PARAM].value + inputs[ATTACK_INPUT].value / 10.f, 0.f, 1.f);
		float decay = clamp(params[DECAY_PARAM].value + inputs[DECAY_INPUT].value / 10.f, 0.f, 1.f);
		float sustain = clamp(params[SUSTAIN_PARAM].value + inputs[SUSTAIN_INPUT].value / 10.f, 0.f, 1.f);
		float release = clamp(params[RELEASE_PARAM].value + inputs[RELEASE_INPUT].value / 10.f, 0.f, 1.f);

		// Gate and trigger
		bool gated = inputs[GATE_INPUT].value >= 1.f;
		if (trigger.process(inputs[TRIG_INPUT].value))
			decaying = false;

		const float base = 20000.f;
		const float maxTime = 10.f;
		if (gated) {
			if (decaying) {
				// Decay
				if (decay < 1e-4) {
					env = sustain;
				}
				else {
					env += std::pow(base, 1 - decay) / maxTime * (sustain - env) * APP->engine->getSampleTime();
				}
			}
			else {
				// Attack
				// Skip ahead if attack is all the way down (infinitely fast)
				if (attack < 1e-4) {
					env = 1.f;
				}
				else {
					env += std::pow(base, 1 - attack) / maxTime * (1.01f - env) * APP->engine->getSampleTime();
				}
				if (env >= 1.f) {
					env = 1.f;
					decaying = true;
				}
			}
		}
		else {
			// Release
			if (release < 1e-4) {
				env = 0.f;
			}
			else {
				env += std::pow(base, 1 - release) / maxTime * (0.f - env) * APP->engine->getSampleTime();
			}
			decaying = false;
		}

		bool sustaining = isNear(env, sustain, 1e-3);
		bool resting = isNear(env, 0.f, 1e-3);

		outputs[ENVELOPE_OUTPUT].value = 10.f * env;

		// Lights
		lights[ATTACK_LIGHT].value = (gated && !decaying) ? 1.f : 0.f;
		lights[DECAY_LIGHT].value = (gated && decaying && !sustaining) ? 1.f : 0.f;
		lights[SUSTAIN_LIGHT].value = (gated && decaying && sustaining) ? 1.f : 0.f;
		lights[RELEASE_LIGHT].value = (!gated && !resting) ? 1.f : 0.f;
	}
};


struct ADSRWidget : ModuleWidget {
	ADSRWidget(ADSR *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ADSR.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParam<RoundLargeBlackKnob>(Vec(62, 57), module, ADSR::ATTACK_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(62, 124), module, ADSR::DECAY_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(62, 191), module, ADSR::SUSTAIN_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(62, 257), module, ADSR::RELEASE_PARAM));

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
};


Model *modelADSR = createModel<ADSR, ADSRWidget>("ADSR");
