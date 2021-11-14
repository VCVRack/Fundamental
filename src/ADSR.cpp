#include "plugin.hpp"


using simd::float_4;


struct ADSR : Module {
	enum ParamIds {
		ATTACK_PARAM,
		DECAY_PARAM,
		SUSTAIN_PARAM,
		RELEASE_PARAM,
		// added in 2.0
		ATTACK_CV_PARAM,
		DECAY_CV_PARAM,
		SUSTAIN_CV_PARAM,
		RELEASE_CV_PARAM,
		PUSH_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ATTACK_INPUT,
		DECAY_INPUT,
		SUSTAIN_INPUT,
		RELEASE_INPUT,
		GATE_INPUT,
		RETRIG_INPUT,
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
		PUSH_LIGHT,
		NUM_LIGHTS
	};

	static constexpr float MIN_TIME = 1e-3f;
	static constexpr float MAX_TIME = 10.f;
	static constexpr float LAMBDA_BASE = MAX_TIME / MIN_TIME;

	float_4 attacking[4] = {};
	float_4 env[4] = {};
	dsp::TSchmittTrigger<float_4> trigger[4];
	dsp::ClockDivider cvDivider;
	float_4 attackLambda[4] = {};
	float_4 decayLambda[4] = {};
	float_4 releaseLambda[4] = {};
	float_4 sustain[4] = {};
	dsp::ClockDivider lightDivider;

	ADSR() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ATTACK_PARAM, 0.f, 1.f, 0.5f, "Attack", " ms", LAMBDA_BASE, MIN_TIME * 1000);
		configParam(DECAY_PARAM, 0.f, 1.f, 0.5f, "Decay", " ms", LAMBDA_BASE, MIN_TIME * 1000);
		configParam(SUSTAIN_PARAM, 0.f, 1.f, 0.5f, "Sustain", "%", 0, 100);
		configParam(RELEASE_PARAM, 0.f, 1.f, 0.5f, "Release", " ms", LAMBDA_BASE, MIN_TIME * 1000);

		configParam(ATTACK_CV_PARAM, -1.f, 1.f, 0.f, "Attack CV", "%", 0, 100);
		configParam(DECAY_CV_PARAM, -1.f, 1.f, 0.f, "Decay CV", "%", 0, 100);
		configParam(SUSTAIN_CV_PARAM, -1.f, 1.f, 0.f, "Sustain CV", "%", 0, 100);
		configParam(RELEASE_CV_PARAM, -1.f, 1.f, 0.f, "Release CV", "%", 0, 100);

		configButton(PUSH_PARAM, "Push");

		configInput(ATTACK_INPUT, "Attack");
		configInput(DECAY_INPUT, "Decay");
		configInput(SUSTAIN_INPUT, "Sustain");
		configInput(RELEASE_INPUT, "Release");
		configInput(GATE_INPUT, "Gate");
		configInput(RETRIG_INPUT, "Retrigger");

		configOutput(ENVELOPE_OUTPUT, "Envelope");

		cvDivider.setDivision(16);
		lightDivider.setDivision(128);
	}

	void process(const ProcessArgs& args) override {
		// 0.16-0.19 us serial
		// 0.23 us serial with all lambdas computed
		// 0.15-0.18 us serial with all lambdas computed with SSE

		int channels = std::max(1, inputs[GATE_INPUT].getChannels());

		// Compute lambdas
		if (cvDivider.process()) {
			float attackParam = params[ATTACK_PARAM].getValue();
			float decayParam = params[DECAY_PARAM].getValue();
			float sustainParam = params[SUSTAIN_PARAM].getValue();
			float releaseParam = params[RELEASE_PARAM].getValue();

			float attackCvParam = params[ATTACK_CV_PARAM].getValue();
			float decayCvParam = params[DECAY_CV_PARAM].getValue();
			float sustainCvParam = params[SUSTAIN_CV_PARAM].getValue();
			float releaseCvParam = params[RELEASE_CV_PARAM].getValue();

			for (int c = 0; c < channels; c += 4) {
				// CV
				float_4 attack = attackParam + inputs[ATTACK_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f * attackCvParam;
				float_4 decay = decayParam + inputs[DECAY_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f * decayCvParam;
				float_4 sustain = sustainParam + inputs[SUSTAIN_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f * sustainCvParam;
				float_4 release = releaseParam + inputs[RELEASE_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f * releaseCvParam;

				attack = simd::clamp(attack, 0.f, 1.f);
				decay = simd::clamp(decay, 0.f, 1.f);
				sustain = simd::clamp(sustain, 0.f, 1.f);
				release = simd::clamp(release, 0.f, 1.f);

				attackLambda[c / 4] = simd::pow(LAMBDA_BASE, -attack) / MIN_TIME;
				decayLambda[c / 4] = simd::pow(LAMBDA_BASE, -decay) / MIN_TIME;
				releaseLambda[c / 4] = simd::pow(LAMBDA_BASE, -release) / MIN_TIME;
				this->sustain[c / 4] = sustain;
			}
		}

		float_4 gate[4] = {};
		bool push = (params[PUSH_PARAM].getValue() > 0.f);

		for (int c = 0; c < channels; c += 4) {
			// Gate
			gate[c / 4] = inputs[GATE_INPUT].getVoltageSimd<float_4>(c) >= 1.f;

			if (push) {
				gate[c / 4] = float_4::mask();
			}

			// Retrigger
			float_4 triggered = trigger[c / 4].process(inputs[RETRIG_INPUT].getPolyVoltageSimd<float_4>(c));
			attacking[c / 4] = simd::ifelse(triggered, float_4::mask(), attacking[c / 4]);

			// Get target and lambda for exponential decay
			const float attackTarget = 1.2f;
			float_4 target = simd::ifelse(gate[c / 4], simd::ifelse(attacking[c / 4], attackTarget, sustain[c / 4]), 0.f);
			float_4 lambda = simd::ifelse(gate[c / 4], simd::ifelse(attacking[c / 4], attackLambda[c / 4], decayLambda[c / 4]), releaseLambda[c / 4]);

			// Adjust env
			env[c / 4] += (target - env[c / 4]) * lambda * args.sampleTime;

			// Turn off attacking state if envelope is HIGH
			attacking[c / 4] = simd::ifelse(env[c / 4] >= 1.f, float_4::zero(), attacking[c / 4]);

			// Turn on attacking state if gate is LOW
			attacking[c / 4] = simd::ifelse(gate[c / 4], attacking[c / 4], float_4::mask());

			// Set output
			outputs[ENVELOPE_OUTPUT].setVoltageSimd(10.f * env[c / 4], c);
		}

		outputs[ENVELOPE_OUTPUT].setChannels(channels);

		// Lights
		if (lightDivider.process()) {
			lights[ATTACK_LIGHT].setBrightness(0);
			lights[DECAY_LIGHT].setBrightness(0);
			lights[SUSTAIN_LIGHT].setBrightness(0);
			lights[RELEASE_LIGHT].setBrightness(0);

			for (int c = 0; c < channels; c += 4) {
				const float epsilon = 0.01f;
				float_4 sustaining = (sustain[c / 4] <= env[c / 4]) & (env[c / 4] < sustain[c / 4] + epsilon);
				float_4 resting = (env[c / 4] < epsilon);

				if (simd::movemask(gate[c / 4] & attacking[c / 4]))
					lights[ATTACK_LIGHT].setBrightness(1);
				if (simd::movemask(gate[c / 4] & ~attacking[c / 4] & ~sustaining))
					lights[DECAY_LIGHT].setBrightness(1);
				if (simd::movemask(gate[c / 4] & ~attacking[c / 4] & sustaining))
					lights[SUSTAIN_LIGHT].setBrightness(1);
				if (simd::movemask(~gate[c / 4] & ~resting))
					lights[RELEASE_LIGHT].setBrightness(1);
			}

			// Push button light
			bool anyGate = false;
			for (int c = 0; c < channels; c += 4)
				anyGate = anyGate || simd::movemask(gate[c / 4]);
			lights[PUSH_LIGHT].setBrightness(anyGate);
		}
	}

	void paramsFromJson(json_t* rootJ) override {
		// These attenuators didn't exist in version <2.0, so set to 1 in case they are not overwritten.
		params[ATTACK_CV_PARAM].setValue(1.f);
		params[DECAY_CV_PARAM].setValue(1.f);
		params[SUSTAIN_CV_PARAM].setValue(1.f);
		params[RELEASE_CV_PARAM].setValue(1.f);

		Module::paramsFromJson(rootJ);
	}
};


struct ADSRDisplay : LedDisplay {
	ADSR* module;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			// Module parameters
			float attackLambda = module ? module->attackLambda[0][0] : 1.f;
			float decayLambda = module ? module->decayLambda[0][0] : 1.f;
			float releaseLambda = module ? module->releaseLambda[0][0] : 1.f;
			float sustain = module ? module->sustain[0][0] : 0.5f;

			// Scale lambdas
			const float power = 0.5f;
			float attack = std::pow(attackLambda, -power);
			float decay = std::pow(decayLambda, -power);
			float release = std::pow(releaseLambda, -power);
			float totalLambda = attack + decay + release;
			if (totalLambda == 0.f)
				return;

			Rect r = box.zeroPos().shrink(Vec(4, 5));
			Vec p0 = r.getBottomLeft();
			Vec p1 = r.interpolate(Vec(attack / totalLambda, 0));
			Vec p2 = r.interpolate(Vec((attack + decay) / totalLambda, 1 - sustain));
			Vec p3 = r.getBottomRight();
			Vec attackHandle = Vec(p0.x, crossfade(p0.y, p1.y, 0.8f));
			Vec decayHandle = Vec(p1.x, crossfade(p1.y, p2.y, 0.8f));
			Vec releaseHandle = Vec(p2.x, crossfade(p2.y, p3.y, 0.8f));

			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, p0.x, p0.y);
			nvgBezierTo(args.vg, p0.x, p0.y, attackHandle.x, attackHandle.y, p1.x, p1.y);
			nvgBezierTo(args.vg, p1.x, p1.y, decayHandle.x, decayHandle.y, p2.x, p2.y);
			nvgBezierTo(args.vg, p2.x, p2.y, releaseHandle.x, releaseHandle.y, p3.x, p3.y);
			nvgLineCap(args.vg, NVG_ROUND);
			nvgMiterLimit(args.vg, 2.f);
			nvgStrokeWidth(args.vg, 1.5f);
			nvgStrokeColor(args.vg, SCHEME_YELLOW);
			nvgStroke(args.vg);
		}
		LedDisplay::drawLayer(args, layer);
	}
};


struct ADSRWidget : ModuleWidget {
	ADSRWidget(ADSR* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/ADSR.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(6.604, 55.454)), module, ADSR::ATTACK_PARAM, ADSR::ATTACK_LIGHT));
		addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(17.441, 55.454)), module, ADSR::DECAY_PARAM, ADSR::DECAY_LIGHT));
		addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(28.279, 55.454)), module, ADSR::SUSTAIN_PARAM, ADSR::SUSTAIN_LIGHT));
		addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(39.116, 55.454)), module, ADSR::RELEASE_PARAM, ADSR::RELEASE_LIGHT));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.604, 80.603)), module, ADSR::ATTACK_CV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(17.441, 80.63)), module, ADSR::DECAY_CV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(28.279, 80.603)), module, ADSR::SUSTAIN_CV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(39.119, 80.603)), module, ADSR::RELEASE_CV_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(6.604, 113.115)), module, ADSR::PUSH_PARAM, ADSR::PUSH_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.604, 96.882)), module, ADSR::ATTACK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.441, 96.859)), module, ADSR::DECAY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(28.279, 96.886)), module, ADSR::SUSTAIN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(39.119, 96.89)), module, ADSR::RELEASE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.441, 113.115)), module, ADSR::GATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(28.279, 113.115)), module, ADSR::RETRIG_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.119, 113.115)), module, ADSR::ENVELOPE_OUTPUT));

		ADSRDisplay* display = createWidget<ADSRDisplay>(mm2px(Vec(0.0, 13.039)));
		display->box.size = mm2px(Vec(45.72, 21.219));
		display->module = module;
		addChild(display);
	}
};


Model* modelADSR = createModel<ADSR, ADSRWidget>("ADSR");
