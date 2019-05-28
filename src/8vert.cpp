#include "plugin.hpp"


struct _8vert : Module {
	enum ParamIds {
		ENUMS(GAIN_PARAMS, 8),
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(IN_INPUTS, 8),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUT_OUTPUTS, 8),
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	_8vert() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i = 0; i < 8; i++) {
			configParam(GAIN_PARAMS + i, -1.f, 1.f, 0.f, string::f("Ch %d gain", i + 1), "%", 0, 100);
		}
	}

	void process(const ProcessArgs &args) override {
		float in[16] = {10.f};
		int channels = 1;

		for (int i = 0; i < 8; i++) {
			// Get input
			if (inputs[IN_INPUTS + i].isConnected()) {
				channels = inputs[IN_INPUTS + i].getChannels();
				inputs[IN_INPUTS + i].readVoltages(in);
			}

			if (outputs[OUT_OUTPUTS + i].isConnected()) {
				// Apply gain
				float out[16];
				float gain = params[GAIN_PARAMS + i].getValue();
				for (int c = 0; c < channels; c++) {
					out[c] = gain * in[c];
				}

				// Set output
				outputs[OUT_OUTPUTS + i].setChannels(channels);
				outputs[OUT_OUTPUTS + i].writeVoltages(out);
			}
		}
	}
};


struct _8vertWidget : ModuleWidget {
	_8vertWidget(_8vert *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/8vert.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

		addParam(createParam<RoundBlackKnob>(Vec(45.308, 47.753), module, _8vert::GAIN_PARAMS + 0));
		addParam(createParam<RoundBlackKnob>(Vec(45.308, 86.198), module, _8vert::GAIN_PARAMS + 1));
		addParam(createParam<RoundBlackKnob>(Vec(45.308, 124.639), module, _8vert::GAIN_PARAMS + 2));
		addParam(createParam<RoundBlackKnob>(Vec(45.308, 163.084), module, _8vert::GAIN_PARAMS + 3));
		addParam(createParam<RoundBlackKnob>(Vec(45.308, 201.529), module, _8vert::GAIN_PARAMS + 4));
		addParam(createParam<RoundBlackKnob>(Vec(45.308, 239.974), module, _8vert::GAIN_PARAMS + 5));
		addParam(createParam<RoundBlackKnob>(Vec(45.308, 278.415), module, _8vert::GAIN_PARAMS + 6));
		addParam(createParam<RoundBlackKnob>(Vec(45.308, 316.86), module, _8vert::GAIN_PARAMS + 7));

		addInput(createInput<PJ301MPort>(Vec(9.507, 50.397), module, _8vert::IN_INPUTS + 0));
		addInput(createInput<PJ301MPort>(Vec(9.507, 88.842), module, _8vert::IN_INPUTS + 1));
		addInput(createInput<PJ301MPort>(Vec(9.507, 127.283), module, _8vert::IN_INPUTS + 2));
		addInput(createInput<PJ301MPort>(Vec(9.507, 165.728), module, _8vert::IN_INPUTS + 3));
		addInput(createInput<PJ301MPort>(Vec(9.507, 204.173), module, _8vert::IN_INPUTS + 4));
		addInput(createInput<PJ301MPort>(Vec(9.507, 242.614), module, _8vert::IN_INPUTS + 5));
		addInput(createInput<PJ301MPort>(Vec(9.507, 281.059), module, _8vert::IN_INPUTS + 6));
		addInput(createInput<PJ301MPort>(Vec(9.507, 319.504), module, _8vert::IN_INPUTS + 7));

		addOutput(createOutput<PJ301MPort>(Vec(86.393, 50.397), module, _8vert::OUT_OUTPUTS + 0));
		addOutput(createOutput<PJ301MPort>(Vec(86.393, 88.842), module, _8vert::OUT_OUTPUTS + 1));
		addOutput(createOutput<PJ301MPort>(Vec(86.393, 127.283), module, _8vert::OUT_OUTPUTS + 2));
		addOutput(createOutput<PJ301MPort>(Vec(86.393, 165.728), module, _8vert::OUT_OUTPUTS + 3));
		addOutput(createOutput<PJ301MPort>(Vec(86.393, 204.173), module, _8vert::OUT_OUTPUTS + 4));
		addOutput(createOutput<PJ301MPort>(Vec(86.393, 242.614), module, _8vert::OUT_OUTPUTS + 5));
		addOutput(createOutput<PJ301MPort>(Vec(86.393, 281.059), module, _8vert::OUT_OUTPUTS + 6));
		addOutput(createOutput<PJ301MPort>(Vec(86.393, 319.504), module, _8vert::OUT_OUTPUTS + 7));
	}
};


Model *model_8vert = createModel<_8vert, _8vertWidget>("8vert");
