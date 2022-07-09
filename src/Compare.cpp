#include "plugin.hpp"


struct Compare : Module {
	enum ParamId {
		B_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		A_INPUT,
		B_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		MAX_OUTPUT,
		MIN_OUTPUT,
		CLIP_OUTPUT,
		LIM_OUTPUT,
		CLIPGATE_OUTPUT,
		LIMGATE_OUTPUT,
		GREATER_OUTPUT,
		LESS_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(CLIP_LIGHT, 2),
		ENUMS(LIM_LIGHT, 2),
		ENUMS(GREATER_LIGHT, 2),
		ENUMS(LESS_LIGHT, 2),
		LIGHTS_LEN
	};

	dsp::ClockDivider lightDivider;

	Compare() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(B_PARAM, -10.f, 10.f, 0.f, "B offset", " V");
		configInput(A_INPUT, "A");
		configInput(B_INPUT, "B");
		configOutput(MAX_OUTPUT, "Maximum");
		configOutput(MIN_OUTPUT, "Minimum");
		configOutput(CLIP_OUTPUT, "Clip");
		configOutput(LIM_OUTPUT, "Limit");
		configOutput(CLIPGATE_OUTPUT, "Clip gate");
		configOutput(LIMGATE_OUTPUT, "Limit gate");
		configOutput(GREATER_OUTPUT, "A>B");
		configOutput(LESS_OUTPUT, "A<B");

		lightDivider.setDivision(32);
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max({1, inputs[A_INPUT].getChannels(), inputs[B_INPUT].getChannels()});
		float bOffset = params[B_PARAM].getValue();

		bool anyClipped = false;
		bool anyLimmed = false;
		bool anyGreater = false;
		bool anyLess = false;

		for (int c = 0; c < channels; c++) {
			float a = inputs[A_INPUT].getVoltage(c);
			float b = inputs[B_INPUT].getVoltage(c) + bOffset;
			float bAbs = std::fabs(b);

			outputs[MAX_OUTPUT].setVoltage(std::max(a, b), c);
			outputs[MIN_OUTPUT].setVoltage(std::min(a, b), c);

			float clip = a;
			bool clipped = false;
			if (bAbs < a) {
				clip = bAbs;
				clipped = true;
			}
			else if (a < -bAbs) {
				clip = -bAbs;
				clipped = true;
			}
			outputs[CLIP_OUTPUT].setVoltage(clip, c);
			outputs[LIM_OUTPUT].setVoltage(a - clip, c);

			outputs[CLIPGATE_OUTPUT].setVoltage(clipped ? 10.f : 0.f, c);
			anyClipped = anyClipped || clipped;
			anyLimmed = anyLimmed || !clipped;
			outputs[LIMGATE_OUTPUT].setVoltage(!clipped ? 10.f : 0.f, c);

			outputs[GREATER_OUTPUT].setVoltage(a > b ? 10.f : 0.f, c);
			anyGreater = anyGreater || (a > b);
			outputs[LESS_OUTPUT].setVoltage(a < b ? 10.f : 0.f, c);
			anyLess = anyLess || (a < b);
		}

		outputs[MAX_OUTPUT].setChannels(channels);
		outputs[MIN_OUTPUT].setChannels(channels);
		outputs[CLIP_OUTPUT].setChannels(channels);
		outputs[LIM_OUTPUT].setChannels(channels);
		outputs[CLIPGATE_OUTPUT].setChannels(channels);
		outputs[LIMGATE_OUTPUT].setChannels(channels);
		outputs[GREATER_OUTPUT].setChannels(channels);
		outputs[LESS_OUTPUT].setChannels(channels);

		if (lightDivider.process()) {
			float lightTime = args.sampleTime * lightDivider.getDivision();
			lights[CLIP_LIGHT + 0].setBrightnessSmooth(anyClipped && channels <= 1, lightTime);
			lights[CLIP_LIGHT + 1].setBrightnessSmooth(anyClipped && channels > 1, lightTime);
			lights[LIM_LIGHT + 0].setBrightnessSmooth(anyLimmed && channels <= 1, lightTime);
			lights[LIM_LIGHT + 1].setBrightnessSmooth(anyLimmed && channels > 1, lightTime);
			lights[GREATER_LIGHT + 0].setBrightnessSmooth(anyGreater && channels <= 1, lightTime);
			lights[GREATER_LIGHT + 1].setBrightnessSmooth(anyGreater && channels > 1, lightTime);
			lights[LESS_LIGHT + 0].setBrightnessSmooth(anyLess && channels <= 1, lightTime);
			lights[LESS_LIGHT + 1].setBrightnessSmooth(anyLess && channels > 1, lightTime);
		}
	}
};


struct CompareWidget : ModuleWidget {
	CompareWidget(Compare* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Compare.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(12.646, 26.755)), module, Compare::B_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.299, 52.31)), module, Compare::A_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.136, 52.31)), module, Compare::B_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.297, 67.53)), module, Compare::MAX_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 67.53)), module, Compare::MIN_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.297, 82.732)), module, Compare::CLIP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 82.732)), module, Compare::LIM_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.297, 97.958)), module, Compare::CLIPGATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 97.958)), module, Compare::LIMGATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.297, 113.115)), module, Compare::GREATER_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 113.115)), module, Compare::LESS_OUTPUT));

		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(11.027, 94.233)), module, Compare::CLIP_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(21.864, 94.233)), module, Compare::LIM_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(11.027, 109.393)), module, Compare::GREATER_LIGHT));
		addChild(createLightCentered<TinyLight<YellowBlueLight<>>>(mm2px(Vec(21.864, 109.393)), module, Compare::LESS_LIGHT));
	}
};


Model* modelCompare = createModel<Compare, CompareWidget>("Compare");