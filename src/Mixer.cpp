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

	Mixer() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Level", "%", 0, 100);
		for (int i = 0; i < 6; i++)
			configInput(IN_INPUTS + i, string::f("Channel %d", i + 1));
		configOutput(OUT_OUTPUT, "Mix");
	}

	void process(const ProcessArgs& args) override {
		// Get number of channels
		int channels = 1;
		for (int i = 0; i < 6; i++)
			channels = std::max(channels, inputs[IN_INPUTS + i].getChannels());

		float gain = params[LEVEL_PARAM].getValue();

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
};


Model* modelMixer = createModel<Mixer, MixerWidget>("Mixer");