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
		ENUMS(VU_LIGHTS, 6),
		NUM_LIGHTS
	};

	dsp::VuMeter2 vuMeter;
	dsp::ClockDivider vuDivider;
	dsp::ClockDivider lightDivider;
	int lastChannels = 0;

	Sum() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Level", "%", 0.f, 100.f);
		configInput(POLY_INPUT, "Polyphonic");
		configOutput(MONO_OUTPUT, "Monophonic");

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
			lastChannels = inputs[POLY_INPUT].getChannels();

			lights[VU_LIGHTS + 0].setBrightness(vuMeter.getBrightness(0.f, 0.f));
			for (int i = 1; i < 6; i++) {
				lights[VU_LIGHTS + i].setBrightness(vuMeter.getBrightness(-3.f * i, -3.f * (i - 1)));
			}
		}
	}
};


struct SumDisplay : LedDisplay {
	Sum* module;
};


struct SumChannelDisplay : ChannelDisplay {
	Sum* module;
	void step() override {
		int channels = 16;
		if (module)
			channels = module->lastChannels;
		text = string::f("%d", channels);
	}
};


struct SumWidget : ModuleWidget {
	SumWidget(Sum* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Sum.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 64.284)), module, Sum::LEVEL_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 96.798)), module, Sum::POLY_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 113.066)), module, Sum::MONO_OUTPUT));

		SumDisplay* display = createWidget<SumDisplay>(mm2px(Vec(0.0, 12.834)));
		display->box.size = mm2px(Vec(15.241, 36.981));
		display->module = module;
		addChild(display);

		SumChannelDisplay* channelDisplay = createWidget<SumChannelDisplay>(mm2px(Vec(3.521, 77.191)));
		channelDisplay->box.size = mm2px(Vec(8.197, 8.197));
		channelDisplay->module = module;
		addChild(channelDisplay);
	}
};


Model* modelSum = createModel<Sum, SumWidget>("Sum");
