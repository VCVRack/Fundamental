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
	p->addModel(modelCVMix);
	p->addModel(modelFade);
	p->addModel(modelLogic);
	p->addModel(modelCompare);
	p->addModel(modelGates);
	p->addModel(modelProcess);
	p->addModel(modelMult);
	p->addModel(modelRescale);
	p->addModel(modelRandomValues);
	p->addModel(modelPush);
	p->addModel(modelSHASR);
}


MenuItem* createRangeItem(std::string label, float* gain, float* offset) {
	struct Range {
		float gain;
		float offset;

		bool operator==(const Range& other) const {
			return gain == other.gain && offset == other.offset;
		}
	};

	static const std::vector<Range> ranges = {
		{10.f, 0.f},
		{5.f, 0.f},
		{1.f, 0.f},
		{20.f, -10.f},
		{10.f, -5.f},
		{2.f, -1.f},
	};
	static std::vector<std::string> labels;
	if (labels.empty()) {
		for (const Range& range : ranges) {
			labels.push_back(string::f("%gV to %gV", range.offset, range.offset + range.gain));
		}
	}

	return createIndexSubmenuItem(label, labels,
		[=]() {
			auto it = std::find(ranges.begin(), ranges.end(), Range{*gain, *offset});
			return std::distance(ranges.begin(), it);
		},
		[=](int i) {
			*gain = ranges[i].gain;
			*offset = ranges[i].offset;
		}
	);
}