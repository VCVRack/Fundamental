#include "Fundamental.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	plugin->slug = "Fundamental";
	plugin->name = "Fundamental";
	plugin->homepageUrl = "https://github.com/VCVRack/Fundamental";
	createModel<VCOWidget>(plugin, "VCO", "VCO-1");
	createModel<VCO2Widget>(plugin, "VCO2", "VCO-2");
	createModel<VCFWidget>(plugin, "VCF", "VCF");
	createModel<VCAWidget>(plugin, "VCA", "VCA");
	createModel<LFOWidget>(plugin, "LFO", "LFO-1");
	createModel<LFO2Widget>(plugin, "LFO2", "LFO-2");
	createModel<DelayWidget>(plugin, "Delay", "Delay");
	createModel<ADSRWidget>(plugin, "ADSR", "ADSR");
	createModel<VCMixerWidget>(plugin, "VCMixer", "VC Mixer");
	createModel<ScopeWidget>(plugin, "Scope", "Scope");
	createModel<SEQ3Widget>(plugin, "SEQ3", "SEQ-3");
}
