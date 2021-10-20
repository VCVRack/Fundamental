#include "plugin.hpp"
#include <osdialog.h>


using simd::float_4;


struct WTLFO : Module {
	enum ParamId {
		OFFSET_PARAM,
		INVERT_PARAM,
		FREQ_PARAM,
		POS_PARAM,
		FM_PARAM,
		// added in 2.0
		POS_CV_PARAM,
		NUM_PARAMS
	};
	enum InputId {
		FM_INPUT,
		RESET_INPUT,
		POS_INPUT,
		// added in 2.0
		CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputId {
		WAVE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightId {
		ENUMS(PHASE_LIGHT, 3),
		OFFSET_LIGHT,
		INVERT_LIGHT,
		NUM_LIGHTS
	};

	// All waves concatenated
	std::vector<float> wavetable;
	// Number of points in each wave
	size_t waveLen = 0;
	bool offset = false;
	bool invert = false;
	std::string filename;

	float_4 phases[4] = {};
	float lastPos = 0.f;

	dsp::ClockDivider lightDivider;
	dsp::BooleanTrigger offsetTrigger;
	dsp::BooleanTrigger invertTrigger;

	WTLFO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		// TODO Change to momentary with backward compatibility in fromJson()
		configButton(OFFSET_PARAM, "Offset 0-10V");
		configButton(INVERT_PARAM, "Invert wave");
		configParam(FREQ_PARAM, -8.f, 10.f, 1.f, "Frequency", " Hz", 2, 1);
		configParam(POS_PARAM, 0.f, 1.f, 0.f, "Wavetable position", "%", 0.f, 100.f);
		configParam(FM_PARAM, -1.f, 1.f, 0.f, "Frequency modulation", "%", 0.f, 100.f);
		configParam(POS_CV_PARAM, -1.f, 1.f, 0.f, "Wavetable position CV", "%", 0.f, 100.f);
		configInput(FM_INPUT, "Frequency modulation");
		configInput(RESET_INPUT, "Reset");
		configInput(POS_INPUT, "Wavetable position");
		configInput(CLOCK_INPUT, "Clock");
		configOutput(WAVE_OUTPUT, "Wavetable");
		configLight(PHASE_LIGHT, "Phase");

		lightDivider.setDivision(16);
		onReset(ResetEvent());
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);

		// Build geometric waveforms
		filename = "Basic.wav";
		wavetable.clear();
		waveLen = 1024;
		wavetable.resize(waveLen * 4);

		for (size_t i = 0; i < waveLen; i++) {
			float p = float(i) / waveLen;
			float sin = std::sin(2 * float(M_PI) * p);
			wavetable[i + 0 * waveLen] = sin;
			float tri = (p < 0.25f) ? 4*p : (p < 0.75f) ? 2 - 4*p : 4*p - 4;
			wavetable[i + 1 * waveLen] = tri;
			float saw = (p < 0.5f) ? 2*p : 2*p - 2;
			wavetable[i + 2 * waveLen] = saw;
			float sqr = (p < 0.5f) ? 1 : -1;
			wavetable[i + 3 * waveLen] = sqr;
		}

		// Reset state
		for (int c = 0; c < 16; c += 4) {
			phases[c / 4] = 0.f;
		}
	}

	void clearOutput() {
		outputs[WAVE_OUTPUT].setVoltage(0.f);
		outputs[WAVE_OUTPUT].setChannels(1);
		lights[PHASE_LIGHT + 0].setBrightness(0.f);
		lights[PHASE_LIGHT + 1].setBrightness(0.f);
		lights[PHASE_LIGHT + 2].setBrightness(0.f);
	}

	void process(const ProcessArgs& args) override {
		float freqParam = params[FREQ_PARAM].getValue();
		float fmParam = params[FM_PARAM].getValue();
		float posParam = params[POS_PARAM].getValue();
		float posCvParam = params[POS_CV_PARAM].getValue();

		if (offsetTrigger.process(params[OFFSET_PARAM].getValue() > 0.f))
			offset ^= true;
		if (invertTrigger.process(params[INVERT_PARAM].getValue() > 0.f))
			invert ^= true;

		int channels = std::max(1, inputs[FM_INPUT].getChannels());

		// Check valid wave and wavetable size
		if (waveLen < 2) {
			clearOutput();
			return;
		}
		int wavetableLen = wavetable.size() / waveLen;
		if (wavetableLen < 1) {
			clearOutput();
			return;
		}

		for (int c = 0; c < channels; c += 4) {
			// Calculate frequency in Hz
			float_4 pitch = freqParam + inputs[FM_INPUT].getVoltageSimd<float_4>(c) * fmParam;
			float_4 freq = simd::pow(2.f, pitch);

			// Accumulate phase
			float_4 phase = phases[c / 4];
			phase += freq * args.sampleTime;
			// Wrap phase
			phase -= simd::trunc(phase);
			phases[c / 4] = phase;
			// Scale phase from 0 to waveLen
			phase *= waveLen;

			// Get wavetable position, scaled from 0 to (wavetableLen - 1)
			float_4 pos = posParam + inputs[POS_INPUT].getPolyVoltageSimd<float_4>(c) * posCvParam / 10.f;
			pos = simd::clamp(pos);
			pos *= (wavetableLen - 1);

			if (c == 0)
				lastPos = pos[0];

			// Get wavetable points
			float_4 out = 0.f;
			for (int i = 0; i < 4 && c + i < channels; i++) {
				// Get wave indexes
				float phaseF = phase[i] - std::trunc(phase[i]);
				size_t i0 = std::trunc(phase[i]);
				size_t i1 = (i0 + 1) % waveLen;
				// Get pos indexes
				float posF = pos[0] - std::trunc(pos[0]);
				size_t pos0 = std::trunc(pos[0]);
				size_t pos1 = pos0 + 1;
				// Get waves
				float out0 = crossfade(wavetable[i0 + pos0 * waveLen], wavetable[i1 + pos0 * waveLen], phaseF);
				if (posF > 0.f) {
					float out1 = crossfade(wavetable[i0 + pos1 * waveLen], wavetable[i1 + pos1 * waveLen], phaseF);
					out[i] = crossfade(out0, out1, posF);
				}
				else {
					out[i] = out0;
				}
			}

			// Invert and offset
			if (invert)
				out *= -1.f;
			if (offset)
				out += 1.f;

			outputs[WAVE_OUTPUT].setVoltageSimd(out * 5.f, c);
		}

		outputs[WAVE_OUTPUT].setChannels(channels);

		// Light
		if (lightDivider.process()) {
			if (channels == 1) {
				float b = 1.f - phases[0][0];
				lights[PHASE_LIGHT + 0].setSmoothBrightness(b, args.sampleTime * lightDivider.getDivision());
				lights[PHASE_LIGHT + 1].setBrightness(0.f);
				lights[PHASE_LIGHT + 2].setBrightness(0.f);
			}
			else {
				lights[PHASE_LIGHT + 0].setBrightness(0.f);
				lights[PHASE_LIGHT + 1].setBrightness(0.f);
				lights[PHASE_LIGHT + 2].setBrightness(1.f);
			}
		}
	}

	void loadWavetable(std::string path) {
		// TODO
	}

	void loadWavetableDialog() {
		static const char WAVETABLE_FILTERS[] = "WAV (.wav):wav";
		osdialog_filters* filters = osdialog_filters_parse(WAVETABLE_FILTERS);
		DEFER({osdialog_filters_free(filters);});

		char* pathC = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
		if (!pathC) {
			// Fail silently
			return;
		}
		std::string path = pathC;
		std::free(pathC);

		loadWavetable(path);
	}
};


template <class TModule>
struct WTDisplay : LedDisplay {
	TModule* module;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			if (!module)
				return;

			// Draw filename text
			std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
			nvgFontSize(args.vg, 13);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -2);
			nvgFillColor(args.vg, SCHEME_YELLOW);
			nvgText(args.vg, 4.0, 13.0, module->filename.c_str(), NULL);

			// Get wavetable metadata
			size_t waveLen = module->waveLen;
			if (waveLen < 2)
				return;

			size_t wavetableLen = module->wavetable.size() / waveLen;
			if (module->lastPos > wavetableLen - 1)
				return;
			float posF = module->lastPos - std::trunc(module->lastPos);
			size_t pos0 = std::trunc(module->lastPos);

			// Draw scope
			nvgScissor(args.vg, RECT_ARGS(args.clipBox));
			nvgBeginPath(args.vg);
			Vec scopePos = Vec(0.0, 13.0);
			Rect scopeRect = Rect(scopePos, box.size - scopePos);
			scopeRect = scopeRect.shrink(Vec(4, 5));

			for (size_t i = 0; i <= waveLen; i++) {
				// Get wave value
				float wave;
				float wave0 = module->wavetable[(i % waveLen) + waveLen * pos0];
				if (posF > 0.f) {
					float wave1 = module->wavetable[(i % waveLen) + waveLen * (pos0 + 1)];
					wave = crossfade(wave0, wave1, posF);
				}
				else {
					wave = wave0;
				}

				// Add point to line
				Vec p;
				p.x = float(i) / waveLen;
				p.y = 0.5f - 0.5f * wave;
				p = scopeRect.pos + scopeRect.size * p;
				if (i == 0)
					nvgMoveTo(args.vg, VEC_ARGS(p));
				else
					nvgLineTo(args.vg, VEC_ARGS(p));
			}
			nvgLineCap(args.vg, NVG_ROUND);
			nvgMiterLimit(args.vg, 2.f);
			nvgStrokeWidth(args.vg, 1.5f);
			nvgStrokeColor(args.vg, SCHEME_YELLOW);
			nvgStroke(args.vg);
		}
		LedDisplay::drawLayer(args, layer);
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (module)
				module->loadWavetableDialog();
			e.consume(this);
		}
		LedDisplay::onButton(e);
	}

	void onPathDrop(const PathDropEvent& e) override {
		if (!module)
			return;
		if (e.paths.empty())
			return;
		std::string path = e.paths[0];
		if (system::getExtension(path) != ".wav")
			return;
		module->loadWavetable(path);
		e.consume(this);
	}
};


struct WTLFOWidget : ModuleWidget {
	WTLFOWidget(WTLFO* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/WTLFO.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(8.913, 56.388)), module, WTLFO::FREQ_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(26.647, 56.388)), module, WTLFO::POS_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.987, 80.603)), module, WTLFO::FM_PARAM));
		addParam(createLightParamCentered<LEDLightBezel<>>(mm2px(Vec(17.824, 80.517)), module, WTLFO::INVERT_PARAM, WTLFO::INVERT_LIGHT));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(28.662, 80.536)), module, WTLFO::POS_CV_PARAM));
		addParam(createLightParamCentered<LEDLightBezel<>>(mm2px(Vec(17.824, 96.859)), module, WTLFO::OFFSET_PARAM, WTLFO::OFFSET_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.987, 96.859)), module, WTLFO::FM_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(28.662, 96.859)), module, WTLFO::POS_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.987, 113.115)), module, WTLFO::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.824, 113.115)), module, WTLFO::RESET_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(28.662, 113.115)), module, WTLFO::WAVE_OUTPUT));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(17.731, 49.409)), module, WTLFO::PHASE_LIGHT));

		WTDisplay<WTLFO>* display = createWidget<WTDisplay<WTLFO>>(mm2px(Vec(0.004, 13.04)));
		display->box.size = mm2px(Vec(35.56, 29.224));
		display->module = module;
		addChild(display);
	}

	void appendContextMenu(Menu* menu) override {
		WTLFO* module = dynamic_cast<WTLFO*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createMenuItem("Load wavetable...", "",
			[=]() {module->loadWavetableDialog();}
		));
	}
};


Model* modelLFO2 = createModel<WTLFO, WTLFOWidget>("LFO2");