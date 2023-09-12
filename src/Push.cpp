#include "plugin.hpp"


struct Push : Module {
	enum ParamId {
		PUSH_PARAM,
		HOLD_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		HOLD_INPUT,
		PUSH_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		TRIG_OUTPUT,
		GATE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		PUSH_LIGHT,
		HOLD_LIGHT,
		LIGHTS_LEN
	};

	dsp::BooleanTrigger holdBoolean;
	dsp::SchmittTrigger holdSchmitt;
	dsp::BooleanTrigger pushBoolean;
	dsp::SchmittTrigger pushSchmitt;
	dsp::PulseGenerator trigPulse;
	bool hold = false;

	Push() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(PUSH_PARAM, "Push");
		configButton(HOLD_PARAM, "Hold");
		configInput(HOLD_INPUT, "Hold");
		configInput(PUSH_INPUT, "Push");
		configOutput(TRIG_OUTPUT, "Trigger");
		configOutput(GATE_OUTPUT, "Gate");
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		hold = false;
	}

	void process(const ProcessArgs& args) override {
		// Hold button
		if (holdBoolean.process(params[HOLD_PARAM].getValue()))
			hold ^= true;

		// Hold input
		if (holdSchmitt.process(inputs[HOLD_INPUT].getVoltage(), 0.1f, 1.f))
			hold ^= true;

		// Push button
		bool push = params[PUSH_PARAM].getValue() > 0.f;

		// Push input
		pushSchmitt.process(inputs[PUSH_INPUT].getVoltage(), 0.1f, 1.f);

		// Gate and trigger outputs
		bool gate = push || pushSchmitt.isHigh();
		gate ^= hold;
		if (pushBoolean.process(gate)) {
			trigPulse.trigger(1e-3f);
		}

		outputs[TRIG_OUTPUT].setVoltage(trigPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[GATE_OUTPUT].setVoltage(gate ? 10.f : 0.f);

		lights[HOLD_LIGHT].setBrightnessSmooth(hold, args.sampleTime);
		lights[PUSH_LIGHT].setBrightnessSmooth(gate, args.sampleTime);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "hold", json_boolean(hold));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* holdJ = json_object_get(rootJ, "hold");
		if (holdJ)
			hold = json_boolean_value(holdJ);
	}
};


struct PushWidget : ModuleWidget {
	PushWidget(Push* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Push.svg"), asset::plugin(pluginInstance, "res/Push-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createLightParamCentered<LightButton<VCVBezelBig, VCVBezelLightBig<WhiteLight>>>(mm2px(Vec(7.62, 24.723)), module, Push::PUSH_PARAM, Push::PUSH_LIGHT));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(7.617, 48.074)), module, Push::HOLD_PARAM, Push::HOLD_LIGHT));

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(7.612, 64.344)), module, Push::HOLD_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(7.612, 80.597)), module, Push::PUSH_INPUT));

		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(7.62, 96.864)), module, Push::TRIG_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(7.62, 113.115)), module, Push::GATE_OUTPUT));
	}
};


Model* modelPush = createModel<Push, PushWidget>("Push");