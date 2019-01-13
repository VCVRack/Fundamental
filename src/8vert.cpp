#include "Fundamental.hpp"


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
		ENUMS(OUT_LIGHTS, 8*2),
		NUM_LIGHTS
	};

	_8vert() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		params[GAIN_PARAMS + 0].config(-1.f, 1.f, 0.f);
		params[GAIN_PARAMS + 1].config(-1.f, 1.f, 0.f);
		params[GAIN_PARAMS + 2].config(-1.f, 1.f, 0.f);
		params[GAIN_PARAMS + 3].config(-1.f, 1.f, 0.f);
		params[GAIN_PARAMS + 4].config(-1.f, 1.f, 0.f);
		params[GAIN_PARAMS + 5].config(-1.f, 1.f, 0.f);
		params[GAIN_PARAMS + 6].config(-1.f, 1.f, 0.f);
		params[GAIN_PARAMS + 7].config(-1.f, 1.f, 0.f);
	}

	void step() override {
		float lastIn = 10.f;
		for (int i = 0; i < 8; i++) {
			lastIn = inputs[i].normalize(lastIn);
			float out = lastIn * params[i].value;
			outputs[i].value = out;
			lights[2*i + 0].setBrightnessSmooth(std::max(0.f, out / 5.f));
			lights[2*i + 1].setBrightnessSmooth(std::max(0.f, -out / 5.f));
		}
	}
};


struct _8vertWidget : ModuleWidget {
	_8vertWidget(_8vert *module);
};

_8vertWidget::_8vertWidget(_8vert *module) : ModuleWidget(module) {
	setPanel(SVG::load(asset::plugin(plugin, "res/8vert.svg")));

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

	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 50.414), module, _8vert::OUT_LIGHTS + 0*2));
	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 88.859), module, _8vert::OUT_LIGHTS + 1*2));
	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 127.304), module, _8vert::OUT_LIGHTS + 2*2));
	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 165.745), module, _8vert::OUT_LIGHTS + 3*2));
	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 204.19), module, _8vert::OUT_LIGHTS + 4*2));
	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 242.635), module, _8vert::OUT_LIGHTS + 5*2));
	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 281.076), module, _8vert::OUT_LIGHTS + 6*2));
	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 319.521), module, _8vert::OUT_LIGHTS + 7*2));
}


Model *model_8vert = createModel<_8vert, _8vertWidget>("8vert");
