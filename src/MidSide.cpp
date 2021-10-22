#include "plugin.hpp"


struct MidSide : Module {
	enum ParamIds {
		ENC_WIDTH_PARAM,
		DEC_WIDTH_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENC_WIDTH_INPUT,
		ENC_LEFT_INPUT,
		ENC_RIGHT_INPUT,
		DEC_WIDTH_INPUT,
		DEC_MID_INPUT,
		DEC_SIDES_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENC_MID_OUTPUT,
		ENC_SIDES_OUTPUT,
		DEC_LEFT_OUTPUT,
		DEC_RIGHT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	MidSide() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ENC_WIDTH_PARAM, 0.f, 2.f, 1.f, "Encoder width", "%", 0, 100);
		configParam(DEC_WIDTH_PARAM, 0.f, 2.f, 1.f, "Decoder width", "%", 0, 100);
		configInput(ENC_WIDTH_INPUT, "Encoder width");
		configInput(ENC_LEFT_INPUT, "Encoder left");
		configInput(ENC_RIGHT_INPUT, "Encoder right");
		configInput(DEC_WIDTH_INPUT, "Decoder width");
		configInput(DEC_MID_INPUT, "Decoder mid");
		configInput(DEC_SIDES_INPUT, "Decoder sides");
		configOutput(ENC_MID_OUTPUT, "Encoder mid");
		configOutput(ENC_SIDES_OUTPUT, "Encoder sides");
		configOutput(DEC_LEFT_OUTPUT, "Decoder left");
		configOutput(DEC_RIGHT_OUTPUT, "Decoder right");
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		// Encoder
		{
			int channels = std::max(inputs[ENC_LEFT_INPUT].getChannels(), inputs[ENC_RIGHT_INPUT].getChannels());
			outputs[ENC_MID_OUTPUT].setChannels(channels);
			outputs[ENC_SIDES_OUTPUT].setChannels(channels);

			for (int c = 0; c < channels; c += 4) {
				float_4 width = params[ENC_WIDTH_PARAM].getValue();
				width += inputs[ENC_WIDTH_INPUT].getPolyVoltageSimd<float_4>(c) / 10 * 2;
				width = simd::fmax(width, 0.f);
				float_4 left = inputs[ENC_LEFT_INPUT].getVoltageSimd<float_4>(c);
				float_4 right = inputs[ENC_RIGHT_INPUT].getVoltageSimd<float_4>(c);
				float_4 mid = (left + right) / 2;
				float_4 sides = (left - right) / 2 * width;
				outputs[ENC_MID_OUTPUT].setVoltageSimd(mid, c);
				outputs[ENC_SIDES_OUTPUT].setVoltageSimd(sides, c);
			}
		}

		// Decoder
		{
			int channels = std::max(inputs[DEC_MID_INPUT].getChannels(), inputs[DEC_SIDES_INPUT].getChannels());
			outputs[DEC_LEFT_OUTPUT].setChannels(channels);
			outputs[DEC_RIGHT_OUTPUT].setChannels(channels);

			for (int c = 0; c < channels; c += 4) {
				float_4 width = params[DEC_WIDTH_PARAM].getValue();
				width += inputs[DEC_WIDTH_INPUT].getPolyVoltageSimd<float_4>(c) / 10 * 2;
				width = simd::fmax(width, 0.f);
				float_4 mid = inputs[DEC_MID_INPUT].getVoltageSimd<float_4>(c);
				float_4 sides = inputs[DEC_SIDES_INPUT].getVoltageSimd<float_4>(c);
				float_4 left = mid + sides * width;
				float_4 right = mid - sides * width;
				outputs[DEC_LEFT_OUTPUT].setVoltageSimd(left, c);
				outputs[DEC_RIGHT_OUTPUT].setVoltageSimd(right, c);
			}
		}
	}
};


struct MidSideWidget : ModuleWidget {
	MidSideWidget(MidSide* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/MidSide.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.285, 25.203)), module, MidSide::ENC_WIDTH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.285, 80.583)), module, MidSide::DEC_WIDTH_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.122, 25.142)), module, MidSide::ENC_WIDTH_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.285, 41.373)), module, MidSide::ENC_LEFT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.122, 41.373)), module, MidSide::ENC_RIGHT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.122, 80.603)), module, MidSide::DEC_WIDTH_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.285, 96.859)), module, MidSide::DEC_MID_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.122, 96.859)), module, MidSide::DEC_SIDES_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.285, 57.679)), module, MidSide::ENC_MID_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.122, 57.679)), module, MidSide::ENC_SIDES_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.285, 113.115)), module, MidSide::DEC_LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.122, 113.115)), module, MidSide::DEC_RIGHT_OUTPUT));
	}
};


Model* modelMidSide = createModel<MidSide, MidSideWidget>("MidSide");