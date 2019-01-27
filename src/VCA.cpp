#include "Fundamental.hpp"


struct VCA : Module {
	enum ParamIds {
		LEVEL1_PARAM,
		LEVEL2_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		EXP1_INPUT,
		LIN1_INPUT,
		IN1_INPUT,
		EXP2_INPUT,
		LIN2_INPUT,
		IN2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT1_OUTPUT,
		OUT2_OUTPUT,
		NUM_OUTPUTS
	};

	VCA() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
		params[LEVEL1_PARAM].config(0.0, 1.0, 0.0, "Ch 1 level", "%", 0, 100);
		params[LEVEL2_PARAM].config(0.0, 1.0, 0.0, "Ch 2 level", "%", 0, 100);
	}

	void stepChannel(InputIds in, ParamIds level, InputIds lin, InputIds exp, OutputIds out) {
		float v = inputs[in].value * params[level].value;
		if (inputs[lin].active)
			v *= clamp(inputs[lin].value / 10.0f, 0.0f, 1.0f);
		const float expBase = 50.0f;
		if (inputs[exp].active)
			v *= rescale(std::pow(expBase, clamp(inputs[exp].value / 10.0f, 0.0f, 1.0f)), 1.0f, expBase, 0.0f, 1.0f);
		outputs[out].value = v;
	}

	void step() override {
		stepChannel(IN1_INPUT, LEVEL1_PARAM, LIN1_INPUT, EXP1_INPUT, OUT1_OUTPUT);
		stepChannel(IN2_INPUT, LEVEL2_PARAM, LIN2_INPUT, EXP2_INPUT, OUT2_OUTPUT);
	}
};



struct VCAWidget : ModuleWidget {
	VCAWidget(VCA *module) {
		setModule(module);
		setPanel(SVG::load(asset::plugin(plugin, "res/VCA.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<RoundLargeBlackKnob>(mm2px(Vec(6.35, 19.11753)), module, VCA::LEVEL1_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(mm2px(Vec(6.35, 74.80544)), module, VCA::LEVEL2_PARAM));

		addInput(createInput<PJ301MPort>(mm2px(Vec(2.5907, 38.19371)), module, VCA::EXP1_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(14.59752, 38.19371)), module, VCA::LIN1_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(2.5907, 52.80642)), module, VCA::IN1_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(2.5907, 93.53435)), module, VCA::EXP2_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(14.59752, 93.53435)), module, VCA::LIN2_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(2.5907, 108.14706)), module, VCA::IN2_INPUT));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(14.59752, 52.80642)), module, VCA::OUT1_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(14.59752, 108.14706)), module, VCA::OUT2_OUTPUT));
	}
};


Model *modelVCA = createModel<VCA, VCAWidget>("VCA");


struct VCA_1 : Module {
	enum ParamIds {
		LEVEL_PARAM,
		EXP_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CV_INPUT,
		IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	float lastCv = 0.f;

	VCA_1() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		params[LEVEL_PARAM].config(0.0, 1.0, 1.0, "Level", "%", 0, 100);
		params[EXP_PARAM].config(0.0, 1.0, 1.0, "Response mode");
	}

	void step() override {
		float cv = inputs[CV_INPUT].getNormalVoltage(10.f) / 10.f;
		if ((int) params[EXP_PARAM].value == 0)
			cv = std::pow(cv, 4.f);
		lastCv = cv;
		outputs[OUT_OUTPUT].value = inputs[IN_INPUT].value * params[LEVEL_PARAM].value * cv;
	}
};


struct VCA_1VUKnob : SliderKnob {
	VCA_1 *module = NULL;

	VCA_1VUKnob() {
		box.size = mm2px(Vec(10, 46));
	}

	void draw(NVGcontext *vg) override {
		float lastCv = module ? module->lastCv : 1.f;

		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 2.0);
		nvgFillColor(vg, nvgRGB(0, 0, 0));
		nvgFill(vg);

		const int segs = 25;
		const Vec margin = Vec(3, 3);
		Rect r = box.zeroPos().grow(margin.neg());

		for (int i = 0; i < segs; i++) {
			float value = paramQuantity ? paramQuantity->getValue() : 1.f;
			float segValue = clamp(value * segs - (segs - i - 1), 0.f, 1.f);
			float amplitude = value * lastCv;
			float segAmplitude = clamp(amplitude * segs - (segs - i - 1), 0.f, 1.f);
			nvgBeginPath(vg);
			nvgRect(vg, r.pos.x, r.pos.y + r.size.y / segs * i + 0.5,
				r.size.x, r.size.y / segs - 1.0);
			if (segValue > 0.f) {
				nvgFillColor(vg, color::alpha(nvgRGBf(0.33, 0.33, 0.33), segValue));
				nvgFill(vg);
			}
			if (segAmplitude > 0.f) {
				nvgFillColor(vg, color::alpha(SCHEME_GREEN, segAmplitude));
				nvgFill(vg);
			}
		}
	}
};


struct VCA_1Widget : ModuleWidget {
	VCA_1Widget(VCA_1 *module) {
		setModule(module);
		setPanel(SVG::load(asset::plugin(plugin, "res/VCA-1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		VCA_1VUKnob *levelParam = createParam<VCA_1VUKnob>(mm2px(Vec(2.62103, 12.31692)), module, VCA_1::LEVEL_PARAM);
		levelParam->module = module;
		addParam(levelParam);
		addParam(createParam<CKSS>(mm2px(Vec(5.24619, 79.9593)), module, VCA_1::EXP_PARAM));

		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51261, 60.4008)), module, VCA_1::CV_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 97.74977)), module, VCA_1::IN_INPUT));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.51398, 108.64454)), module, VCA_1::OUT_OUTPUT));
	}
};


Model *modelVCA_1 = createModel<VCA_1, VCA_1Widget>("VCA-1");
