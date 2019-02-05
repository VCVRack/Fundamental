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
		ENUMS(VU_LIGHTS, 16*2),
		NUM_LIGHTS
	};

	int channels = 0;
	dsp::VUMeter2 vuMeter[16];
	int frame = 0;

	Viz() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		for (int c = 0; c < 16; c++) {
			vuMeter[c].lambda = 1 / 0.1f;
		}
	}

	void step() override {
		if (frame % 16 == 0) {
			channels = inputs[POLY_INPUT].getChannels();
			float deltaTime = APP->engine->getSampleTime() * 16;

			// Process VU meters
			for (int c = 0; c < channels; c++) {
				float value = inputs[POLY_INPUT].getVoltage(c) / 10.f;
				vuMeter[c].process(deltaTime, value);
			}
			for (int c = channels; c < 16; c++) {
				vuMeter[c].reset();
			}
		}

		if (frame % 256 == 0) {
			// Set lights
			for (int c = 0; c < 16; c++) {
				float green = vuMeter[c].getBrightness(-24.f, -6.f);
				float red = vuMeter[c].getBrightness(-6.f, 0.f);
				lights[VU_LIGHTS + c*2 + 0].setBrightness(green - red);
				lights[VU_LIGHTS + c*2 + 1].setBrightness(red);
			}
		}

		frame++;
	}
};


struct VizDisplay : Widget {
	Viz *module;
	std::shared_ptr<Font> font;

	VizDisplay() {
		box.size = mm2px(Vec(15.24, 88.126));
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/nunito/Nunito-Bold.ttf"));
	}

	void draw(const widget::DrawContext &ctx) override {
		for (int c = 0; c < 16; c++) {
			Vec p = Vec(15, 16 + (float) c / 16 * (box.size.y - 10));
			std::string text = string::f("%d", c + 1);

			nvgFontFaceId(ctx.vg, font->handle);
			nvgFontSize(ctx.vg, 11);
			nvgTextLetterSpacing(ctx.vg, 0.0);
			nvgTextAlign(ctx.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
			if (module && c < module->channels)
				nvgFillColor(ctx.vg, nvgRGB(255, 255, 255));
			else
				nvgFillColor(ctx.vg, nvgRGB(99, 99, 99));
			nvgText(ctx.vg, p.x, p.y, text.c_str(), NULL);
		}
	}
};


struct VizWidget : ModuleWidget {
	VizWidget(Viz *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Viz.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.619, 21.346)), module, Viz::POLY_INPUT));

		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 33.626)), module, Viz::VU_LIGHTS + 0*2));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 38.916)), module, Viz::VU_LIGHTS + 1*2));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 44.205)), module, Viz::VU_LIGHTS + 2*2));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 49.496)), module, Viz::VU_LIGHTS + 3*2));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 54.785)), module, Viz::VU_LIGHTS + 4*2));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 60.075)), module, Viz::VU_LIGHTS + 5*2));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 65.364)), module, Viz::VU_LIGHTS + 6*2));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 70.654)), module, Viz::VU_LIGHTS + 7*2));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 75.943)), module, Viz::VU_LIGHTS + 8*2));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 81.233)), module, Viz::VU_LIGHTS + 9*2));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 86.522)), module, Viz::VU_LIGHTS + 10*2));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 91.812)), module, Viz::VU_LIGHTS + 11*2));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 97.101)), module, Viz::VU_LIGHTS + 12*2));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 102.392)), module, Viz::VU_LIGHTS + 13*2));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 107.681)), module, Viz::VU_LIGHTS + 14*2));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.854, 112.971)), module, Viz::VU_LIGHTS + 15*2));

		VizDisplay *vizDisplay = createWidget<VizDisplay>(mm2px(Vec(0.0, 29.235)));
		vizDisplay->module = module;
		addChild(vizDisplay);
	}
};


Model *modelViz = createModel<Viz, VizWidget>("Viz");
