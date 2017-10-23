#include "Fundamental.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	p->slug = "Fundamental";
#ifdef VERSION
	p->version = TOSTRING(VERSION);
#endif
	p->addModel(createModel<VCOWidget>("Fundamental", "Fundamental", "VCO", "VCO-1"));
	p->addModel(createModel<VCO2Widget>("Fundamental", "Fundamental", "VCO2", "VCO-2"));
	p->addModel(createModel<VCFWidget>("Fundamental", "Fundamental", "VCF", "VCF"));
	p->addModel(createModel<VCAWidget>("Fundamental", "Fundamental", "VCA", "VCA"));
	p->addModel(createModel<LFOWidget>("Fundamental", "Fundamental", "LFO", "LFO-1"));
	p->addModel(createModel<LFO2Widget>("Fundamental", "Fundamental", "LFO2", "LFO-2"));
	p->addModel(createModel<DelayWidget>("Fundamental", "Fundamental", "Delay", "Delay"));
	p->addModel(createModel<ADSRWidget>("Fundamental", "Fundamental", "ADSR", "ADSR"));
	p->addModel(createModel<VCMixerWidget>("Fundamental", "Fundamental", "VCMixer", "VC Mixer"));
	p->addModel(createModel<ScopeWidget>("Fundamental", "Fundamental", "Scope", "Scope"));
	p->addModel(createModel<SEQ3Widget>("Fundamental", "Fundamental", "SEQ3", "SEQ-3"));
}
