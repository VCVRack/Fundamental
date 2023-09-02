#include "plugin.hpp"


struct Logic : Module {
	enum ParamId {
		B_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		A_INPUT,
		B_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		NOTA_OUTPUT,
		NOTB_OUTPUT,
		OR_OUTPUT,
		NOR_OUTPUT,
		AND_OUTPUT,
		NAND_OUTPUT,
		XOR_OUTPUT,
		XNOR_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		B_BUTTON_LIGHT,
		ENUMS(NOTA_LIGHT, 2),
		ENUMS(NOTB_LIGHT, 2),
		ENUMS(OR_LIGHT, 2),
		ENUMS(NOR_LIGHT, 2),
		ENUMS(AND_LIGHT, 2),
		ENUMS(NAND_LIGHT, 2),
		ENUMS(XOR_LIGHT, 2),
		ENUMS(XNOR_LIGHT, 2),
		LIGHTS_LEN
	};

	dsp::ClockDivider lightDivider;

	Logic() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(B_PARAM, "B");
		configInput(A_INPUT, "A");
		configInput(B_INPUT, "B");
		configOutput(NOTA_OUTPUT, "NOT A");
		configOutput(NOTB_OUTPUT, "NOT B");
		configOutput(OR_OUTPUT, "OR");
		configOutput(NOR_OUTPUT, "NOR");
		configOutput(AND_OUTPUT, "AND");
		configOutput(NAND_OUTPUT, "NAND");
		configOutput(XOR_OUTPUT, "XOR");
		configOutput(XNOR_OUTPUT, "XNOR");

		lightDivider.setDivision(32);
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max({1, inputs[A_INPUT].getChannels(), inputs[B_INPUT].getChannels()});

		bool bPush = params[B_PARAM].getValue() > 0.f;
		bool anyState[8] = {};

		for (int c = 0; c < channels; c++) {
			bool a = inputs[A_INPUT].getPolyVoltage(c) >= 1.f;
			bool b = bPush || inputs[B_INPUT].getPolyVoltage(c) >= 1.f;

			bool states[8] = {
				!a, // NOTA
				!b, // NOTB
				a || b, // OR
				!(a || b), // NOR
				a && b, // AND
				!(a && b), // NAND
				a != b, // XOR
				a == b, // XNOR
			};

			for (int i = 0; i < 8; i++) {
				outputs[NOTA_OUTPUT + i].setVoltage(states[i] ? 10.f : 0.f, c);
				if (states[i])
					anyState[i] = true;
			}
		}

		for (int i = 0; i < 8; i++) {
			outputs[NOTA_OUTPUT + i].setChannels(channels);
		}

		// Set lights
		if (lightDivider.process()) {
			float lightTime = args.sampleTime * lightDivider.getDivision();
			lights[B_BUTTON_LIGHT].setBrightness(bPush);
			for (int i = 0; i < 8; i++) {
				lights[NOTA_LIGHT + 2 * i + 0].setBrightnessSmooth(anyState[i] && channels == 1, lightTime);
				lights[NOTA_LIGHT + 2 * i + 1].setBrightnessSmooth(anyState[i] && channels > 1, lightTime);
			}
		}
	}
};


struct LogicWidget : ModuleWidget {
	LogicWidget(Logic* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Logic.svg"), asset::plugin(pluginInstance, "res/Logic-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createLightParamCentered<LightButton<VCVBezelBig, VCVBezelLightBig<WhiteLight>>>(mm2px(Vec(12.7, 26.755)), module, Logic::B_PARAM, Logic::B_BUTTON_LIGHT));

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(7.299, 52.31)), module, Logic::A_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(18.136, 52.31)), module, Logic::B_INPUT));

		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(7.297, 67.53)), module, Logic::NOTA_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(18.134, 67.53)), module, Logic::NOTB_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(7.297, 82.732)), module, Logic::OR_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(18.134, 82.732)), module, Logic::NOR_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(7.297, 97.958)), module, Logic::AND_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(18.134, 97.958)), module, Logic::NAND_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(7.297, 113.115)), module, Logic::XOR_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(18.134, 113.115)), module, Logic::XNOR_OUTPUT));

		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(11.027, 63.805)), module, Logic::NOTA_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(21.864, 63.805)), module, Logic::NOTB_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(11.027, 79.007)), module, Logic::OR_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(21.864, 79.007)), module, Logic::NOR_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(11.027, 94.233)), module, Logic::AND_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(21.864, 94.233)), module, Logic::NAND_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(11.027, 109.393)), module, Logic::XOR_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(21.864, 109.393)), module, Logic::XNOR_LIGHT));
	}
};


Model* modelLogic = createModel<Logic, LogicWidget>("Logic");