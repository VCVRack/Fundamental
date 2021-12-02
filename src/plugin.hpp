#include <rack.hpp>


using namespace rack;


extern Plugin* pluginInstance;

extern Model* modelVCO;
extern Model* modelVCO2;
extern Model* modelVCF;
extern Model* modelVCA_1;
extern Model* modelVCA;
extern Model* modelLFO;
extern Model* modelLFO2;
extern Model* modelDelay;
extern Model* modelADSR;
extern Model* modelMixer;
extern Model* modelVCMixer;
extern Model* model_8vert;
extern Model* modelUnity;
extern Model* modelMutes;
extern Model* modelPulses;
extern Model* modelScope;
extern Model* modelSEQ3;
extern Model* modelSequentialSwitch1;
extern Model* modelSequentialSwitch2;
extern Model* modelOctave;
extern Model* modelQuantizer;
extern Model* modelSplit;
extern Model* modelMerge;
extern Model* modelSum;
extern Model* modelViz;
extern Model* modelMidSide;
extern Model* modelNoise;
extern Model* modelRandom;


struct DigitalDisplay : Widget {
	std::string fontPath;
	std::string bgText;
	std::string text;
	float fontSize;
	NVGcolor bgColor = nvgRGB(0x46,0x46, 0x46);
	NVGcolor fgColor = SCHEME_YELLOW;
	Vec textPos;

	void prepareFont(const DrawArgs& args) {
		// Get font
		std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
		if (!font)
			return;
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, fontSize);
		nvgTextLetterSpacing(args.vg, 0.0);
		nvgTextAlign(args.vg, NVG_ALIGN_RIGHT);
	}

	void draw(const DrawArgs& args) override {
		// Background
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2);
		nvgFillColor(args.vg, nvgRGB(0x19, 0x19, 0x19));
		nvgFill(args.vg);

		prepareFont(args);

		// Background text
		nvgFillColor(args.vg, bgColor);
		nvgText(args.vg, textPos.x, textPos.y, bgText.c_str(), NULL);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			prepareFont(args);

			// Foreground text
			nvgFillColor(args.vg, fgColor);
			nvgText(args.vg, textPos.x, textPos.y, text.c_str(), NULL);
		}
		Widget::drawLayer(args, layer);
	}
};


struct ChannelDisplay : DigitalDisplay {
	ChannelDisplay() {
		fontPath = asset::system("res/fonts/DSEG7ClassicMini-BoldItalic.ttf");
		textPos = Vec(22, 20);
		bgText = "18";
		fontSize = 16;
	}
};


template <typename TBase = GrayModuleLightWidget>
struct YellowRedLight : TBase {
	YellowRedLight() {
		this->addBaseColor(SCHEME_YELLOW);
		this->addBaseColor(SCHEME_RED);
	}
};
