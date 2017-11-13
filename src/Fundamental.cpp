#include "Fundamental.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	p->slug = "Fundamental";
#ifdef VERSION
	p->version = TOSTRING(VERSION);
#endif
	p->website = "https://github.com/VCVRack/Fundamental";

	p->addModel(createModel<VCOWidget>("Fundamental", "VCO", "VCO-1", OSCILLATOR_TAG));
	p->addModel(createModel<VCO2Widget>("Fundamental", "VCO2", "VCO-2", OSCILLATOR_TAG));
	p->addModel(createModel<VCFWidget>("Fundamental", "VCF", "VCF", FILTER_TAG));
	p->addModel(createModel<VCAWidget>("Fundamental", "VCA", "VCA", AMPLIFIER_TAG));
	p->addModel(createModel<LFOWidget>("Fundamental", "LFO", "LFO-1", LFO_TAG));
	p->addModel(createModel<LFO2Widget>("Fundamental", "LFO2", "LFO-2", LFO_TAG));
	p->addModel(createModel<DelayWidget>("Fundamental", "Delay", "Delay", DELAY_TAG));
	p->addModel(createModel<ADSRWidget>("Fundamental", "ADSR", "ADSR", ENVELOPE_GENERATOR_TAG));
	p->addModel(createModel<VCMixerWidget>("Fundamental", "VCMixer", "VC Mixer", MIXER_TAG, AMPLIFIER_TAG));
	p->addModel(createModel<_8vertWidget>("Fundamental", "8vert", "8vert", ATTENUATOR_TAG));
	p->addModel(createModel<UnityWidget>("Fundamental", "Unity", "Unity", UTILITY_TAG));
	p->addModel(createModel<MutesWidget>("Fundamental", "Mutes", "Mutes", SWITCH_TAG));
	p->addModel(createModel<ScopeWidget>("Fundamental", "Scope", "Scope", VISUAL_TAG));
	p->addModel(createModel<SEQ3Widget>("Fundamental", "SEQ3", "SEQ-3", SEQUENCER_TAG));
}
