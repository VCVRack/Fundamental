#include "Fundamental.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	plugin->slug = "Fundamental";
	plugin->name = "Fundamental";
	createModel<VCOWidget>(plugin, "VCO", "VCO");
	createModel<VCFWidget>(plugin, "VCF", "VCF");
	createModel<VCAWidget>(plugin, "VCA", "VCA");
	createModel<DelayWidget>(plugin, "Delay", "Delay");
	createModel<ADSRWidget>(plugin, "ADSR", "ADSR");
	createModel<VCMixerWidget>(plugin, "VCMixer", "VC Mixer");
	createModel<ScopeWidget>(plugin, "Scope", "Scope");
	createModel<SEQ3Widget>(plugin, "SEQ3", "SEQ-3");
}
