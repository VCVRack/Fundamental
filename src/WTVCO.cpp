#include "plugin.hpp"


struct WTVCO : Module {
	enum ParamIds {
		MODE_PARAM, // removed
		SYNC_PARAM,
		FREQ_PARAM,
		WAVE_PARAM,
		FM_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FM_INPUT,
		SYNC_INPUT,
		WAVE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(PHASE_LIGHT, 3),
		NUM_LIGHTS
	};

	// VoltageControlledOscillator<8, 8, float_4> oscillators[4];
	dsp::ClockDivider lightDivider;

	WTVCO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(MODE_PARAM, 0.f, 1.f, 1.f, "Engine mode", {"Digital", "Analog"});
		configSwitch(SYNC_PARAM, 0.f, 1.f, 1.f, "Sync mode", {"Soft", "Hard"});
		configParam(FREQ_PARAM, -54.f, 54.f, 0.f, "Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(WAVE_PARAM, 0.f, 3.f, 1.5f, "Wave");
		configParam(FM_PARAM, 0.f, 1.f, 1.f, "Frequency modulation", "%", 0.f, 100.f);
		configInput(FM_INPUT, "Frequency modulation");
		configInput(SYNC_INPUT, "Sync");
		configInput(WAVE_INPUT, "Wave type");
		configOutput(OUT_OUTPUT, "Audio");

		lightDivider.setDivision(16);
	}

	void process(const ProcessArgs& args) override {
		// float freqParam = params[FREQ_PARAM].getValue() / 12.f;
		// float fmParam = dsp::quadraticBipolar(params[FM_PARAM].getValue());
		// float waveParam = params[WAVE_PARAM].getValue();

		// int channels = std::max(inputs[FM_INPUT].getChannels(), 1);

		// for (int c = 0; c < channels; c += 4) {
		// 	auto* oscillator = &oscillators[c / 4];
		// 	oscillator->channels = std::min(channels - c, 4);
		// 	oscillator->analog = (params[MODE_PARAM].getValue() > 0.f);
		// 	oscillator->soft = (params[SYNC_PARAM].getValue() <= 0.f);

		// 	float_4 pitch = freqParam;
		// 	pitch += fmParam * inputs[FM_INPUT].getVoltageSimd<float_4>(c);
		// 	oscillator->setPitch(pitch);

		// 	oscillator->syncEnabled = inputs[SYNC_INPUT].isConnected();
		// 	oscillator->process(args.sampleTime, inputs[SYNC_INPUT].getPolyVoltageSimd<float_4>(c));

		// 	// Outputs
		// 	if (outputs[OUT_OUTPUT].isConnected()) {
		// 		float_4 wave = simd::clamp(waveParam + inputs[WAVE_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f * 3.f, 0.f, 3.f);
		// 		float_4 v = 0.f;
		// 		v += oscillator->sin() * simd::fmax(0.f, 1.f - simd::fabs(wave - 0.f));
		// 		v += oscillator->tri() * simd::fmax(0.f, 1.f - simd::fabs(wave - 1.f));
		// 		v += oscillator->saw() * simd::fmax(0.f, 1.f - simd::fabs(wave - 2.f));
		// 		v += oscillator->sqr() * simd::fmax(0.f, 1.f - simd::fabs(wave - 3.f));
		// 		outputs[OUT_OUTPUT].setVoltageSimd(5.f * v, c);
		// 	}
		// }

		// outputs[OUT_OUTPUT].setChannels(channels);

		// // Light
		// if (lightDivider.process()) {
		// 	if (channels == 1) {
		// 		float lightValue = oscillators[0].light()[0];
		// 		lights[PHASE_LIGHT + 0].setSmoothBrightness(-lightValue, args.sampleTime * lightDivider.getDivision());
		// 		lights[PHASE_LIGHT + 1].setSmoothBrightness(lightValue, args.sampleTime * lightDivider.getDivision());
		// 		lights[PHASE_LIGHT + 2].setBrightness(0.f);
		// 	}
		// 	else {
		// 		lights[PHASE_LIGHT + 0].setBrightness(0.f);
		// 		lights[PHASE_LIGHT + 1].setBrightness(0.f);
		// 		lights[PHASE_LIGHT + 2].setBrightness(1.f);
		// 	}
		// }
	}
};


struct WTVCODisplay : LedDisplay {
	WTVCODisplay() {
		box.size = mm2px(Vec(35.56, 29.021));
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
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(26.645, 56.388)), module, WTVCO::WAVE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.897, 80.603)), module, WTVCO::FM_PARAM));
		// addParam(createParamCentered<LEDButton>(mm2px(Vec(17.734, 80.603)), module, WTVCO::LFM_PARAM));
		// addParam(createParamCentered<Trimpot>(mm2px(Vec(28.571, 80.603)), module, WTVCO::POSCV_PARAM));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(17.734, 96.859)), module, WTVCO::SYNC_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.897, 96.813)), module, WTVCO::FM_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(28.571, 96.859)), module, WTVCO::WAVE_INPUT));
		// addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.897, 113.115)), module, WTVCO::PITCH_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.734, 113.115)), module, WTVCO::SYNC_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(28.571, 113.115)), module, WTVCO::OUT_OUTPUT));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(17.733, 49.409)), module, WTVCO::PHASE_LIGHT));

		addChild(createWidget<WTVCODisplay>(mm2px(Vec(0.0, 13.039))));
	}
};


Model* modelVCO2 = createModel<WTVCO, WTVCOWidget>("VCO2");