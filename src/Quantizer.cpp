#include "plugin.hpp"


struct Quantizer : Module {
	enum ParamIds {
		OFFSET_PARAM, // TODO
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
		configParam(OFFSET_PARAM, -1.f, 1.f, 0.f, "Pre-offset", " semitones", 0.f, 12.f);
		configInput(PITCH_INPUT, "1V/octave pitch");
		configOutput(PITCH_OUTPUT, "Pitch");
		configBypass(PITCH_INPUT, PITCH_OUTPUT);

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

	void process(const ProcessArgs& args) override {
		bool playingNotes[12] = {};
		int channels = std::max(inputs[PITCH_INPUT].getChannels(), 1);
		float offsetParam = params[OFFSET_PARAM].getValue();

		for (int c = 0; c < channels; c++) {
			float pitch = inputs[PITCH_INPUT].getVoltage(c);
			pitch += offsetParam;
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

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_t* enabledNotesJ = json_array();
		for (int i = 0; i < 12; i++) {
			json_array_insert_new(enabledNotesJ, i, json_boolean(enabledNotes[i]));
		}
		json_object_set_new(rootJ, "enabledNotes", enabledNotesJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* enabledNotesJ = json_object_get(rootJ, "enabledNotes");
		if (enabledNotesJ) {
			for (int i = 0; i < 12; i++) {
				json_t* enabledNoteJ = json_array_get(enabledNotesJ, i);
				if (enabledNoteJ)
					enabledNotes[i] = json_boolean_value(enabledNoteJ);
			}
		}
		updateRanges();
	}
};


struct QuantizerButton : OpaqueWidget {
	int note;
	Quantizer* module;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;

		Rect r = box.zeroPos();
		const float margin = mm2px(1.0);
		Rect rMargin = r.grow(Vec(margin, margin));

		nvgBeginPath(args.vg);
		nvgRect(args.vg, RECT_ARGS(rMargin));
		nvgFillColor(args.vg, nvgRGB(0x12, 0x12, 0x12));
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgRect(args.vg, RECT_ARGS(r));
		if (module ? module->playingNotes[note] : (note == 0)) {
			nvgFillColor(args.vg, SCHEME_YELLOW);
		}
		else if (module ? module->enabledNotes[note] : true) {
			nvgFillColor(args.vg, nvgRGB(0x7f, 0x6b, 0x0a));
		}
		else {
			nvgFillColor(args.vg, nvgRGB(0x40, 0x40, 0x40));
		}
		nvgFill(args.vg);
	}

	void onDragStart(const event::DragStart& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			module->enabledNotes[note] ^= true;
			module->updateRanges();
		}
		OpaqueWidget::onDragStart(e);
	}

	void onDragEnter(const event::DragEnter& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			QuantizerButton* origin = dynamic_cast<QuantizerButton*>(e.origin);
			if (origin) {
				module->enabledNotes[note] = module->enabledNotes[origin->note];;
				module->updateRanges();
			}
		}
		OpaqueWidget::onDragEnter(e);
	}
};


struct QuantizerDisplay : LedDisplay {
	void setModule(Quantizer* module) {
		std::vector<Vec> noteAbsPositions = {
			mm2px(Vec(2.242, 60.54)),
			mm2px(Vec(2.242, 58.416)),
			mm2px(Vec(2.242, 52.043)),
			mm2px(Vec(2.242, 49.919)),
			mm2px(Vec(2.242, 45.67)),
			mm2px(Vec(2.242, 39.298)),
			mm2px(Vec(2.242, 37.173)),
			mm2px(Vec(2.242, 30.801)),
			mm2px(Vec(2.242, 28.677)),
			mm2px(Vec(2.242, 22.304)),
			mm2px(Vec(2.242, 20.18)),
			mm2px(Vec(2.242, 15.931)),
		};
		std::vector<Vec> noteSizes = {
			mm2px(Vec(10.734, 5.644)),
			mm2px(Vec(8.231, 3.52)),
			mm2px(Vec(10.734, 7.769)),
			mm2px(Vec(8.231, 3.52)),
			mm2px(Vec(10.734, 5.644)),
			mm2px(Vec(10.734, 5.644)),
			mm2px(Vec(8.231, 3.52)),
			mm2px(Vec(10.734, 7.769)),
			mm2px(Vec(8.231, 3.52)),
			mm2px(Vec(10.734, 7.768)),
			mm2px(Vec(8.231, 3.52)),
			mm2px(Vec(10.734, 5.644)),
		};

		// White notes
		static const std::vector<int> whiteNotes = {0, 2, 4, 5, 7, 9, 11};
		for (int note : whiteNotes) {
			QuantizerButton* quantizerButton = new QuantizerButton();
			quantizerButton->box.pos = noteAbsPositions[note] - box.pos;
			quantizerButton->box.size = noteSizes[note];
			quantizerButton->module = module;
			quantizerButton->note = note;
			addChild(quantizerButton);
		}
		// Black notes
		static const std::vector<int> blackNotes = {1, 3, 6, 8, 10};
		for (int note : blackNotes) {
			QuantizerButton* quantizerButton = new QuantizerButton();
			quantizerButton->box.pos = noteAbsPositions[note] - box.pos;
			quantizerButton->box.size = noteSizes[note];
			quantizerButton->module = module;
			quantizerButton->note = note;
			addChild(quantizerButton);
		}
	}
};


struct QuantizerWidget : ModuleWidget {
	QuantizerWidget(Quantizer* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Quantizer.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 80.551)), module, Quantizer::OFFSET_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 96.859)), module, Quantizer::PITCH_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 113.115)), module, Quantizer::PITCH_OUTPUT));

		QuantizerDisplay* quantizerDisplay = createWidget<QuantizerDisplay>(mm2px(Vec(0.0, 13.039)));
		quantizerDisplay->box.size = mm2px(Vec(15.24, 55.88));
		quantizerDisplay->setModule(module);
		addChild(quantizerDisplay);
	}
};


Model* modelQuantizer = createModel<Quantizer, QuantizerWidget>("Quantizer");