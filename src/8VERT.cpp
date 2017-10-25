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

	addParam(createParam<RoundSmallBlackKnob>(Vec(45.308, 47.753), module, 0, -1.0, 1.0, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(45.308, 86.198), module, 1, -1.0, 1.0, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(45.308, 124.639), module, 2, -1.0, 1.0, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(45.308, 163.084), module, 3, -1.0, 1.0, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(45.308, 201.529), module, 4, -1.0, 1.0, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(45.308, 239.974), module, 5, -1.0, 1.0, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(45.308, 278.415), module, 6, -1.0, 1.0, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(45.308, 316.86), module, 7, -1.0, 1.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(9.507, 50.397), module, 0));
	addInput(createInput<PJ301MPort>(Vec(9.507, 88.842), module, 1));
	addInput(createInput<PJ301MPort>(Vec(9.507, 127.283), module, 2));
	addInput(createInput<PJ301MPort>(Vec(9.507, 165.728), module, 3));
	addInput(createInput<PJ301MPort>(Vec(9.507, 204.173), module, 4));
	addInput(createInput<PJ301MPort>(Vec(9.507, 242.614), module, 5));
	addInput(createInput<PJ301MPort>(Vec(9.507, 281.059), module, 6));
	addInput(createInput<PJ301MPort>(Vec(9.507, 319.504), module, 7));

	addOutput(createOutput<PJ301MPort>(Vec(86.393, 50.397), module, 0));
	addOutput(createOutput<PJ301MPort>(Vec(86.393, 88.842), module, 1));
	addOutput(createOutput<PJ301MPort>(Vec(86.393, 127.283), module, 2));
	addOutput(createOutput<PJ301MPort>(Vec(86.393, 165.728), module, 3));
	addOutput(createOutput<PJ301MPort>(Vec(86.393, 204.173), module, 4));
	addOutput(createOutput<PJ301MPort>(Vec(86.393, 242.614), module, 5));
	addOutput(createOutput<PJ301MPort>(Vec(86.393, 281.059), module, 6));
	addOutput(createOutput<PJ301MPort>(Vec(86.393, 319.504), module, 7));

	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107.702, 50.414), &module->lights[0].value));
	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107.702, 88.859), &module->lights[1].value));
	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107.702, 127.304), &module->lights[2].value));
	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107.702, 165.745), &module->lights[3].value));
	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107.702, 204.19), &module->lights[4].value));
	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107.702, 242.635), &module->lights[5].value));
	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107.702, 281.076), &module->lights[6].value));
	addChild(createValueLight<TinyLight<GreenRedPolarityLight>>(Vec(107.702, 319.521), &module->lights[7].value));
}
