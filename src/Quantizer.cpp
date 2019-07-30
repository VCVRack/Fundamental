#include "plugin.hpp"


struct Quantizer : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		PITCH_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	bool enabledNotes[12];
	// Intervals [i / 24, (i+1) / 24) V mapping to the closest enabled note
	int ranges[24];
	bool playingNotes[12];

	Quantizer() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		onReset();
	}

	void onReset() override {
		for (int i = 0; i < 12; i++) {
			enabledNotes[i] = true;
		}
		updateRanges();
	}

	void onRandomize() override {
		for (int i = 0; i < 12; i++) {
			enabledNotes[i] = (random::uniform() < 0.5f);
		}
		updateRanges();
	}

	void process(const ProcessArgs &args) override {
		bool playingNotes[12] = {};
		int channels = std::max(inputs[PITCH_INPUT].getChannels(), 1);

		for (int c = 0; c < channels; c++) {
			float pitch = inputs[PITCH_INPUT].getVoltage(c);
			int range = std::floor(pitch * 24);
			int octave = eucDiv(range, 24);
			range -= octave * 24;
			int note = ranges[range] + octave * 12;
			playingNotes[eucMod(note, 12)] = true;
			pitch = float(note) / 12;
			outputs[PITCH_OUTPUT].setVoltage(pitch, c);
		}
		outputs[PITCH_OUTPUT].setChannels(channels);
		std::memcpy(this->playingNotes, playingNotes, sizeof(playingNotes));
	}

	void updateRanges() {
		// Check if no notes are enabled
		bool anyEnabled = false;
		for (int note = 0; note < 12; note++) {
			if (enabledNotes[note]) {
				anyEnabled = true;
				break;
			}
		}
		// Find closest notes for each range
		for (int i = 0; i < 24; i++) {
			int closestNote = 0;
			int closestDist = INT_MAX;
			for (int note = -12; note <= 24; note++) {
				int dist = std::abs((i + 1) / 2 - note);
				// Ignore enabled state if no notes are enabled
				if (anyEnabled && !enabledNotes[eucMod(note, 12)]) {
					continue;
				}
				if (dist < closestDist) {
					closestNote = note;
					closestDist = dist;
				}
				else {
					// If dist increases, we won't find a better one.
					break;
				}
			}
			ranges[i] = closestNote;
		}
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_t *enabledNotesJ = json_array();
		for (int i = 0; i < 12; i++) {
			json_array_insert_new(enabledNotesJ, i, json_boolean(enabledNotes[i]));
		}
		json_object_set_new(rootJ, "enabledNotes", enabledNotesJ);

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *enabledNotesJ = json_object_get(rootJ, "enabledNotes");
		if (enabledNotesJ) {
			for (int i = 0; i < 12; i++) {
				json_t *enabledNoteJ = json_array_get(enabledNotesJ, i);
				if (enabledNoteJ)
					enabledNotes[i] = json_boolean_value(enabledNoteJ);
			}
		}
		updateRanges();
	}
};


struct QuantizerButton : OpaqueWidget {
	int note;
	Quantizer *module;

	void draw(const DrawArgs &args) override {
		const float margin = mm2px(1.5);
		Rect r = box.zeroPos().grow(Vec(margin, margin / 2).neg());
		nvgBeginPath(args.vg);
		nvgRect(args.vg, RECT_ARGS(r));
		if (module ? module->playingNotes[note] : (note == 0)) {
			nvgFillColor(args.vg, nvgRGB(0xff, 0xd7, 0x14));
		}
		else if (module ? module->enabledNotes[note] : true) {
			nvgFillColor(args.vg, nvgRGB(0x7f, 0x6b, 0x0a));
		}
		else {
			nvgFillColor(args.vg, nvgRGB(0x40, 0x36, 0x05));
		}
		nvgFill(args.vg);
	}

	void onDragStart(const event::DragStart &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			module->enabledNotes[note] ^= true;
			module->updateRanges();
		}
		OpaqueWidget::onDragStart(e);
	}

	void onDragEnter(const event::DragEnter &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			QuantizerButton *origin = dynamic_cast<QuantizerButton*>(e.origin);
			if (origin) {
				module->enabledNotes[note] = module->enabledNotes[origin->note];;
				module->updateRanges();
			}
		}
		OpaqueWidget::onDragEnter(e);
	}
};


struct QuantizerDisplay : OpaqueWidget {
	void setModule(Quantizer *module) {
		const float margin = mm2px(1.5) / 2;
		box.size = mm2px(Vec(15.24, 72.0));
		const int notes = 12;
		const float height = box.size.y - 2 * margin;
		for (int note = 0; note < notes; note++) {
			QuantizerButton *quantizerButton = new QuantizerButton();
			quantizerButton->box.pos = Vec(0, margin + height / notes * note);
			quantizerButton->box.size = Vec(box.size.x, height / notes);
			quantizerButton->module = module;
			quantizerButton->note = note;
			addChild(quantizerButton);
		}
	}

	void draw(const DrawArgs &args) override {
		// Background
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgFill(args.vg);

		OpaqueWidget::draw(args);
	}
};


struct QuantizerWidget : ModuleWidget {
	QuantizerWidget(Quantizer *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Quantizer.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 97.253)), module, Quantizer::PITCH_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 112.253)), module, Quantizer::PITCH_OUTPUT));

		QuantizerDisplay *quantizerDisplay = createWidget<QuantizerDisplay>(mm2px(Vec(0.0, 14.585)));
		quantizerDisplay->setModule(module);
		addChild(quantizerDisplay);
	}
};


Model *modelQuantizer = createModel<Quantizer, QuantizerWidget>("Quantizer");