#include "plugin.hpp"
#include "Wavetable.hpp"


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

	Wavetable wavetable;

	float_4 phases[4] = {};
	float lastPos = 0.f;
	float clockFreq = 1.f;
	dsp::Timer clockTimer;

	dsp::ClockDivider lightDivider;
	dsp::BooleanTrigger offsetTrigger;
	dsp::BooleanTrigger invertTrigger;
	dsp::SchmittTrigger clockTrigger;
	dsp::TSchmittTrigger<float_4> resetTriggers[4];

	WTLFO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(OFFSET_PARAM, 0.f, 1.f, 1.f, "Offset", {"Bipolar", "Unipolar"});
		configSwitch(INVERT_PARAM, 0.f, 1.f, 0.f, "Invert");

		struct FrequencyQuantity : ParamQuantity {
			float getDisplayValue() override {
				WTLFO* module = reinterpret_cast<WTLFO*>(this->module);
				if (module->clockFreq == 2.f) {
					unit = " Hz";
					displayMultiplier = 1.f;
				}
				else {
					unit = "x";
					displayMultiplier = 1 / 2.f;
				}
				return ParamQuantity::getDisplayValue();
			}
		};
		configParam<FrequencyQuantity>(FREQ_PARAM, -8.f, 10.f, 1.f, "Frequency", " Hz", 2, 1);
		configParam(POS_PARAM, 0.f, 1.f, 0.f, "Wavetable position", "%", 0.f, 100.f);
		configParam(FM_PARAM, -1.f, 1.f, 0.f, "Frequency modulation", "%", 0.f, 100.f);
		getParamQuantity(FM_PARAM)->randomizeEnabled = false;
		configParam(POS_CV_PARAM, -1.f, 1.f, 0.f, "Wavetable position CV", "%", 0.f, 100.f);
		getParamQuantity(POS_CV_PARAM)->randomizeEnabled = false;

		configInput(FM_INPUT, "Frequency modulation");
		configInput(RESET_INPUT, "Reset");
		configInput(POS_INPUT, "Wavetable position");
		configInput(CLOCK_INPUT, "Clock");

		configOutput(WAVE_OUTPUT, "Wavetable");

		configLight(PHASE_LIGHT, "Phase");

		lightDivider.setDivision(16);
		onReset();
	}

	void onReset() override {
		wavetable.reset();

		// Reset state
		for (int c = 0; c < 16; c += 4) {
			phases[c / 4] = 0.f;
		}
		clockFreq = 1.f;
		clockTimer.reset();
	}

	void onAdd(const AddEvent& e) override {
		std::string path = system::join(getPatchStorageDirectory(), "wavetable.wav");
		// Silently fails
		wavetable.load(path);
	}

	void onSave(const SaveEvent& e) override {
		if (!wavetable.samples.empty()) {
			std::string path = system::join(createPatchStorageDirectory(), "wavetable.wav");
			wavetable.save(path);
		}
	}

	void process(const ProcessArgs& args) override {
		float freqParam = params[FREQ_PARAM].getValue();
		float fmParam = params[FM_PARAM].getValue();
		float posParam = params[POS_PARAM].getValue();
		float posCvParam = params[POS_CV_PARAM].getValue();
		bool offset = (params[OFFSET_PARAM].getValue() > 0.f);
		bool invert = (params[INVERT_PARAM].getValue() > 0.f);

		// Clock
		if (inputs[CLOCK_INPUT].isConnected()) {
			clockTimer.process(args.sampleTime);

			if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f)) {
				float clockFreq = 1.f / clockTimer.getTime();
				clockTimer.reset();
				if (0.001f <= clockFreq && clockFreq <= 1000.f) {
					this->clockFreq = clockFreq;
				}
			}
		}
		else {
			// Default frequency when clock is unpatched
			clockFreq = 2.f;
		}

		int channels = std::max(1, inputs[FM_INPUT].getChannels());

		// Check valid wave and wavetable size
		int waveCount = wavetable.getWaveCount();
		if (!wavetable.loading && wavetable.waveLen >= 2 && waveCount >= 1) {
			// Iterate channels
			for (int c = 0; c < channels; c += 4) {
				// Calculate frequency in Hz
				float_4 pitch = freqParam + inputs[FM_INPUT].getVoltageSimd<float_4>(c) * fmParam;
				float_4 freq = clockFreq / 2.f * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
				freq = simd::fmin(freq, 1024.f);

				// Accumulate phase
				float_4 phase = phases[c / 4];
				phase += freq * args.sampleTime;
				// Wrap phase
				phase -= simd::trunc(phase);
				// Reset phase
				float_4 reset = resetTriggers[c / 4].process(simd::rescale(inputs[RESET_INPUT].getPolyVoltageSimd<float_4>(c), 0.1f, 2.f, 0.f, 1.f));
				phase = simd::ifelse(reset, 0.f, phase);
				phases[c / 4] = phase;
				// Scale phase from 0 to waveLen
				phase *= wavetable.waveLen;

				// Get wavetable position, scaled from 0 to (waveCount - 1)
				float_4 pos = posParam + inputs[POS_INPUT].getPolyVoltageSimd<float_4>(c) * posCvParam / 10.f;
				pos = simd::clamp(pos);
				pos *= (waveCount - 1);

				if (c == 0)
					lastPos = pos[0];

				// Get wavetable points
				float_4 out = 0.f;
				for (int cc = 0; cc < 4 && c + cc < channels; cc++) {
					// Get wave indexes
					float phaseF = phase[cc] - std::trunc(phase[cc]);
					size_t i0 = std::trunc(phase[cc]);
					size_t i1 = (i0 + 1) % wavetable.waveLen;
					// Get pos indexes
					float posF = pos[cc] - std::trunc(pos[cc]);
					size_t pos0 = std::trunc(pos[cc]);
					size_t pos1 = pos0 + 1;
					// Get waves
					float out0 = crossfade(wavetable.at(pos0, i0), wavetable.at(pos0, i1), phaseF);
					if (posF > 0.f) {
						float out1 = crossfade(wavetable.at(pos1, i0), wavetable.at(pos1, i1), phaseF);
						out[cc] = crossfade(out0, out1, posF);
					}
					else {
						out[cc] = out0;
					}
				}

				// Invert and offset
				if (invert)
					out *= -1.f;
				if (offset)
					out += 1.f;

				outputs[WAVE_OUTPUT].setVoltageSimd(out * 5.f, c);
			}
		}
		else {
			// Wavetable is invalid, so set 0V
			for (int c = 0; c < channels; c += 4) {
				outputs[WAVE_OUTPUT].setVoltageSimd(float_4(0.f), c);
			}
		}

		outputs[WAVE_OUTPUT].setChannels(channels);

		// Light
		if (lightDivider.process()) {
			if (channels == 1) {
				float b = 1.f - phases[0][0];
				lights[PHASE_LIGHT + 0].setSmoothBrightness(b, args.sampleTime * lightDivider.getDivision());
				lights[PHASE_LIGHT + 1].setSmoothBrightness(b, args.sampleTime * lightDivider.getDivision());
				lights[PHASE_LIGHT + 2].setBrightness(0.f);
			}
			else {
				lights[PHASE_LIGHT + 0].setBrightness(0.f);
				lights[PHASE_LIGHT + 1].setBrightness(0.f);
				lights[PHASE_LIGHT + 2].setBrightness(1.f);
			}
			lights[OFFSET_LIGHT].setBrightness(offset);
			lights[INVERT_LIGHT].setBrightness(invert);
		}
	}

	void paramsFromJson(json_t* rootJ) override {
		// In <2.0, there were no attenuverters, so set them to 1.0 in case they are not overwritten.
		params[POS_CV_PARAM].setValue(1.f);
		Module::paramsFromJson(rootJ);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		// Merge wavetable
		json_t* wavetableJ = wavetable.toJson();
		json_object_update(rootJ, wavetableJ);
		json_decref(wavetableJ);
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// wavetable
		wavetable.fromJson(rootJ);
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
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(17.824, 80.517)), module, WTLFO::INVERT_PARAM, WTLFO::INVERT_LIGHT));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(28.662, 80.536)), module, WTLFO::POS_CV_PARAM));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(17.824, 96.859)), module, WTLFO::OFFSET_PARAM, WTLFO::OFFSET_LIGHT));

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

		module->wavetable.appendContextMenu(menu);
	}
};


Model* modelLFO2 = createModel<WTLFO, WTLFOWidget>("LFO2");