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

	dsp::ClockDivider paramDivider;

	_8vert() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i = 0; i < 8; i++) {
			configParam(GAIN_PARAMS + i, -1.f, 1.f, 0.f, string::f("Row %d gain", i + 1), "%", 0, 100);
			configInput(IN_INPUTS + i, string::f("Row %d", i + 1));
			configOutput(OUT_OUTPUTS + i, string::f("Row %d", i + 1));
		}

		paramDivider.setDivision(2048);
	}

	void process(const ProcessArgs& args) override {
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

		if (paramDivider.process()) {
			refreshParamQuantities();
		}
	}

	/** Set the gain param units to either V or %, depending on whether a cable is connected. */
	void refreshParamQuantities() {
		bool normalized = true;

		for (int i = 0; i < 8; i++) {
			ParamQuantity* pq = paramQuantities[GAIN_PARAMS + i];
			if (!pq)
				continue;

			if (inputs[IN_INPUTS + i].isConnected())
				normalized = false;
			if (normalized) {
				pq->unit = "V";
				pq->displayMultiplier = 10.f;
			}
			else {
				pq->unit = "%";
				pq->displayMultiplier = 100.f;
			}
		}
	}
};


struct _8vertWidget : ModuleWidget {
	_8vertWidget(_8vert* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/8vert.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.351, 21.968)), module, _8vert::GAIN_PARAMS + 0));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.351, 34.982)), module, _8vert::GAIN_PARAMS + 1));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.351, 48.004)), module, _8vert::GAIN_PARAMS + 2));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.351, 61.026)), module, _8vert::GAIN_PARAMS + 3));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.351, 74.048)), module, _8vert::GAIN_PARAMS + 4));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.351, 87.07)), module, _8vert::GAIN_PARAMS + 5));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.351, 100.093)), module, _8vert::GAIN_PARAMS + 6));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.351, 113.115)), module, _8vert::GAIN_PARAMS + 7));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.331, 21.968)), module, _8vert::IN_INPUTS + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.331, 34.982)), module, _8vert::IN_INPUTS + 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.331, 48.004)), module, _8vert::IN_INPUTS + 2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.331, 61.026)), module, _8vert::IN_INPUTS + 3));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.331, 74.048)), module, _8vert::IN_INPUTS + 4));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.331, 87.07)), module, _8vert::IN_INPUTS + 5));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.331, 100.093)), module, _8vert::IN_INPUTS + 6));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.331, 113.115)), module, _8vert::IN_INPUTS + 7));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.371, 21.968)), module, _8vert::OUT_OUTPUTS + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.371, 34.982)), module, _8vert::OUT_OUTPUTS + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.371, 48.004)), module, _8vert::OUT_OUTPUTS + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.371, 61.026)), module, _8vert::OUT_OUTPUTS + 3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.371, 74.048)), module, _8vert::OUT_OUTPUTS + 4));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.371, 87.07)), module, _8vert::OUT_OUTPUTS + 5));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.371, 100.093)), module, _8vert::OUT_OUTPUTS + 6));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.371, 113.115)), module, _8vert::OUT_OUTPUTS + 7));
	}
};


Model* model_8vert = createModel<_8vert, _8vertWidget>("8vert");
