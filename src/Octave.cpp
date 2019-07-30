#include "plugin.hpp"


struct Octave : Module {
	enum ParamIds {
		OCTAVE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,
		OCTAVE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		PITCH_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	Octave() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(OCTAVE_PARAM, -4.f, 4.f, 0.f, "Octave shift");
	}

	void process(const ProcessArgs &args) override {
		int channels = std::max(inputs[PITCH_INPUT].getChannels(), 1);
		float octaveParam = params[OCTAVE_PARAM].getValue();

		for (int c = 0; c < channels; c++) {
			float octave = octaveParam + inputs[OCTAVE_INPUT].getPolyVoltage(c);
			octave = std::round(octave);
			float pitch = inputs[PITCH_INPUT].getVoltage(c);
			pitch += octave;
			outputs[PITCH_OUTPUT].setVoltage(pitch, c);
		}
		outputs[PITCH_OUTPUT].setChannels(channels);
	}

	void dataFromJson(json_t *rootJ) override {
		// In Fundamental 1.1.1 and earlier, the octave param was internal data.
		json_t *octaveJ = json_object_get(rootJ, "octave");
		if (octaveJ) {
			params[OCTAVE_PARAM].setValue(json_integer_value(octaveJ));
		}
	}
};


struct OctaveButton : Widget {
	int octave;

	void draw(const DrawArgs &args) override {
		Vec c = box.size.div(2);

		int activeOctave = 0;
		ParamWidget *paramWidget = getAncestorOfType<ParamWidget>();
		if (paramWidget && paramWidget->paramQuantity) {
			activeOctave = std::round(paramWidget->paramQuantity->getValue());
		}

		if (activeOctave == octave) {
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

	void onDragHover(const event::DragHover &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			e.consume(this);
		}
		Widget::onDragHover(e);
	}

	void onDragEnter(const event::DragEnter &e) override;
};


struct OctaveParam : ParamWidget {
	OctaveParam() {
		box.size = mm2px(Vec(15.24, 63.0));
		const int octaves = 9;
		const float margin = mm2px(2.0);
		float height = box.size.y - 2*margin;
		for (int i = 0; i < octaves; i++) {
			OctaveButton *octaveButton = new OctaveButton();
			octaveButton->box.pos = Vec(0, height / octaves * i + margin);
			octaveButton->box.size = Vec(box.size.x, height / octaves);
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

		ParamWidget::draw(args);
	}
};


inline void OctaveButton::onDragEnter(const event::DragEnter &e) {
	if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
		OctaveParam *origin = dynamic_cast<OctaveParam*>(e.origin);
		if (origin) {
			ParamWidget *paramWidget = getAncestorOfType<ParamWidget>();
			if (paramWidget && paramWidget->paramQuantity) {
				paramWidget->paramQuantity->setValue(octave);
			}
		}
	}
	Widget::onDragEnter(e);
}



struct OctaveWidget : ModuleWidget {
	OctaveWidget(Octave *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Octave.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 82.753)), module, Octave::OCTAVE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 97.253)), module, Octave::PITCH_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 112.253)), module, Octave::PITCH_OUTPUT));

		addParam(createParam<OctaveParam>(mm2px(Vec(0.0, 12.817)), module, Octave::OCTAVE_PARAM));
	}
};


Model *modelOctave = createModel<Octave, OctaveWidget>("Octave");
