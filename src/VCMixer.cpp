#include "plugin.hpp"


struct VCMixer : Module {
	enum ParamIds {
		MIX_LVL_PARAM,
		ENUMS(LVL_PARAMS, 4),
		NUM_PARAMS
	};
	enum InputIds {
		MIX_CV_INPUT,
		ENUMS(CH_INPUTS, 4),
		ENUMS(CV_INPUTS, 4),
		NUM_INPUTS
	};
	enum OutputIds {
		MIX_OUTPUT,
		ENUMS(CH_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(LVL_LIGHTS, 4),
		NUM_LIGHTS
	};

	dsp::VuMeter2 chMeters[4];
	dsp::ClockDivider lightDivider;

	VCMixer() {
		config(0, 0, 0, 0);
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		// x^1 scaling up to 6 dB
		configParam(MIX_LVL_PARAM, 0.0, 2.0, 1.0, "Mix level", " dB", -10, 20);
		// x^2 scaling up to 6 dB
		configParam(LVL_PARAMS + 0, 0.0, M_SQRT2, 1.0, "Channel 1 level", " dB", -10, 40);
		configParam(LVL_PARAMS + 1, 0.0, M_SQRT2, 1.0, "Channel 2 level", " dB", -10, 40);
		configParam(LVL_PARAMS + 2, 0.0, M_SQRT2, 1.0, "Channel 3 level", " dB", -10, 40);
		configParam(LVL_PARAMS + 3, 0.0, M_SQRT2, 1.0, "Channel 4 level", " dB", -10, 40);
		configInput(MIX_CV_INPUT, "Mix CV");
		for (int i = 0; i < 4; i++)
			configInput(CH_INPUTS + i, string::f("Channel %d", i + 1));
		for (int i = 0; i < 4; i++)
			configInput(CV_INPUTS + i, string::f("Channel %d CV", i + 1));
		configOutput(MIX_OUTPUT, "Mix");
		for (int i = 0; i < 4; i++)
			configOutput(CH_OUTPUTS + i, string::f("Channel %d", i + 1));

		lightDivider.setDivision(512);
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		// Get number of poly channels for mix output
		int channels = 1;
		for (int i = 0; i < 4; i++) {
			channels = std::max(channels, inputs[CH_INPUTS + i].getChannels());
		}

		// Iterate polyphony channels (voices)
		float chSum[4] = {};

		for (int c = 0; c < channels; c += 4) {
			float_4 mix = 0.f;

			// Channel strips
			for (int i = 0; i < 4; i++) {
				float_4 out = 0.f;

				if (inputs[CH_INPUTS + i].isConnected()) {
					// Get input
					out = inputs[CH_INPUTS + i].getPolyVoltageSimd<float_4>(c);

					// Apply fader gain
					float gain = std::pow(params[LVL_PARAMS + i].getValue(), 2.f);
					out *= gain;

					// Apply CV gain
					if (inputs[CV_INPUTS + i].isConnected()) {
						float_4 cv = inputs[CV_INPUTS + i].getPolyVoltageSimd<float_4>(c) / 10.f;
						cv = simd::fmax(0.f, cv);
						out *= cv;
					}

					// Sum channel for VU meter
					for (int c2 = 0; c2 < 4; c2++) {
						chSum[i] += out[c2];
					}

					// Add to mix
					mix += out;
				}

				// Set channel output
				outputs[CH_OUTPUTS + i].setVoltageSimd(out, c);
			}

			// Mix output
			if (outputs[MIX_OUTPUT].isConnected()) {
				// Apply mix knob gain
				float gain = params[MIX_LVL_PARAM].getValue();
				mix *= gain;

				// Apply mix CV gain
				if (inputs[MIX_CV_INPUT].isConnected()) {
					float_4 cv = inputs[MIX_CV_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f;
					cv = simd::fmax(0.f, cv);
					mix *= cv;
				}

				// Set mix output
				outputs[MIX_OUTPUT].setVoltageSimd(mix, c);
			}
		}

		// Set output channels
		for (int i = 0; i < 4; i++) {
			outputs[CH_OUTPUTS + i].setChannels(channels);
		}
		outputs[MIX_OUTPUT].setChannels(channels);

		// VU lights
		for (int i = 0; i < 4; i++) {
			chMeters[i].process(args.sampleTime, chSum[i] / 5.f);
		}
		if (lightDivider.process()) {
			for (int i = 0; i < 4; i++) {
				lights[LVL_LIGHTS + i].setBrightness(chMeters[i].getBrightness(-24.f, 0.f));
			}
		}
	}
};


struct VCMixerWidget : ModuleWidget {
	VCMixerWidget(VCMixer* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/VCMixer.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(6.604, 33.605)), module, VCMixer::LVL_PARAMS + 0, VCMixer::LVL_LIGHTS + 0));
		addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(17.441, 33.605)), module, VCMixer::LVL_PARAMS + 1, VCMixer::LVL_LIGHTS + 1));
		addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(28.279, 33.605)), module, VCMixer::LVL_PARAMS + 2, VCMixer::LVL_LIGHTS + 2));
		addParam(createLightParamCentered<VCVLightSlider<YellowLight>>(mm2px(Vec(39.116, 33.605)), module, VCMixer::LVL_PARAMS + 3, VCMixer::LVL_LIGHTS + 3));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.776, 64.366)), module, VCMixer::MIX_LVL_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.604, 64.347)), module, VCMixer::MIX_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.604, 80.549)), module, VCMixer::CV_INPUTS + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.441, 80.549)), module, VCMixer::CV_INPUTS + 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(28.279, 80.549)), module, VCMixer::CV_INPUTS + 2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(39.116, 80.549)), module, VCMixer::CV_INPUTS + 3));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.604, 96.859)), module, VCMixer::CH_INPUTS + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.441, 96.859)), module, VCMixer::CH_INPUTS + 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(28.279, 96.859)), module, VCMixer::CH_INPUTS + 2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(39.116, 96.821)), module, VCMixer::CH_INPUTS + 3));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.116, 64.347)), module, VCMixer::MIX_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.604, 113.115)), module, VCMixer::CH_OUTPUTS + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(17.441, 113.115)), module, VCMixer::CH_OUTPUTS + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(28.279, 113.115)), module, VCMixer::CH_OUTPUTS + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.116, 113.115)), module, VCMixer::CH_OUTPUTS + 3));
	}
};


Model* modelVCMixer = createModel<VCMixer, VCMixerWidget>("VCMixer");
