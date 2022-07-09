#include "plugin.hpp"


struct Process : Module {
	enum ParamId {
		SLEW_PARAM,
		GATE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		SLEW_INPUT,
		IN_INPUT,
		GATE_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		SH1_OUTPUT,
		SH2_OUTPUT,
		TH_OUTPUT,
		HT_OUTPUT,
		SLEW_OUTPUT,
		GLIDE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		GATE_LIGHT,
		LIGHTS_LEN
	};

	bool state[16] = {};
	float sample1[16] = {};
	float sample2[16] = {};
	float holdValue[16] = {};
	float slewValue[16] = {};
	float glideValue[16] = {};

	Process() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(SLEW_PARAM, std::log2(1e-3f), std::log2(10.f), std::log2(1e-3f), "Slew", " ms/V", 2, 1000);
		configButton(GATE_PARAM, "Gate");
		configInput(SLEW_INPUT, "Slew");
		configInput(IN_INPUT, "Voltage");
		configInput(GATE_INPUT, "Gate");
		configOutput(SH1_OUTPUT, "Sample & hold");
		configOutput(SH2_OUTPUT, "Sample & hold 2");
		configOutput(TH_OUTPUT, "Track & hold");
		configOutput(HT_OUTPUT, "Hold & track");
		configOutput(SLEW_OUTPUT, "Slew");
		configOutput(GLIDE_OUTPUT, "Glide");
	}

	void process(const ProcessArgs& args) override {
		int channels = inputs[IN_INPUT].getChannels();
		bool gateButton = params[GATE_PARAM].getValue() > 0.f;

		for (int c = 0; c < channels; c++) {
			float in = inputs[IN_INPUT].getVoltage(c);

			float slewPitch = -params[SLEW_PARAM].getValue() - inputs[SLEW_INPUT].getPolyVoltage(c);
			// V/s
			float slew = dsp::approxExp2_taylor5(slewPitch + 30.f) / 1073741824;

			float gateValue = inputs[GATE_INPUT].getPolyVoltage(c);

			if (!state[c]) {
				if (gateValue >= 2.f || gateButton) {
					// Triggered
					state[c] = true;
					// Hold and track
					holdValue[c] = in;
					// Sample and hold
					sample2[c] = sample1[c];
					sample1[c] = in;
					// Glide
					// TODO delay timer
					glideValue[c] = in;
				}
			}
			else {
				if (gateValue <= 0.1f && !gateButton) {
					// Untriggered
					state[c] = false;
					// Track and hold
					holdValue[c] = in;
				}
			}

			// Slew each value
			float slewDelta = slew * args.sampleTime;
			if (state[c]) {
				slewValue[c] = in;
			}
			else {
				slewValue[c] += clamp(in - slewValue[c], -slewDelta, slewDelta);
			}
			glideValue[c] += clamp(in - glideValue[c], -slewDelta, slewDelta);

			outputs[SH1_OUTPUT].setVoltage(sample1[c], c);
			outputs[SH2_OUTPUT].setVoltage(sample2[c], c);
			outputs[TH_OUTPUT].setVoltage(state[c] ? holdValue[c] : in, c);
			outputs[HT_OUTPUT].setVoltage(state[c] ? in : holdValue[c], c);
			outputs[SLEW_OUTPUT].setVoltage(slewValue[c], c);
			outputs[GLIDE_OUTPUT].setVoltage(glideValue[c], c);
		}

		outputs[SH1_OUTPUT].setChannels(channels);
		outputs[SH2_OUTPUT].setChannels(channels);
		outputs[TH_OUTPUT].setChannels(channels);
		outputs[HT_OUTPUT].setChannels(channels);
		outputs[SLEW_OUTPUT].setChannels(channels);
		outputs[GLIDE_OUTPUT].setChannels(channels);

		lights[GATE_LIGHT].setBrightness(gateButton);
	}
};


struct ProcessWidget : ModuleWidget {
	ProcessWidget(Process* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Process.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(12.646, 26.755)), module, Process::SLEW_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(18.136, 52.31)), module, Process::GATE_PARAM, Process::GATE_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.299, 52.31)), module, Process::SLEW_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.297, 67.53)), module, Process::IN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.122, 67.53)), module, Process::GATE_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.297, 82.732)), module, Process::SH1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 82.732)), module, Process::SH2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.297, 97.958)), module, Process::TH_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 97.923)), module, Process::HT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.297, 113.115)), module, Process::SLEW_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 113.115)), module, Process::GLIDE_OUTPUT));
	}
};


Model* modelProcess = createModel<Process, ProcessWidget>("Process");