#include "plugin.hpp"


struct Pulses : Module {
	enum ParamIds {
		ENUMS(TAP_PARAMS, 10),
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
		ENUMS(TAP_LIGHTS, 10),
		NUM_LIGHTS
	};

	dsp::BooleanTrigger tapTriggers[10];
	dsp::PulseGenerator pulseGenerators[10];

	Pulses() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i = 0; i < 10; i++)
			configParam(TAP_PARAMS + i, 0.f, 1.f, 0.f, string::f("Tap %d", i + 1));
	}

	void process(const ProcessArgs& args) override {
		for (int i = 0; i < 10; i++) {
			bool tap = params[TAP_PARAMS + i].getValue() > 0.f;
			if (tapTriggers[i].process(tap)) {
				pulseGenerators[i].trigger(1e-3f);
			}
			bool pulse = pulseGenerators[i].process(args.sampleTime);
			outputs[TRIG_OUTPUTS + i].setVoltage(pulse ? 10.f : 0.f);
			outputs[GATE_OUTPUTS + i].setVoltage(tap ? 10.f : 0.f);
			lights[TAP_LIGHTS + i].setBrightness(tap);
		}
	}
};


struct PulsesWidget : ModuleWidget {
	PulsesWidget(Pulses* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Pulses.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<LEDBezel>(mm2px(Vec(8.32, 21.505)), module, Pulses::TAP_PARAMS + 0));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(8.32, 31.504)), module, Pulses::TAP_PARAMS + 1));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(8.32, 41.505)), module, Pulses::TAP_PARAMS + 2));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(8.32, 51.505)), module, Pulses::TAP_PARAMS + 3));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(8.32, 61.505)), module, Pulses::TAP_PARAMS + 4));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(8.32, 71.505)), module, Pulses::TAP_PARAMS + 5));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(8.32, 81.505)), module, Pulses::TAP_PARAMS + 6));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(8.32, 91.504)), module, Pulses::TAP_PARAMS + 7));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(8.32, 101.505)), module, Pulses::TAP_PARAMS + 8));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(8.32, 111.505)), module, Pulses::TAP_PARAMS + 9));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.319, 21.504)), module, Pulses::TRIG_OUTPUTS + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(32.319, 21.504)), module, Pulses::GATE_OUTPUTS + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.319, 31.504)), module, Pulses::TRIG_OUTPUTS + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(32.319, 31.504)), module, Pulses::GATE_OUTPUTS + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.319, 41.505)), module, Pulses::TRIG_OUTPUTS + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(32.319, 41.505)), module, Pulses::GATE_OUTPUTS + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.319, 51.504)), module, Pulses::TRIG_OUTPUTS + 3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(32.319, 51.504)), module, Pulses::GATE_OUTPUTS + 3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.319, 61.504)), module, Pulses::TRIG_OUTPUTS + 4));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(32.319, 61.504)), module, Pulses::GATE_OUTPUTS + 4));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.319, 71.504)), module, Pulses::TRIG_OUTPUTS + 5));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(32.319, 71.504)), module, Pulses::GATE_OUTPUTS + 5));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.319, 81.505)), module, Pulses::TRIG_OUTPUTS + 6));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(32.319, 81.505)), module, Pulses::GATE_OUTPUTS + 6));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.319, 91.504)), module, Pulses::TRIG_OUTPUTS + 7));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(32.319, 91.504)), module, Pulses::GATE_OUTPUTS + 7));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.319, 101.504)), module, Pulses::TRIG_OUTPUTS + 8));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(32.319, 101.504)), module, Pulses::GATE_OUTPUTS + 8));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.319, 111.504)), module, Pulses::TRIG_OUTPUTS + 9));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(32.319, 111.504)), module, Pulses::GATE_OUTPUTS + 9));

		addChild(createLightCentered<LEDBezelLight<GreenLight>>(mm2px(Vec(8.32, 21.505)), module, Pulses::TAP_LIGHTS + 0));
		addChild(createLightCentered<LEDBezelLight<GreenLight>>(mm2px(Vec(8.32, 31.504)), module, Pulses::TAP_LIGHTS + 1));
		addChild(createLightCentered<LEDBezelLight<GreenLight>>(mm2px(Vec(8.32, 41.505)), module, Pulses::TAP_LIGHTS + 2));
		addChild(createLightCentered<LEDBezelLight<GreenLight>>(mm2px(Vec(8.32, 51.505)), module, Pulses::TAP_LIGHTS + 3));
		addChild(createLightCentered<LEDBezelLight<GreenLight>>(mm2px(Vec(8.32, 61.505)), module, Pulses::TAP_LIGHTS + 4));
		addChild(createLightCentered<LEDBezelLight<GreenLight>>(mm2px(Vec(8.32, 71.505)), module, Pulses::TAP_LIGHTS + 5));
		addChild(createLightCentered<LEDBezelLight<GreenLight>>(mm2px(Vec(8.32, 81.505)), module, Pulses::TAP_LIGHTS + 6));
		addChild(createLightCentered<LEDBezelLight<GreenLight>>(mm2px(Vec(8.32, 91.504)), module, Pulses::TAP_LIGHTS + 7));
		addChild(createLightCentered<LEDBezelLight<GreenLight>>(mm2px(Vec(8.32, 101.505)), module, Pulses::TAP_LIGHTS + 8));
		addChild(createLightCentered<LEDBezelLight<GreenLight>>(mm2px(Vec(8.32, 111.505)), module, Pulses::TAP_LIGHTS + 9));
	}
};


Model* modelPulses = createModel<Pulses, PulsesWidget>("Pulses");