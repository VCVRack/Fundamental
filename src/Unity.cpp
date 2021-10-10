#include "plugin.hpp"


struct Unity : Module {
	enum ParamIds {
		AVG1_PARAM,
		AVG2_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(IN_INPUTS, 2 * 6),
		NUM_INPUTS
	};
	enum OutputIds {
		MIX1_OUTPUT,
		INV1_OUTPUT,
		MIX2_OUTPUT,
		INV2_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(VU_LIGHTS, 2 * 5),
		NUM_LIGHTS
	};

	bool merge = false;
	dsp::VuMeter2 vuMeters[2];
	dsp::ClockDivider lightDivider;

	Unity() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(AVG1_PARAM, 0.0, 1.0, 0.0, "Channel 1 mode", {"Sum", "Average"});
		configSwitch(AVG2_PARAM, 0.0, 1.0, 0.0, "Channel 2 mode", {"Sum", "Average"});
		for (int i = 0; i < 2; i++) {
			for (int j = 0; j < 6; j++) {
				configInput(IN_INPUTS + i * 6 + j, string::f("Channel %d #%d", i + 1, j + 1));
			}
		}
		configOutput(MIX1_OUTPUT, "Channel 1 mix");
		configOutput(INV1_OUTPUT, "Channel 1 inverse mix");
		configOutput(MIX2_OUTPUT, "Channel 2 mix");
		configOutput(INV2_OUTPUT, "Channel 2 inverse mix");

		lightDivider.setDivision(256);
	}

	void process(const ProcessArgs& args) override {
		float mix[2] = {};
		int count[2] = {};

		for (int i = 0; i < 2; i++) {
			// Inputs
			for (int j = 0; j < 6; j++) {
				mix[i] += inputs[IN_INPUTS + 6 * i + j].getVoltage();
				if (inputs[IN_INPUTS + 6 * i + j].isConnected())
					count[i]++;
			}
		}

		// Combine
		if (merge) {
			mix[0] += mix[1];
			mix[1] = mix[0];
			count[0] += count[1];
			count[1] = count[0];
		}

		for (int i = 0; i < 2; i++) {
			// Params
			if (count[i] > 0 && (int) std::round(params[AVG1_PARAM + i].getValue()) == 1)
				mix[i] /= count[i];

			// Outputs
			outputs[MIX1_OUTPUT + 2 * i].setVoltage(mix[i]);
			outputs[INV1_OUTPUT + 2 * i].setVoltage(-mix[i]);
			vuMeters[i].process(args.sampleTime, mix[i] / 10.f);
		}

		if (lightDivider.process()) {
			// Lights
			for (int i = 0; i < 2; i++) {
				lights[VU_LIGHTS + 5 * i + 0].setBrightness(vuMeters[i].getBrightness(0.f, 0.f));
				for (int j = 1; j < 5; j++) {
					lights[VU_LIGHTS + 5 * i + j].setBrightness(vuMeters[i].getBrightness(-6.f * (j + 1), -6.f * j));
				}
			}
		}
	}

	void onReset() override {
		merge = false;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		// merge
		json_object_set_new(rootJ, "merge", json_boolean(merge));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// merge
		json_t* mergeJ = json_object_get(rootJ, "merge");
		if (mergeJ)
			merge = json_boolean_value(mergeJ);
	}
};


struct UnityWidget : ModuleWidget {
	UnityWidget(Unity* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Unity.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

		addParam(createParam<CKSS>(mm2px(Vec(12.867, 52.961)), module, Unity::AVG1_PARAM));
		addParam(createParam<CKSS>(mm2px(Vec(12.867, 107.006)), module, Unity::AVG2_PARAM));

		addInput(createInput<PJ301MPort>(mm2px(Vec(2.361, 17.144)), module, Unity::IN_INPUTS + 0 * 6 + 0));
		addInput(createInput<PJ301MPort>(mm2px(Vec(19.907, 17.144)), module, Unity::IN_INPUTS + 0 * 6 + 1));
		addInput(createInput<PJ301MPort>(mm2px(Vec(2.361, 28.145)), module, Unity::IN_INPUTS + 0 * 6 + 2));
		addInput(createInput<PJ301MPort>(mm2px(Vec(19.907, 28.145)), module, Unity::IN_INPUTS + 0 * 6 + 3));
		addInput(createInput<PJ301MPort>(mm2px(Vec(2.361, 39.145)), module, Unity::IN_INPUTS + 0 * 6 + 4));
		addInput(createInput<PJ301MPort>(mm2px(Vec(19.907, 39.145)), module, Unity::IN_INPUTS + 0 * 6 + 5));
		addInput(createInput<PJ301MPort>(mm2px(Vec(2.361, 71.145)), module, Unity::IN_INPUTS + 1 * 6 + 0));
		addInput(createInput<PJ301MPort>(mm2px(Vec(19.907, 71.145)), module, Unity::IN_INPUTS + 1 * 6 + 1));
		addInput(createInput<PJ301MPort>(mm2px(Vec(2.361, 82.145)), module, Unity::IN_INPUTS + 1 * 6 + 2));
		addInput(createInput<PJ301MPort>(mm2px(Vec(19.907, 82.145)), module, Unity::IN_INPUTS + 1 * 6 + 3));
		addInput(createInput<PJ301MPort>(mm2px(Vec(2.361, 93.144)), module, Unity::IN_INPUTS + 1 * 6 + 4));
		addInput(createInput<PJ301MPort>(mm2px(Vec(19.907, 93.144)), module, Unity::IN_INPUTS + 1 * 6 + 5));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(2.361, 54.15)), module, Unity::MIX1_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(19.907, 54.15)), module, Unity::INV1_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(2.361, 108.144)), module, Unity::MIX2_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(19.907, 108.144)), module, Unity::INV2_OUTPUT));

		addChild(createLight<MediumLight<RedLight>>(mm2px(Vec(13.652, 19.663)), module, Unity::VU_LIGHTS + 0 * 5 + 0));
		addChild(createLight<MediumLight<YellowLight>>(mm2px(Vec(13.652, 25.163)), module, Unity::VU_LIGHTS + 0 * 5 + 1));
		addChild(createLight<MediumLight<GreenLight>>(mm2px(Vec(13.652, 30.663)), module, Unity::VU_LIGHTS + 0 * 5 + 2));
		addChild(createLight<MediumLight<GreenLight>>(mm2px(Vec(13.652, 36.162)), module, Unity::VU_LIGHTS + 0 * 5 + 3));
		addChild(createLight<MediumLight<GreenLight>>(mm2px(Vec(13.652, 41.662)), module, Unity::VU_LIGHTS + 0 * 5 + 4));
		addChild(createLight<MediumLight<RedLight>>(mm2px(Vec(13.652, 73.663)), module, Unity::VU_LIGHTS + 1 * 5 + 0));
		addChild(createLight<MediumLight<YellowLight>>(mm2px(Vec(13.652, 79.163)), module, Unity::VU_LIGHTS + 1 * 5 + 1));
		addChild(createLight<MediumLight<GreenLight>>(mm2px(Vec(13.652, 84.663)), module, Unity::VU_LIGHTS + 1 * 5 + 2));
		addChild(createLight<MediumLight<GreenLight>>(mm2px(Vec(13.652, 90.162)), module, Unity::VU_LIGHTS + 1 * 5 + 3));
		addChild(createLight<MediumLight<GreenLight>>(mm2px(Vec(13.652, 95.662)), module, Unity::VU_LIGHTS + 1 * 5 + 4));
	}

	void appendContextMenu(Menu* menu) override {
		Unity* module = dynamic_cast<Unity*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolPtrMenuItem("Merge channels 1 & 2", "", &module->merge));
	}
};


Model* modelUnity = createModel<Unity, UnityWidget>("Unity");
