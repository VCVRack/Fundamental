#include "plugin.hpp"


Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;

	p->addModel(modelVCO);
	p->addModel(modelVCO2);
	p->addModel(modelVCF);
	p->addModel(modelVCA_1);
	p->addModel(modelVCA);
	p->addModel(modelLFO);
	p->addModel(modelLFO2);
	p->addModel(modelDelay);
	p->addModel(modelADSR);
	p->addModel(modelMixer);
	p->addModel(modelVCMixer);
	p->addModel(model_8vert);
	p->addModel(modelUnity);
	p->addModel(modelMutes);
	p->addModel(modelPulses);
	p->addModel(modelScope);
	p->addModel(modelSEQ3);
	p->addModel(modelSequentialSwitch1);
	p->addModel(modelSequentialSwitch2);
	p->addModel(modelOctave);
	p->addModel(modelQuantizer);
	p->addModel(modelSplit);
	p->addModel(modelMerge);
	p->addModel(modelSum);
	p->addModel(modelViz);
	p->addModel(modelMidSide);
	p->addModel(modelNoise);
	p->addModel(modelRandom);
}
