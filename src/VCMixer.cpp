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
		// Get number of poly channels for mix output
		int mixChannels = 1;
		for (int i = 0; i < 4; i++) {
			mixChannels = std::max(mixChannels, inputs[CH_INPUTS + i].getChannels());
		}
		float mix[16] = {};

		// Channel strips
		for (int i = 0; i < 4; i++) {
			int channels = 1;
			float in[16] = {};
			float sum = 0.f;

			if (inputs[CH_INPUTS + i].isConnected()) {
				channels = inputs[CH_INPUTS + i].getChannels();

				// Get input
				inputs[CH_INPUTS + i].readVoltages(in);

				// Apply fader gain
				float gain = std::pow(params[LVL_PARAMS + i].getValue(), 2.f);
				for (int c = 0; c < channels; c++) {
					in[c] *= gain;
				}

				// Apply CV gain
				if (inputs[CV_INPUTS + i].isConnected()) {
					for (int c = 0; c < channels; c++) {
						float cv = clamp(inputs[CV_INPUTS + i].getPolyVoltage(c) / 10.f, 0.f, 1.f);
						in[c] *= cv;
					}
				}

				// Add to mix
				if (channels == 1) {
					// Copy the mono signal to all mix channels
					for (int c = 0; c < mixChannels; c++) {
						mix[c] += in[0];
					}
				}
				else {
					// Copy each poly channel to the corresponding mix channel
					for (int c = 0; c < channels; c++) {
						mix[c] += in[c];
					}
				}

				// Sum channel for VU meter
				for (int c = 0; c < channels; c++) {
					sum += in[c];
				}
			}

			chMeters[i].process(args.sampleTime, sum / 5.f);

			// Set channel output
			if (outputs[CH_OUTPUTS + i].isConnected()) {
				outputs[CH_OUTPUTS + i].setChannels(channels);
				outputs[CH_OUTPUTS + i].writeVoltages(in);
			}
		}

		// Mix output
		if (outputs[MIX_OUTPUT].isConnected()) {
			// Apply mix knob gain
			float gain = params[MIX_LVL_PARAM].getValue();
			for (int c = 0; c < mixChannels; c++) {
				mix[c] *= gain;
			}

			// Apply mix CV gain
			if (inputs[MIX_CV_INPUT].isConnected()) {
				for (int c = 0; c < mixChannels; c++) {
					float cv = clamp(inputs[MIX_CV_INPUT].getPolyVoltage(c) / 10.f, 0.f, 1.f);
					mix[c] *= cv;
				}
			}

			// Set mix output
			outputs[MIX_OUTPUT].setChannels(mixChannels);
			outputs[MIX_OUTPUT].writeVoltages(mix);
		}

		// VU lights
		if (lightDivider.process()) {
			for (int i = 0; i < 4; i++) {
				float b = chMeters[i].getBrightness(-24.f, 0.f);
				lights[LVL_LIGHTS + i].setBrightness(b);
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
