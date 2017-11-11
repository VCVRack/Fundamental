#include "Fundamental.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	p->slug = "Fundamental";
#ifdef VERSION
	p->version = TOSTRING(VERSION);
#endif

	p->addModel(createModel<VCOWidget>("Fundamental", "VCO", "VCO-1"));
	p->addModel(createModel<VCO2Widget>("Fundamental", "VCO2", "VCO-2"));
	p->addModel(createModel<VCFWidget>("Fundamental", "VCF", "VCF"));
	p->addModel(createModel<VCAWidget>("Fundamental", "VCA", "VCA"));
	p->addModel(createModel<LFOWidget>("Fundamental", "LFO", "LFO-1"));
	p->addModel(createModel<LFO2Widget>("Fundamental", "LFO2", "LFO-2"));
	p->addModel(createModel<DelayWidget>("Fundamental", "Delay", "Delay"));
	p->addModel(createModel<ADSRWidget>("Fundamental", "ADSR", "ADSR"));
	p->addModel(createModel<VCMixerWidget>("Fundamental", "VCMixer", "VC Mixer"));
	p->addModel(createModel<_8vertWidget>("Fundamental", "8vert", "8vert"));
	p->addModel(createModel<UnityWidget>("Fundamental", "Unity", "Unity"));
	p->addModel(createModel<MutesWidget>("Fundamental", "Mutes", "Mutes"));
	p->addModel(createModel<ScopeWidget>("Fundamental", "Scope", "Scope"));
	p->addModel(createModel<SEQ3Widget>("Fundamental", "SEQ3", "SEQ-3"));
}
