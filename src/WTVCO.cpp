#include "plugin.hpp"
#include "Wavetable.hpp"
#include <samplerate.h>


using simd::float_4;


struct WTVCO : Module {
	enum ParamIds {
		MODE_PARAM, // removed
		SOFT_PARAM,
		FREQ_PARAM,
		POS_PARAM,
		FM_PARAM,
		// added in 2.0
		POS_CV_PARAM,
		LINEAR_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FM_INPUT,
		SYNC_INPUT,
		POS_INPUT,
		// added in 2.0
		PITCH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		WAVE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(PHASE_LIGHT, 3),
		SOFT_LIGHT,
		LINEAR_LIGHT,
		NUM_LIGHTS
	};

	Wavetable wavetable;
	bool soft = false;
	bool linear = false;

	float lastPos = 0.f;
	SRC_STATE* src[16];
	// callback state
	uint8_t callbackChannel = 0;
	float callbackPos = 0.f;
	uint32_t callbackIndexes[16] = {};
	static constexpr int CALLBACK_BUFFER_LEN = 16;
	float callbackBuffers[16][CALLBACK_BUFFER_LEN] = {};

	dsp::ClockDivider lightDivider;
	dsp::BooleanTrigger softTrigger;
	dsp::BooleanTrigger linearTrigger;

	WTVCO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configButton(SOFT_PARAM, "Soft sync");
		configButton(LINEAR_PARAM, "Linear frequency modulation");
		configParam(FREQ_PARAM, -75.f, 75.f, 0.f, "Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(POS_PARAM, 0.f, 1.f, 0.f, "Wavetable position", "%", 0.f, 100.f);
		configParam(FM_PARAM, -1.f, 1.f, 0.f, "Frequency modulation", "%", 0.f, 100.f);
		getParamQuantity(FM_PARAM)->randomizeEnabled = false;
		configParam(POS_CV_PARAM, -1.f, 1.f, 0.f, "Wavetable position CV", "%", 0.f, 100.f);
		getParamQuantity(POS_CV_PARAM)->randomizeEnabled = false;
		configInput(FM_INPUT, "Frequency modulation");
		configInput(SYNC_INPUT, "Sync");
		configInput(POS_INPUT, "Wavetable position");
		configInput(PITCH_INPUT, "1V/octave pitch");
		configOutput(WAVE_OUTPUT, "Wave");
		configLight(PHASE_LIGHT, "Phase");

		lightDivider.setDivision(16);

		for (int c = 0; c < 16; c++) {
			src[c] = src_callback_new(srcCallback, SRC_SINC_FASTEST, 1, NULL, this);
			assert(src[c]);
		}

		onReset(ResetEvent());
	}

	~WTVCO() {
		for (int c = 0; c < 16; c++) {
			src_delete(src[c]);
		}
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		soft = false;
		linear = false;
		wavetable.reset();
	}

	void onRandomize(const RandomizeEvent& e) override {
		Module::onRandomize(e);
		soft = random::get<bool>();
		linear = random::get<bool>();
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

	void clearOutput() {
		outputs[WAVE_OUTPUT].setVoltage(0.f);
		outputs[WAVE_OUTPUT].setChannels(1);
		lights[PHASE_LIGHT + 0].setBrightness(0.f);
		lights[PHASE_LIGHT + 1].setBrightness(0.f);
		lights[PHASE_LIGHT + 2].setBrightness(0.f);
	}

	static long srcCallback(void* cbData, float** data) {
		WTVCO* that = (WTVCO*) cbData;
		int c = that->callbackChannel;
		// Get pos
		float posF = that->callbackPos - std::trunc(that->callbackPos);
		size_t pos0 = std::trunc(that->callbackPos);
		size_t pos1 = pos0 + 1;
		// Fill callbackBuffer
		for (int i = 0; i < CALLBACK_BUFFER_LEN; i++) {
			size_t index = (that->callbackIndexes[c] + i) % that->wavetable.waveLen;
			// Get waves
			float out;
			float out0 = that->wavetable.at(index, pos0);
			if (posF > 0.f) {
				float out1 = that->wavetable.at(index, pos1);
				out = crossfade(out0, out1, posF);
			}
			else {
				out = out0;
			}

			that->callbackBuffers[c][i] = out;
		}

		that->callbackIndexes[c] += CALLBACK_BUFFER_LEN;
		data[0] = that->callbackBuffers[c];
		return CALLBACK_BUFFER_LEN;
	}

	void fillInputBuffer(int c) {
	}

	void process(const ProcessArgs& args) override {
		if (linearTrigger.process(params[LINEAR_PARAM].getValue() > 0.f))
			linear ^= true;
		if (softTrigger.process(params[SOFT_PARAM].getValue() > 0.f))
			soft ^= true;

		int channels = std::max({1, inputs[PITCH_INPUT].getChannels(), inputs[FM_INPUT].getChannels()});

		float freqParam = params[FREQ_PARAM].getValue();
		float fmParam = params[FM_PARAM].getValue();
		float posParam = params[POS_PARAM].getValue();
		float posCvParam = params[POS_CV_PARAM].getValue();

		// Check valid wave and wavetable size
		if (wavetable.waveLen < 2) {
			clearOutput();
			return;
		}
		int waveCount = wavetable.getWaveCount();
		if (waveCount < 1) {
			clearOutput();
			return;
		}

		// Iterate channels
		for (int c = 0; c < channels; c += 4) {
			// Calculate frequency in Hz
			float_4 pitch = freqParam / 12.f + inputs[PITCH_INPUT].getPolyVoltageSimd<float_4>(c) + inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam;
			float_4 freq = dsp::FREQ_C4 * simd::pow(2.f, pitch);
			// Limit to Nyquist frequency
			freq = simd::fmin(freq, args.sampleRate / 2.f);

			float_4 ratio = args.sampleRate / freq / wavetable.waveLen;

			// Get wavetable position, scaled from 0 to (waveCount - 1)
			float_4 pos = posParam + inputs[POS_INPUT].getPolyVoltageSimd<float_4>(c) * posCvParam / 10.f;
			pos = simd::clamp(pos);
			pos *= (waveCount - 1);

			if (c == 0)
				lastPos = pos[0];

			float_4 out = 0.f;

			for (int cc = 0; cc < 4 && c + cc < channels; cc++) {
				callbackChannel = c + cc;
				callbackPos = pos[cc];
				// Not sure why this call is needed since we set ratio in src_callback_read(). Perhaps a SRC bug?
				src_set_ratio(src[c + cc], ratio[cc]);
				float outBuf[1];
				long ret = src_callback_read(src[c + cc], ratio[cc], 1, outBuf);
				// DEBUG("ret %ld ratio %f", ret, ratio[cc]);
				if (ret > 0)
					out[cc] = outBuf[0];
			}

			outputs[WAVE_OUTPUT].setVoltageSimd(out * 5.f, c);
		}

		outputs[WAVE_OUTPUT].setChannels(channels);

		// Light
		if (lightDivider.process()) {
			if (channels == 1) {
				// float b = 1.f - phases[0][0];
				// lights[PHASE_LIGHT + 0].setSmoothBrightness(b, args.sampleTime * lightDivider.getDivision());
				// lights[PHASE_LIGHT + 1].setBrightness(0.f);
				// lights[PHASE_LIGHT + 2].setBrightness(0.f);
			}
			else {
				lights[PHASE_LIGHT + 0].setBrightness(0.f);
				lights[PHASE_LIGHT + 1].setBrightness(0.f);
				lights[PHASE_LIGHT + 2].setBrightness(1.f);
			}
			lights[LINEAR_LIGHT].setBrightness(linear);
			lights[SOFT_LIGHT].setBrightness(soft);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		// soft
		json_object_set_new(rootJ, "soft", json_boolean(soft));
		// linear
		json_object_set_new(rootJ, "linear", json_boolean(linear));
		// Merge wavetable
		json_t* wavetableJ = wavetable.toJson();
		json_object_update(rootJ, wavetableJ);
		json_decref(wavetableJ);
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// soft
		json_t* softJ = json_object_get(rootJ, "soft");
		if (softJ)
			soft = json_boolean_value(softJ);
		// linear
		json_t* linearJ = json_object_get(rootJ, "linear");
		if (linearJ)
			linear = json_boolean_value(linearJ);
		// wavetable
		wavetable.fromJson(rootJ);
	}
};


struct WTVCOWidget : ModuleWidget {
	WTVCOWidget(WTVCO* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/WTVCO.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(8.915, 56.388)), module, WTVCO::FREQ_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(26.645, 56.388)), module, WTVCO::POS_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.897, 80.603)), module, WTVCO::FM_PARAM));
		addParam(createLightParamCentered<LEDLightBezel<>>(mm2px(Vec(17.734, 80.603)), module, WTVCO::LINEAR_PARAM, WTVCO::LINEAR_LIGHT));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(28.571, 80.603)), module, WTVCO::POS_CV_PARAM));
		addParam(createLightParamCentered<LEDLightBezel<>>(mm2px(Vec(17.734, 96.859)), module, WTVCO::SOFT_PARAM, WTVCO::SOFT_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.897, 96.813)), module, WTVCO::FM_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(28.571, 96.859)), module, WTVCO::POS_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.897, 113.115)), module, WTVCO::PITCH_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.734, 113.115)), module, WTVCO::SYNC_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(28.571, 113.115)), module, WTVCO::WAVE_OUTPUT));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(17.733, 49.409)), module, WTVCO::PHASE_LIGHT));

		WTDisplay<WTVCO>* display = createWidget<WTDisplay<WTVCO>>(mm2px(Vec(0.004, 13.04)));
		display->box.size = mm2px(Vec(35.56, 29.224));
		display->module = module;
		addChild(display);
	}

	void appendContextMenu(Menu* menu) override {
		WTVCO* module = dynamic_cast<WTVCO*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);

		module->wavetable.appendContextMenu(menu);
	}
};


Model* modelVCO2 = createModel<WTVCO, WTVCOWidget>("VCO2");