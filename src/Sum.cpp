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
		lightDivider.setDivision(512);
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

			lights[VU_LIGHTS + 0].setBrightness(vuMeter.getBrightness(0, 0));
			lights[VU_LIGHTS + 1].setBrightness(vuMeter.getBrightness(-3, 0));
			lights[VU_LIGHTS + 2].setBrightness(vuMeter.getBrightness(-6, -3));
			lights[VU_LIGHTS + 3].setBrightness(vuMeter.getBrightness(-12, -6));
			lights[VU_LIGHTS + 4].setBrightness(vuMeter.getBrightness(-24, -12));
			lights[VU_LIGHTS + 5].setBrightness(vuMeter.getBrightness(-36, -24));
		}
	}
};


struct SumDisplay : LedDisplay {
	Sum* module;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			static const std::vector<float> posY = {
				mm2px(18.068 - 13.039),
				mm2px(23.366 - 13.039),
				mm2px(28.663 - 13.039),
				mm2px(33.961 - 13.039),
				mm2px(39.258 - 13.039),
				mm2px(44.556 - 13.039),
			};
			static const std::vector<std::string> texts = {
				" 0", "-3", "-6", "-12", "-24", "-36",
			};

			std::string fontPath = asset::system("res/fonts/Nunito-Bold.ttf");
			std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
			if (!font)
				return;

			nvgSave(args.vg);
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 11);
			nvgTextLetterSpacing(args.vg, 0.0);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, nvgRGB(99, 99, 99));

			for (int i = 0; i < 6; i++) {
				nvgText(args.vg, 15.0, posY[i], texts[i].c_str(), NULL);
			}
			nvgRestore(args.vg);
		}
		LedDisplay::drawLayer(args, layer);
	}
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

		SumDisplay* display = createWidget<SumDisplay>(mm2px(Vec(0.0, 13.039)));
		display->box.size = mm2px(Vec(15.241, 36.981));
		display->module = module;
		addChild(display);

		addChild(createLightCentered<SmallSimpleLight<RedLight>>(mm2px(Vec(10.808, 18.081)), module, Sum::VU_LIGHTS + 0));
		addChild(createLightCentered<SmallSimpleLight<YellowLight>>(mm2px(Vec(10.808, 23.378)), module, Sum::VU_LIGHTS + 1));
		addChild(createLightCentered<SmallSimpleLight<GreenLight>>(mm2px(Vec(10.808, 28.676)), module, Sum::VU_LIGHTS + 2));
		addChild(createLightCentered<SmallSimpleLight<GreenLight>>(mm2px(Vec(10.808, 33.973)), module, Sum::VU_LIGHTS + 3));
		addChild(createLightCentered<SmallSimpleLight<GreenLight>>(mm2px(Vec(10.808, 39.271)), module, Sum::VU_LIGHTS + 4));
		addChild(createLightCentered<SmallSimpleLight<GreenLight>>(mm2px(Vec(10.808, 44.568)), module, Sum::VU_LIGHTS + 5));

		SumChannelDisplay* channelDisplay = createWidget<SumChannelDisplay>(mm2px(Vec(3.521, 77.191)));
		channelDisplay->box.size = mm2px(Vec(8.197, 8.197));
		channelDisplay->module = module;
		addChild(channelDisplay);
	}
};


Model* modelSum = createModel<Sum, SumWidget>("Sum");
