#include "rack.hpp"


using namespace rack;


extern float sawTable[2048];
extern float triTable[2048];


////////////////////
// module widgets
////////////////////

struct VCOWidget : ModuleWidget {
	VCOWidget();
};

struct VCFWidget : ModuleWidget {
	VCFWidget();
};

struct VCAWidget : ModuleWidget {
	VCAWidget();
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

// struct MultWidget : ModuleWidget {
// 	MultWidget();
// };

struct ScopeWidget : ModuleWidget {
	ScopeWidget();
};

struct SEQ3Widget : ModuleWidget {
	SEQ3Widget();
};
