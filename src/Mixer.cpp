#include "plugin.hpp"


using simd::float_4;


struct Mixer : Module {
	enum ParamId {
		LEVEL_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(IN_INPUTS, 6),
		INPUTS_LEN
	};
	enum OutputId {
		OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	bool invert = false;
	bool average = false;

	Mixer() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Level", "%", 0, 100);
		for (int i = 0; i < 6; i++)
			configInput(IN_INPUTS + i, string::f("Channel %d", i + 1));
		configOutput(OUT_OUTPUT, "Mix");
	}

	void process(const ProcessArgs& args) override {
		// Get number of channels and number of connected inputs
		int channels = 1;
		int connected = 0;
		for (int i = 0; i < 6; i++) {
			channels = std::max(channels, inputs[IN_INPUTS + i].getChannels());
			if (inputs[IN_INPUTS + i].isConnected())
				connected++;
		}

		float gain = params[LEVEL_PARAM].getValue();
		// Invert
		if (invert) {
			gain *= -1.f;
		}
		// Average
		if (average) {
			gain /= std::max(1, connected);
		}

		// Iterate polyphonic channels
		for (int c = 0; c < channels; c += 4) {
			float_4 out = 0.f;
			// Mix input
			for (int i = 0; i < 6; i++) {
				out += inputs[IN_INPUTS + i].getVoltageSimd<float_4>(c);
			}

			// Apply gain
			out *= gain;

			// Set output
			outputs[OUT_OUTPUT].setVoltageSimd(out, c);
		}

		outputs[OUT_OUTPUT].setChannels(channels);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		// average
		json_object_set_new(rootJ, "average", json_boolean(average));
		// invert
		json_object_set_new(rootJ, "invert", json_boolean(invert));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// average
		json_t* averageJ = json_object_get(rootJ, "average");
		if (averageJ)
			average = json_boolean_value(averageJ);
		// invert
		json_t* invertJ = json_object_get(rootJ, "invert");
		if (invertJ)
			invert = json_boolean_value(invertJ);
	}
};


struct MixerWidget : ModuleWidget {
	MixerWidget(Mixer* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Mixer.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 24.723)), module, Mixer::LEVEL_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 46.059)), module, Mixer::IN_INPUTS + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 56.219)), module, Mixer::IN_INPUTS + 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 66.379)), module, Mixer::IN_INPUTS + 2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 76.539)), module, Mixer::IN_INPUTS + 3));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 86.699)), module, Mixer::IN_INPUTS + 4));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 96.859)), module, Mixer::IN_INPUTS + 5));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 113.115)), module, Mixer::OUT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Mixer* module = dynamic_cast<Mixer*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolPtrMenuItem("Invert output", "", &module->invert));
		menu->addChild(createBoolPtrMenuItem("Average voltages", "", &module->average));
	}
};


Model* modelMixer = createModel<Mixer, MixerWidget>("Mixer");