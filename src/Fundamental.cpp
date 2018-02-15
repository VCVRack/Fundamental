#include "Fundamental.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	p->slug = TOSTRING(SLUG);
	p->version = TOSTRING(VERSION);

	p->addModel(modelVCOWidget);
	p->addModel(modelVCO2Widget);
	p->addModel(modelVCFWidget);
	p->addModel(modelVCAWidget);
	p->addModel(modelLFOWidget);
	p->addModel(modelLFO2Widget);
	p->addModel(modelDelayWidget);
	p->addModel(modelADSRWidget);
	p->addModel(modelVCMixerWidget);
	p->addModel(model_8vertWidget);
	p->addModel(modelUnityWidget);
	p->addModel(modelMutesWidget);
	p->addModel(modelScopeWidget);
	p->addModel(modelSEQ3Widget);
	p->addModel(modelSequentialSwitch1Widget);
	p->addModel(modelSequentialSwitch2Widget);
}
