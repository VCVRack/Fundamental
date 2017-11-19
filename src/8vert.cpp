#include "Fundamental.hpp"


struct _8vert : Module {
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
		NUM_LIGHTS = 16
	};

	_8vert() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;
};

void _8vert::step() {
	float lastIn = 10.0;
	for (int i = 0; i < 8; i++) {
		lastIn = inputs[i].normalize(lastIn);
		float out = lastIn * params[i].value;
		outputs[i].value = out;
		lights[2*i + 0].setBrightnessSmooth(fmaxf(0.0, out / 5.0));
		lights[2*i + 1].setBrightnessSmooth(fmaxf(0.0, -out / 5.0));
	}
}


_8vertWidget::_8vertWidget() {
	_8vert *module = new _8vert();
	setModule(module);
	box.size = Vec(8 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
	setPanel(SVG::load(assetPlugin(plugin, "res/8vert.svg")));

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

	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 50.414), module, 0));
	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 88.859), module, 2));
	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 127.304), module, 4));
	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 165.745), module, 6));
	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 204.19), module, 8));
	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 242.635), module, 10));
	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 281.076), module, 12));
	addChild(createLight<TinyLight<GreenRedLight>>(Vec(107.702, 319.521), module, 14));
}
