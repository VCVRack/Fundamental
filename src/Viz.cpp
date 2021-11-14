#include "plugin.hpp"


struct Viz : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		POLY_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(VU_LIGHTS, 16 * 2),
		NUM_LIGHTS
	};

	int lastChannel = 0;
	dsp::ClockDivider lightDivider;

	Viz() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configInput(POLY_INPUT, "Polyphonic");

		lightDivider.setDivision(16);
	}

	void process(const ProcessArgs& args) override {
		if (lightDivider.process()) {
			lastChannel = inputs[POLY_INPUT].getChannels();
			float deltaTime = args.sampleTime * lightDivider.getDivision();

			for (int c = 0; c < 16; c++) {
				float v = inputs[POLY_INPUT].getVoltage(c) / 10.f;
				lights[VU_LIGHTS + c * 2 + 0].setSmoothBrightness(v, deltaTime);
				lights[VU_LIGHTS + c * 2 + 1].setSmoothBrightness(-v, deltaTime);
			}
		}
	}
};


struct VizDisplay : LedDisplay {
	Viz* module;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			static const std::vector<float> posY = {
				mm2px(18.068 - 13.039),
				mm2px(23.366 - 13.039),
				mm2px(28.663 - 13.039),
				mm2px(33.961 - 13.039),
				mm2px(39.258 - 13.039),
				mm2px(44.556 - 13.039),
				mm2px(49.919 - 13.039),
				mm2px(55.217 - 13.039),
				mm2px(60.514 - 13.039),
				mm2px(65.812 - 13.039),
				mm2px(71.109 - 13.039),
				mm2px(76.473 - 13.039),
				mm2px(81.771 - 13.039),
				mm2px(87.068 - 13.039),
				mm2px(92.366 - 13.039),
				mm2px(97.663 - 13.039),
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

			for (int c = 0; c < 16; c++) {
				if (!module || c < module->lastChannel)
					nvgFillColor(args.vg, nvgRGB(255, 255, 255));
				else
					nvgFillColor(args.vg, nvgRGB(99, 99, 99));
				std::string text = string::f("%d", c + 1);
				nvgText(args.vg, 15.0, posY[c], text.c_str(), NULL);
			}
			nvgRestore(args.vg);
		}
		LedDisplay::drawLayer(args, layer);
	}
};


struct VizWidget : ModuleWidget {
	VizWidget(Viz* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Viz.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 113.115)), module, Viz::POLY_INPUT));

		VizDisplay* display = createWidget<VizDisplay>(mm2px(Vec(0.0, 13.039)));
		display->box.size = mm2px(Vec(15.237, 89.344));
		display->module = module;
		addChild(display);

		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 18.068)), module, Viz::VU_LIGHTS + 2 * 0));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 23.366)), module, Viz::VU_LIGHTS + 2 * 1));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 28.663)), module, Viz::VU_LIGHTS + 2 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 33.961)), module, Viz::VU_LIGHTS + 2 * 3));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 39.258)), module, Viz::VU_LIGHTS + 2 * 4));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 44.556)), module, Viz::VU_LIGHTS + 2 * 5));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 49.919)), module, Viz::VU_LIGHTS + 2 * 6));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 55.217)), module, Viz::VU_LIGHTS + 2 * 7));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 60.514)), module, Viz::VU_LIGHTS + 2 * 8));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 65.812)), module, Viz::VU_LIGHTS + 2 * 9));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 71.109)), module, Viz::VU_LIGHTS + 2 * 10));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 76.473)), module, Viz::VU_LIGHTS + 2 * 11));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 81.771)), module, Viz::VU_LIGHTS + 2 * 12));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 87.068)), module, Viz::VU_LIGHTS + 2 * 13));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 92.366)), module, Viz::VU_LIGHTS + 2 * 14));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(10.846, 97.663)), module, Viz::VU_LIGHTS + 2 * 15));
	}
};


Model* modelViz = createModel<Viz, VizWidget>("Viz");
