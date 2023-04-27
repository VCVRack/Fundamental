#include "plugin.hpp"


struct SlewFilter {
	float value = 0.f;

	float process(float in, float slew) {
		value += math::clamp(in - value, -slew, slew);
		return value;
	}
	float jump(float in) {
		value = in;
		return value;
	}
	float getValue() {
		return value;
	}
};


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

	struct Engine {
		bool state = false;
		// For glide to turn on after 1ms
		float onTime = 0.f;
		float sample1 = 0.f;
		float sample2 = 0.f;
		SlewFilter sample1Filter;
		SlewFilter sample2Filter;
		float holdValue = 0.f;
		SlewFilter slewFilter;
		SlewFilter glideFilter;
	};
	Engine engines[16];

	Process() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		struct SlewQuantity : ParamQuantity {
			float getDisplayValue() override {
				if (getValue() <= getMinValue())
					return 0.f;
				return ParamQuantity::getDisplayValue();
			}
		};
		configParam<SlewQuantity>(SLEW_PARAM, std::log2(1e-3f), std::log2(10.f), std::log2(1e-3f), "Slew", " ms/V", 2, 1000);
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
		float slewParam = params[SLEW_PARAM].getValue();
		// Hard-left param means infinite slew
		if (slewParam <= std::log2(1e-3f))
			slewParam = -INFINITY;

		for (int c = 0; c < channels; c++) {
			Engine& e = engines[c];

			float in = inputs[IN_INPUT].getVoltage(c);
			float gateValue = inputs[GATE_INPUT].getPolyVoltage(c);

			// Slew rate in V/s
			float slew = INFINITY;
			if (std::isfinite(slewParam)) {
				float slewPitch = slewParam + inputs[SLEW_INPUT].getPolyVoltage(c);
				slew = dsp::exp2_taylor5(-slewPitch + 30.f) / std::exp2(30.f);
			}
			float slewDelta = slew * args.sampleTime;

			// Gate trigger/untrigger
			if (!e.state) {
				if (gateValue >= 2.f || gateButton) {
					// Triggered
					e.state = true;
					e.onTime = 0.f;
					// Hold and track
					e.holdValue = in;
					// Sample and hold
					e.sample2 = e.sample1;
					e.sample1 = in;
				}
			}
			else {
				if (gateValue <= 0.1f && !gateButton) {
					// Untriggered
					e.state = false;
					// Track and hold
					e.holdValue = in;
				}
			}

			// Track & hold
			float tr = e.state ? e.holdValue : in;
			float ht = e.state ? in : e.holdValue;

			// Slew
			if (e.state) {
				e.slewFilter.jump(in);
				e.onTime += args.sampleTime;
			}
			else {
				e.slewFilter.process(in, slewDelta);
			}

			// Glide
			// Wait 1ms before considering gate as legato
			if (e.state && e.onTime > 1e-3f) {
				e.glideFilter.process(in, slewDelta);
			}
			else {
				e.glideFilter.jump(in);
			}

			outputs[SH1_OUTPUT].setVoltage(e.sample1Filter.process(e.sample1, slewDelta), c);
			outputs[SH2_OUTPUT].setVoltage(e.sample2Filter.process(e.sample2, slewDelta), c);
			outputs[TH_OUTPUT].setVoltage(tr, c);
			outputs[HT_OUTPUT].setVoltage(ht, c);
			outputs[SLEW_OUTPUT].setVoltage(e.slewFilter.getValue(), c);
			outputs[GLIDE_OUTPUT].setVoltage(e.glideFilter.getValue(), c);
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