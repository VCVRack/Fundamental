#include "plugin.hpp"


struct Random : Module {
	enum ParamIds {
		RATE_PARAM,
		SHAPE_PARAM,
		OFFSET_PARAM,
		MODE_PARAM,
		// new in 2.0
		PROB_PARAM, // TODO
		RAND_PARAM, // TODO
		RATE_CV_PARAM,
		SHAPE_CV_PARAM,
		PROB_CV_PARAM,
		RAND_CV_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		RATE_INPUT,
		SHAPE_INPUT,
		TRIGGER_INPUT,
		EXTERNAL_INPUT,
		// new in 2.0
		PROB_INPUT,
		RAND_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		STEPPED_OUTPUT,
		LINEAR_OUTPUT,
		SMOOTH_OUTPUT,
		EXPONENTIAL_OUTPUT,
		// new in 2.0
		GATE_OUTPUT, // TODO
		NUM_OUTPUTS
	};
	enum LightIds {
		RATE_LIGHT,
		SHAPE_LIGHT,
		PROB_LIGHT,
		RAND_LIGHT,
		OFFSET_LIGHT,
		NUM_LIGHTS
	};

	dsp::SchmittTrigger trigTrigger;
	dsp::BooleanTrigger offsetTrigger;
	float lastValue = 0.f;
	float value = 0.f;
	float clockPhase = 0.f;
	int trigFrame = 0;
	int lastTrigFrames = INT_MAX;

	Random() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RATE_PARAM, std::log2(0.002f), std::log2(2000.f), std::log2(2.f), "Rate", " Hz", 2);
		configParam(SHAPE_PARAM, 0.f, 1.f, 1.f, "Shape", "%", 0, 100);
		configParam(PROB_PARAM, 0.f, 1.f, 1.f, "Probability", "%", 0, 100);
		configParam(RAND_PARAM, 0.f, 1.f, 1.f, "Randomness", "%", 0, 100);

		configParam(RATE_CV_PARAM, -1.f, 1.f, 0.f, "Rate CV", "%", 0, 100);
		getParamQuantity(RATE_CV_PARAM)->randomizeEnabled = false;
		configParam(SHAPE_CV_PARAM, -1.f, 1.f, 0.f, "Shape CV", "%", 0, 100);
		getParamQuantity(SHAPE_CV_PARAM)->randomizeEnabled = false;
		configParam(PROB_CV_PARAM, -1.f, 1.f, 0.f, "Probability CV", "%", 0, 100);
		getParamQuantity(PROB_CV_PARAM)->randomizeEnabled = false;
		configParam(RAND_CV_PARAM, -1.f, 1.f, 0.f, "Randomness CV", "%", 0, 100);
		getParamQuantity(RAND_CV_PARAM)->randomizeEnabled = false;

		configSwitch(OFFSET_PARAM, 0.f, 1.f, 0.f, "Offset", {"Bipolar", "Unipolar"});

		configInput(RATE_INPUT, "Rate");
		configInput(SHAPE_INPUT, "Shape");
		configInput(PROB_INPUT, "Probability");
		configInput(RAND_INPUT, "Randomness");
		configInput(TRIGGER_INPUT, "Trigger");
		configInput(EXTERNAL_INPUT, "External");

		configOutput(STEPPED_OUTPUT, "Stepped");
		configOutput(LINEAR_OUTPUT, "Linear");
		configOutput(SMOOTH_OUTPUT, "Smooth");
		configOutput(EXPONENTIAL_OUTPUT, "Exponential");
		configOutput(GATE_OUTPUT, "Gate");
	}

	void trigger() {
		lastValue = value;
		if (inputs[EXTERNAL_INPUT].isConnected()) {
			value = inputs[EXTERNAL_INPUT].getVoltage() / 10.f;
		}
		else {
			// Choose a new random value
			bool absolute = params[MODE_PARAM].getValue() > 0.f;
			bool uni = params[OFFSET_PARAM].getValue() > 0.f;
			if (absolute) {
				value = random::uniform();
				if (!uni)
					value -= 0.5f;
			}
			else {
				// Switch to uni if bi
				if (!uni)
					value += 0.5f;
				float deltaValue = random::normal();
				// Bias based on value
				deltaValue -= (value - 0.5f) * 2.f;
				// Scale delta and accumulate value
				const float stdDev = 1 / 10.f;
				deltaValue *= stdDev;
				value += deltaValue;
				value = clamp(value, 0.f, 1.f);
				// Switch back to bi
				if (!uni)
					value -= 0.5f;
			}
		}
		lights[RATE_LIGHT].setBrightness(3.f);
	}

	void process(const ProcessArgs& args) override {
		if (inputs[TRIGGER_INPUT].isConnected()) {
			// Advance clock phase based on tempo estimate
			trigFrame++;
			float deltaPhase = 1.f / lastTrigFrames;
			clockPhase += deltaPhase;
			clockPhase = std::min(clockPhase, 1.f);
			// Trigger
			if (trigTrigger.process(rescale(inputs[TRIGGER_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f))) {
				clockPhase = 0.f;
				lastTrigFrames = trigFrame;
				trigFrame = 0;
				trigger();
			}
		}
		else {
			// Advance clock phase by rate
			float rate = params[RATE_PARAM].getValue();
			rate += inputs[RATE_PARAM].getVoltage() * params[RATE_CV_PARAM].getValue();
			float clockFreq = std::pow(2.f, rate);
			float deltaPhase = std::fmin(clockFreq * args.sampleTime, 0.5f);
			clockPhase += deltaPhase;
			// Trigger
			if (clockPhase >= 1.f) {
				clockPhase -= 1.f;
				trigger();
			}
		}

		// Params
		float shape = params[SHAPE_PARAM].getValue();
		shape += inputs[SHAPE_INPUT].getVoltage() / 10.f * params[SHAPE_CV_PARAM].getValue();
		shape = clamp(shape, 0.f, 1.f);

		float prob = params[PROB_PARAM].getValue();
		prob += inputs[PROB_INPUT].getVoltage() / 10.f * params[PROB_CV_PARAM].getValue();
		prob = clamp(prob, 0.f, 1.f);

		float rand = params[RAND_PARAM].getValue();
		rand += inputs[RAND_INPUT].getVoltage() / 10.f * params[RAND_CV_PARAM].getValue();
		rand = clamp(rand, 0.f, 1.f);

		// Stepped
		if (outputs[STEPPED_OUTPUT].isConnected()) {
			float steps = std::ceil(std::pow(shape, 2) * 15 + 1);
			float v = std::ceil(clockPhase * steps) / steps;
			v = rescale(v, 0.f, 1.f, lastValue, value);
			outputs[STEPPED_OUTPUT].setVoltage(v * 10.f);
		}

		// Linear
		if (outputs[LINEAR_OUTPUT].isConnected()) {
			float slope = 1 / shape;
			float v;
			if (slope < 1e6f) {
				v = std::fmin(clockPhase * slope, 1.f);
			}
			else {
				v = 1.f;
			}
			v = rescale(v, 0.f, 1.f, lastValue, value);
			outputs[LINEAR_OUTPUT].setVoltage(v * 10.f);
		}

		// Smooth
		if (outputs[SMOOTH_OUTPUT].isConnected()) {
			float p = 1 / shape;
			float v;
			if (p < 1e6f) {
				v = std::fmin(clockPhase * p, 1.f);
				v = std::cos(M_PI * v);
			}
			else {
				v = -1.f;
			}
			v = rescale(v, 1.f, -1.f, lastValue, value);
			outputs[SMOOTH_OUTPUT].setVoltage(v * 10.f);
		}

		// Exp
		if (outputs[EXPONENTIAL_OUTPUT].isConnected()) {
			float b = std::pow(shape, 4);
			float v;
			if (0.999f < b) {
				v = clockPhase;
			}
			else if (1e-20f < b) {
				v = (std::pow(b, clockPhase) - 1.f) / (b - 1.f);
			}
			else {
				v = 1.f;
			}
			v = rescale(v, 0.f, 1.f, lastValue, value);
			outputs[EXPONENTIAL_OUTPUT].setVoltage(v * 10.f);
		}

		// Lights
		lights[RATE_LIGHT].setSmoothBrightness(0.f, args.sampleTime);
		lights[SHAPE_LIGHT].setBrightness(shape);
		lights[PROB_LIGHT].setBrightness(prob);
		lights[RAND_LIGHT].setBrightness(rand);
		// lights[OFFSET_LIGHT].setBrightness(uni);
	}

	void paramsFromJson(json_t* rootJ) override {
		// In <2.0, there were no attenuverters, so set them to 1.0 in case they are not overwritten.
		params[RATE_CV_PARAM].setValue(1.f);
		params[SHAPE_CV_PARAM].setValue(1.f);
		params[PROB_CV_PARAM].setValue(1.f);
		params[RAND_CV_PARAM].setValue(1.f);
		Module::paramsFromJson(rootJ);
	}
};


struct RandomWidget : ModuleWidget {
	RandomWidget(Random* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Random.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(6.479, 33.605)), module, Random::RATE_PARAM, Random::RATE_LIGHT));
		addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(17.315, 33.605)), module, Random::PROB_PARAM, Random::PROB_LIGHT));
		addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(28.152, 33.605)), module, Random::RAND_PARAM, Random::RAND_LIGHT));
		addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(38.98, 33.605)), module, Random::SHAPE_PARAM, Random::SHAPE_LIGHT));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.479, 64.347)), module, Random::RATE_CV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(17.317, 64.347)), module, Random::PROB_CV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(28.154, 64.347)), module, Random::RAND_CV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(38.991, 64.347)), module, Random::SHAPE_CV_PARAM));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(28.154, 96.859)), module, Random::OFFSET_PARAM, Random::OFFSET_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.479, 80.549)), module, Random::RATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.317, 80.549)), module, Random::PROB_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(28.154, 80.553)), module, Random::RAND_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(38.991, 80.557)), module, Random::SHAPE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.479, 96.859)), module, Random::TRIGGER_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.317, 96.859)), module, Random::EXTERNAL_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.991, 96.859)), module, Random::GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.479, 113.115)), module, Random::STEPPED_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(17.317, 113.115)), module, Random::LINEAR_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(28.154, 113.115)), module, Random::EXPONENTIAL_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.991, 113.115)), module, Random::SMOOTH_OUTPUT));
	}
};


Model* modelRandom = createModel<Random, RandomWidget>("Random");