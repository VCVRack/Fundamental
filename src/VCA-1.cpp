#include "plugin.hpp"


struct VCA_1 : Module {
	enum ParamIds {
		LEVEL_PARAM,
		EXP_PARAM, // removed from panel in 2.0, still in context menu
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

	int lastChannels = 1;
	float lastGains[16] = {};

	VCA_1() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(LEVEL_PARAM, 0.0, 1.0, 1.0, "Level", "%", 0, 100);
		configSwitch(EXP_PARAM, 0.0, 1.0, 1.0, "Response mode", {"Exponential", "Linear"});
		configInput(CV_INPUT, "CV");
		configInput(IN_INPUT, "Channel");
		configOutput(OUT_OUTPUT, "Channel");
		configBypass(IN_INPUT, OUT_OUTPUT);
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max({1, inputs[IN_INPUT].getChannels(), inputs[CV_INPUT].getChannels()});
		float level = params[LEVEL_PARAM].getValue();

		for (int c = 0; c < channels; c++) {
			// Get input
			float in = inputs[IN_INPUT].getPolyVoltage(c);

			// Get gain
			float gain = level;
			if (inputs[CV_INPUT].isConnected()) {
				float cv = clamp(inputs[CV_INPUT].getPolyVoltage(c) / 10.f, 0.f, 1.f);
				if (int(params[EXP_PARAM].getValue()) == 0)
					cv = std::pow(cv, 4.f);
				gain *= cv;
			}

			// Apply gain
			in *= gain;
			lastGains[c] = gain;

			// Set output
			outputs[OUT_OUTPUT].setVoltage(in, c);
		}

		outputs[OUT_OUTPUT].setChannels(channels);
		lastChannels = channels;
	}
};


struct VCA_1VUKnob : SliderKnob {
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;

		VCA_1* module = dynamic_cast<VCA_1*>(this->module);

		Rect r = box.zeroPos();
		NVGcolor bgColor = nvgRGB(0x12, 0x12, 0x12);

		int channels = module ? module->lastChannels : 1;
		engine::ParamQuantity* pq = getParamQuantity();
		float value = pq ? pq->getValue() : 1.f;

		// Segment value
		if (value >= 0.005f) {
			nvgBeginPath(args.vg);
			nvgRect(args.vg,
			        r.pos.x,
			        r.pos.y + r.size.y * (1 - value),
			        r.size.x,
			        r.size.y * value);
			nvgFillColor(args.vg, color::mult(color::WHITE, 0.25));
			nvgFill(args.vg);
		}

		// Segment gain
		for (int c = 0; c < channels; c++) {
			float gain = module ? module->lastGains[c] : 1.f;
			if (gain >= 0.005f) {
				nvgBeginPath(args.vg);
				nvgRect(args.vg,
				        r.pos.x + r.size.x * c / channels,
				        r.pos.y + r.size.y * (1 - gain),
				        r.size.x / channels,
				        r.size.y * gain);
				nvgFillColor(args.vg, SCHEME_YELLOW);
				nvgFill(args.vg);
			}
		}

		// Invisible separators
		const int segs = 25;
		nvgFillColor(args.vg, bgColor);
		for (int i = 1; i < segs; i++) {
			nvgBeginPath(args.vg);
			nvgRect(args.vg,
			        r.pos.x - 1.0,
			        r.pos.y + r.size.y * i / segs,
			        r.size.x + 2.0,
			        1.0);
			nvgFill(args.vg);
		}
	}
};


struct VCA_1Display : LedDisplay {
};


struct VCA_1Widget : ModuleWidget {
	VCA_1Widget(VCA_1* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/VCA-1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 80.603)), module, VCA_1::CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 96.859)), module, VCA_1::IN_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 113.115)), module, VCA_1::OUT_OUTPUT));

		VCA_1Display* display = createWidget<VCA_1Display>(mm2px(Vec(0.0, 13.039)));
		display->box.size = mm2px(Vec(15.263, 55.88));
		addChild(display);

		VCA_1VUKnob* knob = createParam<VCA_1VUKnob>(mm2px(Vec(2.253, 15.931)), module, VCA_1::LEVEL_PARAM);
		knob->box.size = mm2px(Vec(10.734, 50.253));
		addChild(knob);
	}

	void appendContextMenu(Menu* menu) override {
		VCA_1* module = dynamic_cast<VCA_1*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolMenuItem("Exponential response", "",
			[=]() {return module->params[VCA_1::EXP_PARAM].getValue() == 0.f;},
			[=](bool value) {module->params[VCA_1::EXP_PARAM].setValue(!value);}
		));
	}
};


Model* modelVCA_1 = createModel<VCA_1, VCA_1Widget>("VCA-1");
