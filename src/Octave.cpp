#include "plugin.hpp"


struct Octave : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	int octave = 0;

	Octave() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	}

	void onReset() override {
		octave = 0;
	}

	void onRandomize() override {
		octave = (random::u32() % 9) - 4;
	}

	void process(const ProcessArgs &args) override {
		int channels = std::max(inputs[CV_INPUT].getChannels(), 1);
		for (int c = 0; c < channels; c++) {
			float cv = inputs[CV_INPUT].getVoltage(c);
			cv += octave;
			outputs[CV_OUTPUT].setVoltage(cv, c);
		}
		outputs[CV_OUTPUT].setChannels(channels);
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "octave", json_integer(octave));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *octaveJ = json_object_get(rootJ, "octave");
		if (octaveJ)
			octave = json_integer_value(octaveJ);
	}
};


struct OctaveButton : OpaqueWidget {
	Octave *module;
	int octave;

	void draw(const DrawArgs &args) override {
		Vec c = box.size.div(2);

		if ((module && module->octave == octave) || octave == 0) {
			// Enabled
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, c.x, c.y, mm2px(4.0/2));
			if (octave < 0)
				nvgFillColor(args.vg, color::RED);
			else if (octave > 0)
				nvgFillColor(args.vg, color::GREEN);
			else
				nvgFillColor(args.vg, color::alpha(color::WHITE, 0.33));
			nvgFill(args.vg);
		}
		else {
			// Disabled
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, c.x, c.y, mm2px(4.0/2));
			nvgFillColor(args.vg, color::alpha(color::WHITE, 0.33));
			nvgFill(args.vg);

			nvgBeginPath(args.vg);
			nvgCircle(args.vg, c.x, c.y, mm2px(3.0/2));
			nvgFillColor(args.vg, color::BLACK);
			nvgFill(args.vg);

			if (octave == 0) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, c.x, c.y, mm2px(1.0/2));
				nvgFillColor(args.vg, color::alpha(color::WHITE, 0.33));
				nvgFill(args.vg);
			}
		}
	}

	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			module->octave = 0;
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}

	void onDragEnter(const event::DragEnter &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			OctaveButton *w = dynamic_cast<OctaveButton*>(e.origin);
			if (w) {
				module->octave = octave;
			}
		}
	}
};


struct OctaveDisplay : OpaqueWidget {
	OctaveDisplay() {
		box.size = mm2px(Vec(15.240, 72.000));
	}

	void setModule(Octave *module) {
		clearChildren();

		const int octaves = 9;
		const float margin = mm2px(2.0);
		float height = box.size.y - 2*margin;
		for (int i = 0; i < octaves; i++) {
			OctaveButton *octaveButton = new OctaveButton();
			octaveButton->box.pos = Vec(0, height / octaves * i + margin);
			octaveButton->box.size = Vec(box.size.x, height / octaves);
			octaveButton->module = module;
			octaveButton->octave = 4 - i;
			addChild(octaveButton);
		}
	}

	void draw(const DrawArgs &args) override {
		// Background
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgFill(args.vg);

		Widget::draw(args);
	}
};


struct OctaveWidget : ModuleWidget {
	OctaveWidget(Octave *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Octave.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 97.253)), module, Octave::CV_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 112.253)), module, Octave::CV_OUTPUT));

		OctaveDisplay *octaveDisplay = new OctaveDisplay();
		octaveDisplay->box.pos = mm2px(Vec(0.0, 14.584));
		octaveDisplay->setModule(module);
		addChild(octaveDisplay);
	}
};


Model *modelOctave = createModel<Octave, OctaveWidget>("Octave");
