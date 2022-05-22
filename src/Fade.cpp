#include "plugin.hpp"


using simd::float_4;


struct Fade : Module {
	enum ParamId {
		CROSSFADE_PARAM,
		CROSSFADE_CV_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CROSSFADE_INPUT,
		IN1_INPUT,
		IN2_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT1_OUTPUT,
		OUT2_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Fade() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(CROSSFADE_PARAM, 0.f, 1.f, 0.5f, "Crossfade", "%", 0, 100);
		configParam(CROSSFADE_CV_PARAM, -1.f, 1.f, 0.f, "Crossfade CV", "%", 0, 100);
		configInput(CROSSFADE_INPUT, "Crossfade");
		configInput(IN1_INPUT, "Ch 1");
		configInput(IN2_INPUT, "Ch 2");
		configOutput(OUT1_OUTPUT, "Ch 1");
		configOutput(OUT2_OUTPUT, "Ch 2");

		configBypass(IN1_INPUT, OUT1_OUTPUT);
		configBypass(IN2_INPUT, OUT2_OUTPUT);
	}

	void process(const ProcessArgs& args) override {
		if (!outputs[OUT1_OUTPUT].isConnected() && !outputs[OUT2_OUTPUT].isConnected())
			return;

		int channels = std::max({1, inputs[IN1_INPUT].getChannels(), inputs[IN2_INPUT].getChannels()});

		for (int c = 0; c < channels; c += 4) {
			// Get crossfade amount
			float_4 crossfade = params[CROSSFADE_PARAM].getValue();
			crossfade += inputs[CROSSFADE_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f * params[CROSSFADE_CV_PARAM].getValue();
			crossfade = clamp(crossfade, 0.f, 1.f);

			// Get inputs
			float_4 in1 = inputs[IN1_INPUT].getPolyVoltageSimd<float_4>(c);
			float_4 in2 = inputs[IN2_INPUT].getPolyVoltageSimd<float_4>(c);

			// Calculate outputs
			float_4 out1 = (1.f - crossfade) * in1 + crossfade * in2;
			float_4 out2 = crossfade * in1 + (1.f - crossfade) * in2;
			outputs[OUT1_OUTPUT].setVoltageSimd(out1, c);
			outputs[OUT2_OUTPUT].setVoltageSimd(out2, c);
		}

		outputs[OUT1_OUTPUT].setChannels(channels);
		outputs[OUT2_OUTPUT].setChannels(channels);
	}
};


struct FadeWidget : ModuleWidget {
	FadeWidget(Fade* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Fade.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 24.723)), module, Fade::CROSSFADE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 37.064)), module, Fade::CROSSFADE_CV_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 52.987)), module, Fade::CROSSFADE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 67.53)), module, Fade::IN1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 82.732)), module, Fade::IN2_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 97.923)), module, Fade::OUT1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 113.115)), module, Fade::OUT2_OUTPUT));
	}
};


Model* modelFade = createModel<Fade, FadeWidget>("Fade");