#include "rack.hpp"


using namespace rack;


extern Plugin *plugin;

////////////////////
// module widgets
////////////////////

struct VCOWidget : ModuleWidget {
	VCOWidget();
};

struct VCO2Widget : ModuleWidget {
	VCO2Widget();
};

struct VCFWidget : ModuleWidget {
	VCFWidget();
};

struct VCAWidget : ModuleWidget {
	VCAWidget();
};

struct LFOWidget : ModuleWidget {
	LFOWidget();
};

struct LFO2Widget : ModuleWidget {
	LFO2Widget();
};

struct DelayWidget : ModuleWidget {
	DelayWidget();
};

struct ADSRWidget : ModuleWidget {
	ADSRWidget();
};

struct VCMixerWidget : ModuleWidget {
	VCMixerWidget();
};

struct _8VERTWidget : ModuleWidget {
	_8VERTWidget();
};

struct ScopeWidget : ModuleWidget {
	ScopeWidget();
};

struct SEQ3Widget : ModuleWidget {
	SEQ3Widget();
	Menu *createContextMenu() override;
};
