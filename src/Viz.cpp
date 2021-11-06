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

	VizDisplay() {
		box.size = mm2px(Vec(15.24, 88.126));
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			for (int c = 0; c < 16; c++) {
				Vec p = Vec(15, 16 + (float) c / 16 * (box.size.y - 10));
				std::string text = string::f("%d", c + 1);
				std::string fontPath = asset::system("res/fonts/Nunito-Bold.ttf");
				std::shared_ptr<Font> font = APP->window->loadFont(fontPath);

				if (font) {
					nvgFontFaceId(args.vg, font->handle);
					nvgFontSize(args.vg, 11);
					nvgTextLetterSpacing(args.vg, 0.0);
					nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
					if (module && c < module->lastChannel)
						nvgFillColor(args.vg, nvgRGB(255, 255, 255));
					else
						nvgFillColor(args.vg, nvgRGB(99, 99, 99));
					nvgText(args.vg, p.x, p.y, text.c_str(), NULL);
				}
			}
		}
		Widget::drawLayer(args, layer);
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

		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 18.068)), module, Viz::VU_LIGHTS + 0));
		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 23.366)), module, Viz::VU_LIGHTS + 1));
		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 28.663)), module, Viz::VU_LIGHTS + 2));
		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 33.961)), module, Viz::VU_LIGHTS + 3));
		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 39.258)), module, Viz::VU_LIGHTS + 4));
		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 44.556)), module, Viz::VU_LIGHTS + 5));
		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 49.919)), module, Viz::VU_LIGHTS + 6));
		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 55.217)), module, Viz::VU_LIGHTS + 7));
		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 60.514)), module, Viz::VU_LIGHTS + 8));
		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 65.812)), module, Viz::VU_LIGHTS + 9));
		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 71.109)), module, Viz::VU_LIGHTS + 10));
		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 76.473)), module, Viz::VU_LIGHTS + 11));
		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 81.771)), module, Viz::VU_LIGHTS + 12));
		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 87.068)), module, Viz::VU_LIGHTS + 13));
		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 92.366)), module, Viz::VU_LIGHTS + 14));
		addChild(createLightCentered<RedLight>(mm2px(Vec(10.846, 97.663)), module, Viz::VU_LIGHTS + 15));

		VizDisplay* display = createWidget<VizDisplay>(mm2px(Vec(0.003, 13.039)));
		display->box.size = mm2px(Vec(15.237, 89.344));
		display->module = module;
		addChild(display);
	}
};


Model* modelViz = createModel<Viz, VizWidget>("Viz");
