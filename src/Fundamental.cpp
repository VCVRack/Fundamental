#include "Fundamental.hpp"
#include <math.h>
#include "dsp.hpp"


struct FundamentalPlugin : Plugin {
	FundamentalPlugin() {
		slug = "Fundamental";
		name = "Fundamental";
		createModel<VCOWidget>(this, "VCO", "VCO");
		createModel<VCFWidget>(this, "VCF", "VCF");
		createModel<VCAWidget>(this, "VCA", "VCA");
		createModel<DelayWidget>(this, "Delay", "Delay");
		createModel<ADSRWidget>(this, "ADSR", "ADSR");
		createModel<VCMixerWidget>(this, "VCMixer", "VC Mixer");
		// createModel<MultWidget>(this, "Mult", "Mult");
		createModel<ScopeWidget>(this, "Scope", "Scope");
		createModel<SEQ3Widget>(this, "SEQ3", "SEQ-3");
	}
};


Plugin *init() {
	return new FundamentalPlugin();
}
