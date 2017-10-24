#include "Fundamental.hpp"


struct _8VERT : Module {
	enum ParamIds {
		NUM_PARAMS = 8
	};
	enum InputIds {
		NUM_INPUTS = 8
	};
	enum OutputIds {
		NUM_OUTPUTS = 8
	};
	enum LightIds {
		NUM_LIGHTS = 8
	};

	_8VERT() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;
};

void _8VERT::step() {
	float lastIn = 10.0;
	for (int i = 0; i < 8; i++) {
		lastIn = inputs[i].normalize(lastIn);
		float out = lastIn * params[i].value;
		outputs[i].value = out;
		lights[i].value = out / 10.0;
	}
}


_8VERTWidget::_8VERTWidget() {
	_8VERT *module = new _8VERT();
	setModule(module);
	box.size = Vec(8 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(SVG::load(assetPlugin(plugin, "res/8VERT.svg")));
	addChild(panel);

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 30, 365)));

	addParam(createParam<RoundSmallBlackKnob>(Vec(45, 48), module, 0, -1.0, 1.0, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(45, 86), module, 1, -1.0, 1.0, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(45, 125), module, 2, -1.0, 1.0, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(45, 163), module, 3, -1.0, 1.0, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(45, 202), module, 4, -1.0, 1.0, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(45, 240), module, 5, -1.0, 1.0, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(45, 278), module, 6, -1.0, 1.0, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(45, 317), module, 7, -1.0, 1.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(10, 50), module, 0));
	addInput(createInput<PJ301MPort>(Vec(10, 89), module, 1));
	addInput(createInput<PJ301MPort>(Vec(10, 127), module, 2));
	addInput(createInput<PJ301MPort>(Vec(10, 166), module, 3));
	addInput(createInput<PJ301MPort>(Vec(10, 204), module, 4));
	addInput(createInput<PJ301MPort>(Vec(10, 243), module, 5));
	addInput(createInput<PJ301MPort>(Vec(10, 281), module, 6));
	addInput(createInput<PJ301MPort>(Vec(10, 320), module, 7));

	addOutput(createOutput<PJ301MPort>(Vec(86, 50), module, 0));
	addOutput(createOutput<PJ301MPort>(Vec(86, 89), module, 1));
	addOutput(createOutput<PJ301MPort>(Vec(86, 127), module, 2));
	addOutput(createOutput<PJ301MPort>(Vec(86, 166), module, 3));
	addOutput(createOutput<PJ301MPort>(Vec(86, 204), module, 4));
	addOutput(createOutput<PJ301MPort>(Vec(86, 243), module, 5));
	addOutput(createOutput<PJ301MPort>(Vec(86, 281), module, 6));
	addOutput(createOutput<PJ301MPort>(Vec(86, 320), module, 7));

	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107, 49), &module->lights[0].value));
	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107, 88), &module->lights[1].value));
	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107, 126), &module->lights[2].value));
	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107, 165), &module->lights[3].value));
	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107, 203), &module->lights[4].value));
	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107, 242), &module->lights[5].value));
	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107, 280), &module->lights[6].value));
	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107, 319), &module->lights[7].value));
}
