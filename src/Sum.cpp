#include "plugin.hpp"


struct Sum : Module {
	enum ParamIds {
		LEVEL_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		POLY_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MONO_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(CHANNEL_LIGHTS, 16),
		ENUMS(VU_LIGHTS, 6),
		NUM_LIGHTS
	};

	dsp::VuMeter2 vuMeter;
	dsp::ClockDivider vuDivider;
	dsp::ClockDivider lightDivider;

	Sum() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Level", "%", 0.f, 100.f);

		vuMeter.lambda = 1 / 0.1f;
		vuDivider.setDivision(16);
		lightDivider.setDivision(256);
	}

	void process(const ProcessArgs& args) override {
		float sum = inputs[POLY_INPUT].getVoltageSum();
		sum *= params[LEVEL_PARAM].getValue();
		outputs[MONO_OUTPUT].setVoltage(sum);

		if (vuDivider.process()) {
			vuMeter.process(args.sampleTime * vuDivider.getDivision(), sum / 10.f);
		}

		// Set channel lights infrequently
		if (lightDivider.process()) {
			for (int c = 0; c < 16; c++) {
				bool active = (c < inputs[POLY_INPUT].getChannels());
				lights[CHANNEL_LIGHTS + c].setBrightness(active);
			}

			lights[VU_LIGHTS + 0].setBrightness(vuMeter.getBrightness(0.f, 0.f));
			for (int i = 1; i < 6; i++) {
				lights[VU_LIGHTS + i].setBrightness(vuMeter.getBrightness(-3.f * i, -3.f * (i - 1)));
			}
		}
	}
};


struct SumWidget : ModuleWidget {
	SumWidget(Sum* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Sum.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 53.519)), module, Sum::LEVEL_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.619, 21.347)), module, Sum::POLY_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 112.021)), module, Sum::MONO_OUTPUT));

		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(4.619, 33.595)), module, Sum::CHANNEL_LIGHTS + 0));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(6.619, 33.595)), module, Sum::CHANNEL_LIGHTS + 1));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(8.619, 33.595)), module, Sum::CHANNEL_LIGHTS + 2));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(10.619, 33.595)), module, Sum::CHANNEL_LIGHTS + 3));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(4.619, 35.595)), module, Sum::CHANNEL_LIGHTS + 4));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(6.619, 35.595)), module, Sum::CHANNEL_LIGHTS + 5));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(8.619, 35.595)), module, Sum::CHANNEL_LIGHTS + 6));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(10.619, 35.595)), module, Sum::CHANNEL_LIGHTS + 7));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(4.619, 37.595)), module, Sum::CHANNEL_LIGHTS + 8));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(6.619, 37.595)), module, Sum::CHANNEL_LIGHTS + 9));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(8.619, 37.595)), module, Sum::CHANNEL_LIGHTS + 10));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(10.619, 37.595)), module, Sum::CHANNEL_LIGHTS + 11));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(4.619, 39.595)), module, Sum::CHANNEL_LIGHTS + 12));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(6.619, 39.595)), module, Sum::CHANNEL_LIGHTS + 13));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(8.619, 39.595)), module, Sum::CHANNEL_LIGHTS + 14));
		addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(10.619, 39.595)), module, Sum::CHANNEL_LIGHTS + 15));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(7.619, 70.792)), module, Sum::VU_LIGHTS + 0));
		addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(7.619, 75.917)), module, Sum::VU_LIGHTS + 1));
		addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(7.619, 81.042)), module, Sum::VU_LIGHTS + 2));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(7.619, 86.167)), module, Sum::VU_LIGHTS + 3));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(7.619, 91.292)), module, Sum::VU_LIGHTS + 4));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(7.619, 96.417)), module, Sum::VU_LIGHTS + 5));
	}
};


Model* modelSum = createModel<Sum, SumWidget>("Sum");
