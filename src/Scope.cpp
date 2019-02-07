#include <string.h>
#include "plugin.hpp"


static const int BUFFER_SIZE = 512;


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
		NUM_OUTPUTS
	};
	enum LightIds {
		PLOT_LIGHT,
		LISSAJOUS_LIGHT,
		INTERNAL_LIGHT,
		EXTERNAL_LIGHT,
		NUM_LIGHTS
	};

	float bufferX[BUFFER_SIZE] = {};
	float bufferY[BUFFER_SIZE] = {};
	int bufferIndex = 0;
	float frameIndex = 0;

	dsp::SchmittTrigger sumTrigger;
	dsp::SchmittTrigger extTrigger;
	bool lissajous = false;
	bool external = false;
	dsp::SchmittTrigger resetTrigger;

	Scope() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		params[X_SCALE_PARAM].config(-2.0f, 8.0f, 0.0f);
		params[X_POS_PARAM].config(-10.0f, 10.0f, 0.0f);
		params[Y_SCALE_PARAM].config(-2.0f, 8.0f, 0.0f);
		params[Y_POS_PARAM].config(-10.0f, 10.0f, 0.0f);
		params[TIME_PARAM].config(6.0f, 16.0f, 14.0f);
		params[LISSAJOUS_PARAM].config(0.0f, 1.0f, 0.0f);
		params[TRIG_PARAM].config(-10.0f, 10.0f, 0.0f);
		params[EXTERNAL_PARAM].config(0.0f, 1.0f, 0.0f);
	}

	void step() override {
		// Modes
		if (sumTrigger.process(params[LISSAJOUS_PARAM].value)) {
			lissajous = !lissajous;
		}
		lights[PLOT_LIGHT].value = lissajous ? 0.0f : 1.0f;
		lights[LISSAJOUS_LIGHT].value = lissajous ? 1.0f : 0.0f;

		if (extTrigger.process(params[EXTERNAL_PARAM].value)) {
			external = !external;
		}
		lights[INTERNAL_LIGHT].value = external ? 0.0f : 1.0f;
		lights[EXTERNAL_LIGHT].value = external ? 1.0f : 0.0f;

		// Compute time
		float deltaTime = std::pow(2.0f, -params[TIME_PARAM].value);
		int frameCount = (int) std::ceil(deltaTime * APP->engine->getSampleRate());

		// Add frame to buffer
		if (bufferIndex < BUFFER_SIZE) {
			if (++frameIndex > frameCount) {
				frameIndex = 0;
				bufferX[bufferIndex] = inputs[X_INPUT].value;
				bufferY[bufferIndex] = inputs[Y_INPUT].value;
				bufferIndex++;
			}
		}

		// Are we waiting on the next trigger?
		if (bufferIndex >= BUFFER_SIZE) {
			// Trigger immediately if external but nothing plugged in, or in Lissajous mode
			if (lissajous || (external && !inputs[TRIG_INPUT].active)) {
				bufferIndex = 0;
				frameIndex = 0;
				return;
			}

			// Reset the Schmitt trigger so we don't trigger immediately if the input is high
			if (frameIndex == 0) {
				resetTrigger.reset();
			}
			frameIndex++;

			// Must go below 0.1fV to trigger
			float gate = external ? inputs[TRIG_INPUT].value : inputs[X_INPUT].value;

			// Reset if triggered
			float holdTime = 0.1f;
			if (resetTrigger.process(rescale(gate, params[TRIG_PARAM].value - 0.1f, params[TRIG_PARAM].value, 0.f, 1.f)) || (frameIndex >= APP->engine->getSampleRate() * holdTime)) {
				bufferIndex = 0; frameIndex = 0; return;
			}

			// Reset if we've waited too long
			if (frameIndex >= APP->engine->getSampleRate() * holdTime) {
				bufferIndex = 0; frameIndex = 0; return;
			}
		}
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "lissajous", json_integer((int) lissajous));
		json_object_set_new(rootJ, "external", json_integer((int) external));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *sumJ = json_object_get(rootJ, "lissajous");
		if (sumJ)
			lissajous = json_integer_value(sumJ);

		json_t *extJ = json_object_get(rootJ, "external");
		if (extJ)
			external = json_integer_value(extJ);
	}

	void onReset() override {
		lissajous = false;
		external = false;
	}
};


struct ScopeDisplay : TransparentWidget {
	Scope *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	struct Stats {
		float vrms = 0.f;
		float vpp = 0.f;
		float vmin = 0.f;
		float vmax = 0.f;
		void calculate(float *values) {
			vrms = 0.0f;
			vmax = -INFINITY;
			vmin = INFINITY;
			for (int i = 0; i < BUFFER_SIZE; i++) {
				float v = values[i];
				vrms += v*v;
				vmax = std::max(vmax, v);
				vmin = std::min(vmin, v);
			}
			vrms = std::sqrt(vrms / BUFFER_SIZE);
			vpp = vmax - vmin;
		}
	};
	Stats statsX, statsY;

	ScopeDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/sudo/Sudo.ttf"));
	}

	void drawWaveform(const DrawContext &ctx, float *valuesX, float *valuesY) {
		if (!valuesX)
			return;
		nvgSave(ctx.vg);
		Rect b = Rect(Vec(0, 15), box.size.minus(Vec(0, 15*2)));
		nvgScissor(ctx.vg, b.pos.x, b.pos.y, b.size.x, b.size.y);
		nvgBeginPath(ctx.vg);
		// Draw maximum display left to right
		for (int i = 0; i < BUFFER_SIZE; i++) {
			float x, y;
			if (valuesY) {
				x = valuesX[i] / 2.0f + 0.5f;
				y = valuesY[i] / 2.0f + 0.5f;
			}
			else {
				x = (float)i / (BUFFER_SIZE - 1);
				y = valuesX[i] / 2.0f + 0.5f;
			}
			Vec p;
			p.x = b.pos.x + b.size.x * x;
			p.y = b.pos.y + b.size.y * (1.0f - y);
			if (i == 0)
				nvgMoveTo(ctx.vg, p.x, p.y);
			else
				nvgLineTo(ctx.vg, p.x, p.y);
		}
		nvgLineCap(ctx.vg, NVG_ROUND);
		nvgMiterLimit(ctx.vg, 2.0f);
		nvgStrokeWidth(ctx.vg, 1.5f);
		nvgGlobalCompositeOperation(ctx.vg, NVG_LIGHTER);
		nvgStroke(ctx.vg);
		nvgResetScissor(ctx.vg);
		nvgRestore(ctx.vg);
	}

	void drawTrig(const DrawContext &ctx, float value) {
		Rect b = Rect(Vec(0, 15), box.size.minus(Vec(0, 15*2)));
		nvgScissor(ctx.vg, b.pos.x, b.pos.y, b.size.x, b.size.y);

		value = value / 2.0f + 0.5f;
		Vec p = Vec(box.size.x, b.pos.y + b.size.y * (1.0f - value));

		// Draw line
		nvgStrokeColor(ctx.vg, nvgRGBA(0xff, 0xff, 0xff, 0x10));
		{
			nvgBeginPath(ctx.vg);
			nvgMoveTo(ctx.vg, p.x - 13, p.y);
			nvgLineTo(ctx.vg, 0, p.y);
			nvgClosePath(ctx.vg);
		}
		nvgStroke(ctx.vg);

		// Draw indicator
		nvgFillColor(ctx.vg, nvgRGBA(0xff, 0xff, 0xff, 0x60));
		{
			nvgBeginPath(ctx.vg);
			nvgMoveTo(ctx.vg, p.x - 2, p.y - 4);
			nvgLineTo(ctx.vg, p.x - 9, p.y - 4);
			nvgLineTo(ctx.vg, p.x - 13, p.y);
			nvgLineTo(ctx.vg, p.x - 9, p.y + 4);
			nvgLineTo(ctx.vg, p.x - 2, p.y + 4);
			nvgClosePath(ctx.vg);
		}
		nvgFill(ctx.vg);

		nvgFontSize(ctx.vg, 9);
		nvgFontFaceId(ctx.vg, font->handle);
		nvgFillColor(ctx.vg, nvgRGBA(0x1e, 0x28, 0x2b, 0xff));
		nvgText(ctx.vg, p.x - 8, p.y + 3, "T", NULL);
		nvgResetScissor(ctx.vg);
	}

	void drawStats(const DrawContext &ctx, Vec pos, const char *title, Stats *stats) {
		nvgFontSize(ctx.vg, 13);
		nvgFontFaceId(ctx.vg, font->handle);
		nvgTextLetterSpacing(ctx.vg, -2);

		nvgFillColor(ctx.vg, nvgRGBA(0xff, 0xff, 0xff, 0x40));
		nvgText(ctx.vg, pos.x + 6, pos.y + 11, title, NULL);

		nvgFillColor(ctx.vg, nvgRGBA(0xff, 0xff, 0xff, 0x80));
		char text[128];
		snprintf(text, sizeof(text), "pp % 06.2f  max % 06.2f  min % 06.2f", stats->vpp, stats->vmax, stats->vmin);
		nvgText(ctx.vg, pos.x + 22, pos.y + 11, text, NULL);
	}

	void draw(const DrawContext &ctx) override {
		if (!module)
			return;

		float gainX = powf(2.0f, roundf(module->params[Scope::X_SCALE_PARAM].value));
		float gainY = powf(2.0f, roundf(module->params[Scope::Y_SCALE_PARAM].value));
		float offsetX = module->params[Scope::X_POS_PARAM].value;
		float offsetY = module->params[Scope::Y_POS_PARAM].value;

		float valuesX[BUFFER_SIZE];
		float valuesY[BUFFER_SIZE];
		for (int i = 0; i < BUFFER_SIZE; i++) {
			int j = i;
			// Lock display to buffer if buffer update deltaTime <= 2^-11
			if (module->lissajous)
				j = (i + module->bufferIndex) % BUFFER_SIZE;
			valuesX[i] = (module->bufferX[j] + offsetX) * gainX / 10.0f;
			valuesY[i] = (module->bufferY[j] + offsetY) * gainY / 10.0f;
		}

		// Draw waveforms
		if (module->lissajous) {
			// X x Y
			if (module->inputs[Scope::X_INPUT].active || module->inputs[Scope::Y_INPUT].active) {
				nvgStrokeColor(ctx.vg, nvgRGBA(0x9f, 0xe4, 0x36, 0xc0));
				drawWaveform(ctx, valuesX, valuesY);
			}
		}
		else {
			// Y
			if (module->inputs[Scope::Y_INPUT].active) {
				nvgStrokeColor(ctx.vg, nvgRGBA(0xe1, 0x02, 0x78, 0xc0));
				drawWaveform(ctx, valuesY, NULL);
			}

			// X
			if (module->inputs[Scope::X_INPUT].active) {
				nvgStrokeColor(ctx.vg, nvgRGBA(0x28, 0xb0, 0xf3, 0xc0));
				drawWaveform(ctx, valuesX, NULL);
			}

			float valueTrig = (module->params[Scope::TRIG_PARAM].value + offsetX) * gainX / 10.0f;
			drawTrig(ctx, valueTrig);
		}

		// Calculate and draw stats
		if (++frame >= 4) {
			frame = 0;
			statsX.calculate(module->bufferX);
			statsY.calculate(module->bufferY);
		}
		drawStats(ctx, Vec(0, 0), "X", &statsX);
		drawStats(ctx, Vec(0, box.size.y - 15), "Y", &statsY);
	}
};


struct ScopeWidget : ModuleWidget {
	ScopeWidget(Scope *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Scope.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		{
			ScopeDisplay *display = new ScopeDisplay();
			display->module = module;
			display->box.pos = Vec(0, 44);
			display->box.size = Vec(box.size.x, 140);
			addChild(display);
		}

		addParam(createParam<RoundBlackSnapKnob>(Vec(15, 209), module, Scope::X_SCALE_PARAM));
		addParam(createParam<RoundBlackKnob>(Vec(15, 263), module, Scope::X_POS_PARAM));
		addParam(createParam<RoundBlackSnapKnob>(Vec(61, 209), module, Scope::Y_SCALE_PARAM));
		addParam(createParam<RoundBlackKnob>(Vec(61, 263), module, Scope::Y_POS_PARAM));
		addParam(createParam<RoundBlackKnob>(Vec(107, 209), module, Scope::TIME_PARAM));
		addParam(createParam<CKD6>(Vec(106, 262), module, Scope::LISSAJOUS_PARAM));
		addParam(createParam<RoundBlackKnob>(Vec(153, 209), module, Scope::TRIG_PARAM));
		addParam(createParam<CKD6>(Vec(152, 262), module, Scope::EXTERNAL_PARAM));

		addInput(createInput<PJ301MPort>(Vec(17, 319), module, Scope::X_INPUT));
		addInput(createInput<PJ301MPort>(Vec(63, 319), module, Scope::Y_INPUT));
		addInput(createInput<PJ301MPort>(Vec(154, 319), module, Scope::TRIG_INPUT));

		addChild(createLight<SmallLight<GreenLight>>(Vec(104, 251), module, Scope::PLOT_LIGHT));
		addChild(createLight<SmallLight<GreenLight>>(Vec(104, 296), module, Scope::LISSAJOUS_LIGHT));
		addChild(createLight<SmallLight<GreenLight>>(Vec(150, 251), module, Scope::INTERNAL_LIGHT));
		addChild(createLight<SmallLight<GreenLight>>(Vec(150, 296), module, Scope::EXTERNAL_LIGHT));
	}
};


Model *modelScope = createModel<Scope, ScopeWidget>("Scope");
