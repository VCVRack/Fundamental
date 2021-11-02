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


struct VizDisplay : Widget {
	Viz* module;

	VizDisplay() {
		box.size = mm2px(Vec(15.24, 88.126));
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			for (int c = 0; c < 16; c++) {
				Vec p = Vec(15, 16 + (float) c / 16 * (box.size.y - 10));
				std::string text = string::f("%d", c + 1);
				std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/nunito/Nunito-Bold.ttf"));

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

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(2.58, 7.229)), module, Viz::POLY_INPUT));

		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 11.388)), module, Viz::VU_LIGHTS + 0 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 13.18)), module, Viz::VU_LIGHTS + 1 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 14.971)), module, Viz::VU_LIGHTS + 2 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 16.763)), module, Viz::VU_LIGHTS + 3 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 18.554)), module, Viz::VU_LIGHTS + 4 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 20.345)), module, Viz::VU_LIGHTS + 5 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 22.137)), module, Viz::VU_LIGHTS + 6 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 23.928)), module, Viz::VU_LIGHTS + 7 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 25.719)), module, Viz::VU_LIGHTS + 8 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 27.511)), module, Viz::VU_LIGHTS + 9 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 29.302)), module, Viz::VU_LIGHTS + 10 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 31.094)), module, Viz::VU_LIGHTS + 11 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 32.885)), module, Viz::VU_LIGHTS + 12 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 34.677)), module, Viz::VU_LIGHTS + 13 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 36.468)), module, Viz::VU_LIGHTS + 14 * 2));
		addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(mm2px(Vec(3.676, 38.259)), module, Viz::VU_LIGHTS + 15 * 2));

		VizDisplay* display = createWidget<VizDisplay>(mm2px(Vec(0.0, 9.901)));
		display->box.size = mm2px(Vec(5.161, 29.845));
		display->module = module;
		addChild(display);
	}
};


Model* modelViz = createModel<Viz, VizWidget>("Viz");
