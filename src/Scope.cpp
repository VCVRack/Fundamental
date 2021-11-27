#include <string.h>
#include "plugin.hpp"


static const int BUFFER_SIZE = 256;


struct Scope : Module {
	enum ParamIds {
		X_SCALE_PARAM,
		X_POS_PARAM,
		Y_SCALE_PARAM,
		Y_POS_PARAM,
		TIME_PARAM,
		LISSAJOUS_PARAM,
		TRIG_PARAM,
		EXTERNAL_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		X_INPUT,
		Y_INPUT,
		TRIG_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		// new in 2.0
		X_OUTPUT,
		Y_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		LISSAJOUS_LIGHT,
		EXTERNAL_LIGHT,
		NUM_LIGHTS
	};

	struct Point {
		float minX[16] = {};
		float maxX[16] = {};
		float minY[16] = {};
		float maxY[16] = {};

		Point() {
			for (int c = 0; c < 16; c++) {
				minX[c] = INFINITY;
				maxX[c] = -INFINITY;
				minY[c] = INFINITY;
				maxY[c] = -INFINITY;
			}
		}
	};
	Point pointBuffer[BUFFER_SIZE];
	int channelsX = 0;
	int channelsY = 0;
	int bufferIndex = 0;
	int frameIndex = 0;
	Point currentPoint;

	dsp::SchmittTrigger triggers[16];

	Scope() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(X_SCALE_PARAM, 0.f, 8.f, 0.f, "Gain 1", " V/screen", 1 / 2.f, 20);
		getParamQuantity(X_SCALE_PARAM)->snapEnabled = true;
		configParam(X_POS_PARAM, -10.f, 10.f, 0.f, "Offset 1", " V");
		configParam(Y_SCALE_PARAM, 0.f, 8.f, 0.f, "Gain 2", " V/screen", 1 / 2.f, 20);
		getParamQuantity(Y_SCALE_PARAM)->snapEnabled = true;
		configParam(Y_POS_PARAM, -10.f, 10.f, 0.f, "Offset 2", " V");
		const float maxTime = -std::log2(5e1f);
		const float minTime = -std::log2(5e-3f);
		const float defaultTime = -std::log2(5e-1f);
		configParam(TIME_PARAM, maxTime, minTime, defaultTime, "Time", " ms/screen", 1 / 2.f, 1000);
		configSwitch(LISSAJOUS_PARAM, 0.f, 1.f, 0.f, "Scope mode", {"1 & 2", "1 x 2"});
		configParam(TRIG_PARAM, -10.f, 10.f, 0.f, "Trigger threshold", " V");
		configSwitch(EXTERNAL_PARAM, 0.f, 1.f, 1.f, "Trigger mode", {"Internal", "External"});

		configInput(X_INPUT, "Ch 1");
		configInput(Y_INPUT, "Ch 2");
		configInput(TRIG_INPUT, "External trigger");

		configOutput(X_OUTPUT, "Ch 1");
		configOutput(Y_OUTPUT, "Ch 2");
	}

	void onReset() override {
		for (int i = 0; i < BUFFER_SIZE; i++) {
			pointBuffer[i] = Point();
		}
	}

	void process(const ProcessArgs& args) override {
		bool lissajous = params[LISSAJOUS_PARAM].getValue() > 0.f;
		lights[LISSAJOUS_LIGHT].setBrightness(lissajous);

		bool external = params[EXTERNAL_PARAM].getValue() > 0.f;
		lights[EXTERNAL_LIGHT].setBrightness(!external);

		// Compute time
		float deltaTime = std::pow(2.f, -params[TIME_PARAM].getValue()) / BUFFER_SIZE;
		int frameCount = (int) std::ceil(deltaTime * args.sampleRate);

		// Set channels
		int channelsX = inputs[X_INPUT].getChannels();
		if (channelsX != this->channelsX) {
			// TODO
			// std::memset(bufferX, 0, sizeof(bufferX));
			this->channelsX = channelsX;
		}

		int channelsY = inputs[Y_INPUT].getChannels();
		if (channelsY != this->channelsY) {
			// TODO
			// std::memset(bufferY, 0, sizeof(bufferY));
			this->channelsY = channelsY;
		}

		// Copy inputs to outputs
		outputs[X_OUTPUT].setChannels(channelsX);
		outputs[X_OUTPUT].writeVoltages(inputs[X_INPUT].getVoltages());
		outputs[Y_OUTPUT].setChannels(channelsY);
		outputs[Y_OUTPUT].writeVoltages(inputs[Y_INPUT].getVoltages());

		// Add point to buffer if recording
		if (bufferIndex < BUFFER_SIZE) {
			// Get input
			for (int c = 0; c < channelsX; c++) {
				float x = inputs[X_INPUT].getVoltage(c);
				currentPoint.minX[c] = std::min(currentPoint.minX[c], x);
				currentPoint.maxX[c] = std::max(currentPoint.maxX[c], x);
			}
			for (int c = 0; c < channelsY; c++) {
				float y = inputs[Y_INPUT].getVoltage(c);
				currentPoint.minY[c] = std::min(currentPoint.minY[c], y);
				currentPoint.maxY[c] = std::max(currentPoint.maxY[c], y);
			}

			if (++frameIndex >= frameCount) {
				frameIndex = 0;
				// Push current point
				pointBuffer[bufferIndex] = currentPoint;
				// Reset current point
				currentPoint = Point();
				bufferIndex++;
			}
		}

		// Wait for trigger if no longer recording
		if (bufferIndex >= BUFFER_SIZE) {
			auto trigger = [&]() {
				for (int c = 0; c < 16; c++) {
					triggers[c].reset();
				}
				bufferIndex = 0;
				frameIndex = 0;
			};

			// Trigger immediately if external but nothing plugged in, or in Lissajous mode
			if (lissajous || (external && !inputs[TRIG_INPUT].isConnected())) {
				trigger();
				return;
			}

			// Reset if triggered
			float trigThreshold = params[TRIG_PARAM].getValue();
			Input& trigInput = external ? inputs[TRIG_INPUT] : inputs[X_INPUT];

			// This may be 0
			int trigChannels = trigInput.getChannels();
			for (int c = 0; c < trigChannels; c++) {
				float trigVoltage = trigInput.getVoltage(c);
				if (triggers[c].process(rescale(trigVoltage, trigThreshold, trigThreshold + 0.001f, 0.f, 1.f))) {
					trigger();
					return;
				}
			}
		}
	}

	bool isLissajous() {
		return params[LISSAJOUS_PARAM].getValue() > 0.f;
	}

	void dataFromJson(json_t* rootJ) override {
		// In <2.0, lissajous and external were class variables
		json_t* lissajousJ = json_object_get(rootJ, "lissajous");
		if (lissajousJ) {
			if (json_integer_value(lissajousJ))
				params[LISSAJOUS_PARAM].setValue(1.f);
		}

		json_t* externalJ = json_object_get(rootJ, "external");
		if (externalJ) {
			if (json_integer_value(externalJ))
				params[EXTERNAL_PARAM].setValue(1.f);
		}
	}
};


struct ScopeDisplay : LedDisplay {
	Scope* module;
	ModuleWidget* moduleWidget;
	int statsFrame = 0;
	std::string fontPath;

	struct Stats {
		float min = INFINITY;
		float max = -INFINITY;
	};
	Stats statsX;
	Stats statsY;

	ScopeDisplay() {
		fontPath = asset::system("res/fonts/ShareTechMono-Regular.ttf");
	}

	void calculateStats(Stats& stats, int wave, int channels) {
		if (!module)
			return;

		stats = Stats();
		for (int i = 0; i < BUFFER_SIZE; i++) {
			const Scope::Point& point = module->pointBuffer[i];
			for (int c = 0; c < channels; c++) {
				float max = (wave == 0) ? point.maxX[c] : point.maxY[c];
				float min = (wave == 0) ? point.minX[c] : point.minY[c];

				stats.max = std::fmax(stats.max, max);
				stats.min = std::fmin(stats.min, min);
			}
		}
	}

	void drawWave(const DrawArgs& args, int wave, int channel, float offset, float gain) {
		if (!module)
			return;

		nvgSave(args.vg);
		Rect b = box.zeroPos().shrink(Vec(0, 15));
		nvgScissor(args.vg, RECT_ARGS(b));
		nvgBeginPath(args.vg);
		// Draw max points on top
		for (int i = 0; i < BUFFER_SIZE; i++) {
			const Scope::Point& point = module->pointBuffer[i];
			float max = (wave == 0) ? point.maxX[channel] : point.maxY[channel];
			if (!std::isfinite(max))
				max = 0.f;

			Vec p;
			p.x = (float) i / (BUFFER_SIZE - 1);
			p.y = (max + offset) * gain * -0.5f + 0.5f;
			p = b.interpolate(p);
			p.y -= 1.0;
			if (i == 0)
				nvgMoveTo(args.vg, p.x, p.y);
			else
				nvgLineTo(args.vg, p.x, p.y);
		}
		// Draw min points on bottom
		for (int i = BUFFER_SIZE - 1; i >= 0; i--) {
			const Scope::Point& point = module->pointBuffer[i];
			float min = (wave == 0) ? point.minX[channel] : point.minY[channel];
			if (!std::isfinite(min))
				min = 0.f;

			Vec p;
			p.x = (float) i / (BUFFER_SIZE - 1);
			p.y = (min + offset) * gain * -0.5f + 0.5f;
			p = b.interpolate(p);
			p.y += 1.0;
			nvgLineTo(args.vg, p.x, p.y);
		}
		nvgClosePath(args.vg);
		// nvgLineCap(args.vg, NVG_ROUND);
		// nvgMiterLimit(args.vg, 2.f);
		nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
		nvgFill(args.vg);
		nvgResetScissor(args.vg);
		nvgRestore(args.vg);
	}

	void drawLissajous(const DrawArgs& args, int channel, float offsetX, float gainX, float offsetY, float gainY) {
		if (!module)
			return;

		nvgSave(args.vg);
		Rect b = box.zeroPos().shrink(Vec(0, 15));
		nvgScissor(args.vg, RECT_ARGS(b));
		nvgBeginPath(args.vg);
		int bufferIndex = module->bufferIndex;
		for (int i = 0; i < BUFFER_SIZE; i++) {
			// Get average point
			const Scope::Point& point = module->pointBuffer[(i + bufferIndex) % BUFFER_SIZE];
			float avgX = (point.minX[channel] + point.maxX[channel]) / 2;
			float avgY = (point.minY[channel] + point.maxY[channel]) / 2;
			if (!std::isfinite(avgX) || !std::isfinite(avgY))
				continue;

			Vec p;
			p.x = (avgX + offsetX) * gainX * 0.5f + 0.5f;
			p.y = (avgY + offsetY) * gainY * -0.5f + 0.5f;
			p = b.interpolate(p);
			if (i == 0)
				nvgMoveTo(args.vg, p.x, p.y);
			else
				nvgLineTo(args.vg, p.x, p.y);
		}
		nvgLineCap(args.vg, NVG_ROUND);
		nvgMiterLimit(args.vg, 2.f);
		nvgStrokeWidth(args.vg, 1.5f);
		nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
		nvgStroke(args.vg);
		nvgResetScissor(args.vg);
		nvgRestore(args.vg);
	}

	void drawTrig(const DrawArgs& args, float value) {
		Rect b = Rect(Vec(0, 15), box.size.minus(Vec(0, 15 * 2)));
		nvgScissor(args.vg, b.pos.x, b.pos.y, b.size.x, b.size.y);

		value = value / 2.f + 0.5f;
		Vec p = Vec(box.size.x, b.pos.y + b.size.y * (1.f - value));

		// Draw line
		nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x10));
		{
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, p.x - 13, p.y);
			nvgLineTo(args.vg, 0, p.y);
		}
		nvgStroke(args.vg);

		// Draw indicator
		nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x60));
		{
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, p.x - 2, p.y - 4);
			nvgLineTo(args.vg, p.x - 9, p.y - 4);
			nvgLineTo(args.vg, p.x - 13, p.y);
			nvgLineTo(args.vg, p.x - 9, p.y + 4);
			nvgLineTo(args.vg, p.x - 2, p.y + 4);
			nvgClosePath(args.vg);
		}
		nvgFill(args.vg);

		std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
		if (font) {
			nvgFontSize(args.vg, 9);
			nvgFontFaceId(args.vg, font->handle);
			nvgFillColor(args.vg, nvgRGBA(0x1e, 0x28, 0x2b, 0xff));
			nvgText(args.vg, p.x - 8, p.y + 3, "T", NULL);
		}
		nvgResetScissor(args.vg);
	}

	void drawStats(const DrawArgs& args, Vec pos, const char* title, const Stats& stats) {
		std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
		if (!font)
			return;
		nvgFontSize(args.vg, 13);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -2);

		nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x40));
		nvgText(args.vg, pos.x + 6, pos.y + 11, title, NULL);

		nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x80));
		pos = pos.plus(Vec(22, 11));

		std::string text;
		text = "pp ";
		float pp = stats.max - stats.min;
		text += isNear(pp, 0.f, 100.f) ? string::f("% 6.2f", pp) : "  ---";
		nvgText(args.vg, pos.x, pos.y, text.c_str(), NULL);
		text = "max ";
		text += isNear(stats.max, 0.f, 100.f) ? string::f("% 6.2f", stats.max) : "  ---";
		nvgText(args.vg, pos.x + 58 * 1, pos.y, text.c_str(), NULL);
		text = "min ";
		text += isNear(stats.min, 0.f, 100.f) ? string::f("% 6.2f", stats.min) : "  ---";
		nvgText(args.vg, pos.x + 58 * 2, pos.y, text.c_str(), NULL);
	}

	void drawBackground(const DrawArgs& args) {
		Rect b = box.zeroPos().shrink(Vec(0, 15));

		nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x10));
		for (int i = 0; i < 5; i++) {
			nvgBeginPath(args.vg);

			Vec p;
			p.x = 0.0;
			p.y = float(i) / (5 - 1);
			nvgMoveTo(args.vg, VEC_ARGS(b.interpolate(p)));

			p.x = 1.0;
			nvgLineTo(args.vg, VEC_ARGS(b.interpolate(p)));
			nvgStroke(args.vg);
		}
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;

		// Background lines
		drawBackground(args);

		if (!module)
			return;

		float gainX = std::pow(2.f, std::round(module->params[Scope::X_SCALE_PARAM].getValue())) / 10.f;
		float gainY = std::pow(2.f, std::round(module->params[Scope::Y_SCALE_PARAM].getValue())) / 10.f;
		float offsetX = module->params[Scope::X_POS_PARAM].getValue();
		float offsetY = module->params[Scope::Y_POS_PARAM].getValue();

		// Get input colors
		PortWidget* inputX = moduleWidget->getInput(Scope::X_INPUT);
		PortWidget* inputY = moduleWidget->getInput(Scope::Y_INPUT);
		CableWidget* inputXCable = APP->scene->rack->getTopCable(inputX);
		CableWidget* inputYCable = APP->scene->rack->getTopCable(inputY);
		NVGcolor inputXColor = inputXCable ? inputXCable->color : color::WHITE;
		NVGcolor inputYColor = inputYCable ? inputYCable->color : color::WHITE;

		// Draw waveforms
		if (module->isLissajous()) {
			// X x Y
			int lissajousChannels = std::min(module->channelsX, module->channelsY);
			for (int c = 0; c < lissajousChannels; c++) {
				nvgStrokeColor(args.vg, SCHEME_YELLOW);
				drawLissajous(args, c, offsetX, gainX, offsetY, gainY);
			}
		}
		else {
			// Y
			for (int c = 0; c < module->channelsY; c++) {
				nvgFillColor(args.vg, inputYColor);
				drawWave(args, 1, c, offsetY, gainY);
			}

			// X
			for (int c = 0; c < module->channelsX; c++) {
				nvgFillColor(args.vg, inputXColor);
				drawWave(args, 0, c, offsetX, gainX);
			}

			// Trigger
			float trigThreshold = module->params[Scope::TRIG_PARAM].getValue();
			trigThreshold = (trigThreshold + offsetX) * gainX;
			drawTrig(args, trigThreshold);
		}

		// Calculate and draw stats
		if (++statsFrame >= 4) {
			statsFrame = 0;
			calculateStats(statsX, 0, module->channelsX);
			calculateStats(statsY, 1, module->channelsY);
		}
		drawStats(args, Vec(0, 0 + 1), "1", statsX);
		drawStats(args, Vec(0, box.size.y - 15 - 1), "2", statsY);
	}
};


struct ScopeWidget : ModuleWidget {
	ScopeWidget(Scope* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Scope.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(8.643, 80.603)), module, Scope::LISSAJOUS_PARAM, Scope::LISSAJOUS_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(24.897, 80.551)), module, Scope::X_SCALE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(41.147, 80.551)), module, Scope::Y_SCALE_PARAM));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(57.397, 80.521)), module, Scope::EXTERNAL_PARAM, Scope::EXTERNAL_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(8.643, 96.819)), module, Scope::TIME_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(24.897, 96.789)), module, Scope::X_POS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(41.147, 96.815)), module, Scope::Y_POS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(57.397, 96.815)), module, Scope::TRIG_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.643, 113.115)), module, Scope::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(33.023, 113.115)), module, Scope::Y_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(57.397, 113.115)), module, Scope::TRIG_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.833, 113.115)), module, Scope::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(45.212, 113.115)), module, Scope::Y_OUTPUT));

		ScopeDisplay* display = createWidget<ScopeDisplay>(mm2px(Vec(0.0, 13.039)));
		display->box.size = mm2px(Vec(66.04, 55.88));
		display->module = module;
		display->moduleWidget = this;
		addChild(display);
	}
};


Model* modelScope = createModel<Scope, ScopeWidget>("Scope");
