#include "plugin.hpp"


struct Pulses : Module {
	enum ParamIds {
		ENUMS(PUSH_PARAMS, 10),
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(TRIG_OUTPUTS, 10),
		ENUMS(GATE_OUTPUTS, 10),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(PUSH_LIGHTS, 10),
		NUM_LIGHTS
	};

	dsp::BooleanTrigger tapTriggers[10];
	dsp::PulseGenerator pulseGenerators[10];

	Pulses() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i = 0; i < 10; i++) {
			configButton(PUSH_PARAMS + i, string::f("Row %d push", i + 1));
			configOutput(TRIG_OUTPUTS + i, string::f("Row %d trigger", i + 1));
			configOutput(GATE_OUTPUTS + i, string::f("Row %d gate", i + 1));
		}
	}

	void process(const ProcessArgs& args) override {
		for (int i = 0; i < 10; i++) {
			bool tap = params[PUSH_PARAMS + i].getValue() > 0.f;
			if (tapTriggers[i].process(tap)) {
				pulseGenerators[i].trigger(1e-3f);
			}
			bool pulse = pulseGenerators[i].process(args.sampleTime);
			outputs[TRIG_OUTPUTS + i].setVoltage(pulse ? 10.f : 0.f);
			outputs[GATE_OUTPUTS + i].setVoltage(tap ? 10.f : 0.f);
			lights[PUSH_LIGHTS + i].setBrightness(tap);
		}
	}
};


struct PulsesWidget : ModuleWidget {
	PulsesWidget(Pulses* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Pulses.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(7.28, 21.968)), module, Pulses::PUSH_PARAMS + 0, Pulses::PUSH_LIGHTS + 0));
		addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(7.28, 32.095)), module, Pulses::PUSH_PARAMS + 1, Pulses::PUSH_LIGHTS + 1));
		addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(7.28, 42.222)), module, Pulses::PUSH_PARAMS + 2, Pulses::PUSH_LIGHTS + 2));
		addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(7.28, 52.35)), module, Pulses::PUSH_PARAMS + 3, Pulses::PUSH_LIGHTS + 3));
		addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(7.28, 62.477)), module, Pulses::PUSH_PARAMS + 4, Pulses::PUSH_LIGHTS + 4));
		addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(7.28, 72.605)), module, Pulses::PUSH_PARAMS + 5, Pulses::PUSH_LIGHTS + 5));
		addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(7.28, 82.732)), module, Pulses::PUSH_PARAMS + 6, Pulses::PUSH_LIGHTS + 6));
		addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(7.28, 92.86)), module, Pulses::PUSH_PARAMS + 7, Pulses::PUSH_LIGHTS + 7));
		addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(7.28, 102.987)), module, Pulses::PUSH_PARAMS + 8, Pulses::PUSH_LIGHTS + 8));
		addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(7.28, 113.115)), module, Pulses::PUSH_PARAMS + 9, Pulses::PUSH_LIGHTS + 9));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.313, 21.968)), module, Pulses::TRIG_OUTPUTS + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.313, 32.095)), module, Pulses::TRIG_OUTPUTS + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.313, 42.222)), module, Pulses::TRIG_OUTPUTS + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.313, 52.35)), module, Pulses::TRIG_OUTPUTS + 3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.313, 62.477)), module, Pulses::TRIG_OUTPUTS + 4));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.313, 72.605)), module, Pulses::TRIG_OUTPUTS + 5));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.313, 82.732)), module, Pulses::TRIG_OUTPUTS + 6));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.313, 92.86)), module, Pulses::TRIG_OUTPUTS + 7));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.313, 102.987)), module, Pulses::TRIG_OUTPUTS + 8));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.313, 113.115)), module, Pulses::TRIG_OUTPUTS + 9));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.321, 21.968)), module, Pulses::GATE_OUTPUTS + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.321, 32.095)), module, Pulses::GATE_OUTPUTS + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.321, 42.222)), module, Pulses::GATE_OUTPUTS + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.321, 52.35)), module, Pulses::GATE_OUTPUTS + 3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.321, 62.477)), module, Pulses::GATE_OUTPUTS + 4));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.321, 72.605)), module, Pulses::GATE_OUTPUTS + 5));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.321, 82.732)), module, Pulses::GATE_OUTPUTS + 6));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.321, 92.86)), module, Pulses::GATE_OUTPUTS + 7));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.321, 102.987)), module, Pulses::GATE_OUTPUTS + 8));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.321, 113.115)), module, Pulses::GATE_OUTPUTS + 9));

	}
};


Model* modelPulses = createModel<Pulses, PulsesWidget>("Pulses");