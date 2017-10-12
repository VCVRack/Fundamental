#include <string.h>
#include "Fundamental.hpp"
#include "dsp/digital.hpp"


#define BUFFER_SIZE 512

struct Scope : Module {
	enum ParamIds {
		X_SCALE_PARAM,
		X_POS_PARAM,
		Y_SCALE_PARAM,
		Y_POS_PARAM,
		TIME_PARAM,
		MODE_PARAM,
		TRIG_PARAM,
		EXT_PARAM,
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

	float bufferX[BUFFER_SIZE] = {};
	float bufferY[BUFFER_SIZE] = {};
	int bufferIndex = 0;
	float frameIndex = 0;

	SchmittTrigger sumTrigger;
	SchmittTrigger extTrigger;
	bool sum = false;
	bool ext = false;
	float lights[4] = {};
	SchmittTrigger resetTrigger;

	Scope() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step();

	json_t *toJson() {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "sum", json_integer((int) sum));
		json_object_set_new(rootJ, "ext", json_integer((int) ext));
		return rootJ;
	}

	void fromJson(json_t *rootJ) {
		json_t *sumJ = json_object_get(rootJ, "sum");
		if (sumJ)
			sum = json_integer_value(sumJ);

		json_t *extJ = json_object_get(rootJ, "ext");
		if (extJ)
			ext = json_integer_value(extJ);
	}

	void initialize() {
		sum = false;
		ext = false;
	}
};


void Scope::step() {
	// Modes
	if (sumTrigger.process(params[MODE_PARAM].value)) {
		sum = !sum;
	}
	lights[0] = sum ? 0.0 : 1.0;
	lights[1] = sum ? 1.0 : 0.0;

	if (extTrigger.process(params[EXT_PARAM].value)) {
		ext = !ext;
	}
	lights[2] = ext ? 0.0 : 1.0;
	lights[3] = ext ? 1.0 : 0.0;

	// Compute time
	float deltaTime = powf(2.0, params[TIME_PARAM].value);
	int frameCount = (int)ceilf(deltaTime * gSampleRate);

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
		// Trigger immediately if external but nothing plugged in
		if (ext && !inputs[TRIG_INPUT].active) {
			bufferIndex = 0; frameIndex = 0; return;
		}

		// Reset the Schmitt trigger so we don't trigger immediately if the input is high
		if (frameIndex == 0) {
			resetTrigger.reset();
		}
		frameIndex++;

		// Must go below 0.1V to trigger
		resetTrigger.setThresholds(params[TRIG_PARAM].value - 0.1, params[TRIG_PARAM].value);
		float gate = ext ? inputs[TRIG_INPUT].value : inputs[X_INPUT].value;

		// Reset if triggered
		float holdTime = 0.1;
		if (resetTrigger.process(gate) || (frameIndex >= gSampleRate * holdTime)) {
			bufferIndex = 0; frameIndex = 0; return;
		}

		// Reset if we've waited too long
		if (frameIndex >= gSampleRate * holdTime) {
			bufferIndex = 0; frameIndex = 0; return;
		}
	}
}


struct ScopeDisplay : TransparentWidget {
	Scope *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	struct Stats {
		float vrms, vpp, vmin, vmax;
		void calculate(float *values) {
			vrms = 0.0;
			vmax = -INFINITY;
			vmin = INFINITY;
			for (int i = 0; i < BUFFER_SIZE; i++) {
				float v = values[i];
				vrms += v*v;
				vmax = fmaxf(vmax, v);
				vmin = fminf(vmin, v);
			}
			vrms = sqrtf(vrms / BUFFER_SIZE);
			vpp = vmax - vmin;
		}
	};
	Stats statsX, statsY;

	ScopeDisplay() {
		font = Font::load(assetPlugin(plugin, "res/DejaVuSansMono.ttf"));
	}

	void drawWaveform(NVGcontext *vg, float *values, float gain, float offset) {
		nvgSave(vg);
		Rect b = Rect(Vec(0, 15), box.size.minus(Vec(0, 15*2)));
		nvgScissor(vg, b.pos.x, b.pos.y, b.size.x, b.size.y);
		nvgBeginPath(vg);
		// Draw maximum display left to right
		for (int i = 0; i < BUFFER_SIZE; i++) {
			float value = values[i] * gain + offset;
			Vec p = Vec(b.pos.x + i * b.size.x / (BUFFER_SIZE-1), b.pos.y + b.size.y * (1 - value) / 2);
			if (i == 0)
				nvgMoveTo(vg, p.x, p.y);
			else
				nvgLineTo(vg, p.x, p.y);
		}
		nvgLineCap(vg, NVG_ROUND);
		nvgMiterLimit(vg, 2.0);
		nvgStrokeWidth(vg, 1.75);
		nvgGlobalCompositeOperation(vg, NVG_LIGHTER);
		nvgStroke(vg);
		nvgResetScissor(vg);
		nvgRestore(vg);
	}

	void drawTrig(NVGcontext *vg, float value, float gain, float offset) {
		Rect b = Rect(Vec(0, 15), box.size.minus(Vec(0, 15*2)));
		nvgScissor(vg, b.pos.x, b.pos.y, b.size.x, b.size.y);

		value = value * gain + offset;
		Vec p = Vec(box.size.x, b.pos.y + b.size.y * (1 - value) / 2);

		// Draw line
		nvgStrokeColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x10));
		{
			nvgBeginPath(vg);
			nvgMoveTo(vg, p.x - 13, p.y);
			nvgLineTo(vg, 0, p.y);
			nvgClosePath(vg);
		}
		nvgStroke(vg);

		// Draw indicator
		nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x60));
		{
			nvgBeginPath(vg);
			nvgMoveTo(vg, p.x - 2, p.y - 4);
			nvgLineTo(vg, p.x - 9, p.y - 4);
			nvgLineTo(vg, p.x - 13, p.y);
			nvgLineTo(vg, p.x - 9, p.y + 4);
			nvgLineTo(vg, p.x - 2, p.y + 4);
			nvgClosePath(vg);
		}
		nvgFill(vg);

		nvgFontSize(vg, 8);
		nvgFontFaceId(vg, font->handle);
		nvgFillColor(vg, nvgRGBA(0x1e, 0x28, 0x2b, 0xff));
		nvgText(vg, p.x - 8, p.y + 3, "T", NULL);
		nvgResetScissor(vg);
	}

	void drawStats(NVGcontext *vg, Vec pos, const char *title, Stats *stats) {
		nvgFontSize(vg, 10);
		nvgFontFaceId(vg, font->handle);
		nvgTextLetterSpacing(vg, -2);

		nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0xff));
		nvgText(vg, pos.x + 5, pos.y + 10, title, NULL);

		nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x80));
		char text[128];
		snprintf(text, sizeof(text), "rms %5.2f  pp %5.2f  max % 6.2f  min % 6.2f", stats->vrms, stats->vpp, stats->vmax, stats->vmin);
		nvgText(vg, pos.x + 17, pos.y + 10, text, NULL);
	}

	void draw(NVGcontext *vg) {
		float gainX = powf(2.0, roundf(module->params[Scope::X_SCALE_PARAM].value)) / 12.0;
		float gainY = powf(2.0, roundf(module->params[Scope::Y_SCALE_PARAM].value)) / 12.0;
		float posX = module->params[Scope::X_POS_PARAM].value;
		float posY = module->params[Scope::Y_POS_PARAM].value;

		// Draw waveforms
		if (module->sum) {
			float sumBuffer[BUFFER_SIZE];
			for (int i = 0; i < BUFFER_SIZE; i++) {
				sumBuffer[i] = module->bufferX[i] + module->bufferY[i];
			}
			// X + Y
			nvgStrokeColor(vg, nvgRGBA(0x9f, 0xe4, 0x36, 0xc0));
			drawWaveform(vg, sumBuffer, gainX, posX);
		}
		else {
			// Y
			if (module->inputs[Scope::Y_INPUT].active) {
				nvgStrokeColor(vg, nvgRGBA(0xe1, 0x02, 0x78, 0xc0));
				drawWaveform(vg, module->bufferY, gainY, posY);
			}

			// X
			if (module->inputs[Scope::X_INPUT].active) {
				nvgStrokeColor(vg, nvgRGBA(0x28, 0xb0, 0xf3, 0xc0));
				drawWaveform(vg, module->bufferX, gainX, posX);
			}
		}
		drawTrig(vg, module->params[Scope::TRIG_PARAM].value, gainX, posX);

		// Calculate and draw stats
		if (++frame >= 4) {
			frame = 0;
			statsX.calculate(module->bufferX);
			statsY.calculate(module->bufferY);
		}
		drawStats(vg, Vec(0, 0), "X", &statsX);
		drawStats(vg, Vec(0, box.size.y - 15), "Y", &statsY);
	}
};


ScopeWidget::ScopeWidget() {
	Scope *module = new Scope();
	setModule(module);
	box.size = Vec(15*13, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/Scope.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	{
		ScopeDisplay *display = new ScopeDisplay();
		display->module = module;
		display->box.pos = Vec(0, 44);
		display->box.size = Vec(box.size.x, 140);
		addChild(display);
	}

	addParam(createParam<Davies1900hSmallBlackSnapKnob>(Vec(15, 209), module, Scope::X_SCALE_PARAM, -1.0, 9.0, 1.0));
	addParam(createParam<Davies1900hSmallBlackKnob>(Vec(15, 263), module, Scope::X_POS_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Davies1900hSmallBlackSnapKnob>(Vec(61, 209), module, Scope::Y_SCALE_PARAM, -1.0, 9.0, 1.0));
	addParam(createParam<Davies1900hSmallBlackKnob>(Vec(61, 263), module, Scope::Y_POS_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Davies1900hSmallBlackKnob>(Vec(107, 209), module, Scope::TIME_PARAM, -6.0, -16.0, -14.0));
	addParam(createParam<CKD6>(Vec(106, 262), module, Scope::MODE_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Davies1900hSmallBlackKnob>(Vec(153, 209), module, Scope::TRIG_PARAM, -12.0, 12.0, 0.0));
	addParam(createParam<CKD6>(Vec(152, 262), module, Scope::EXT_PARAM, 0.0, 1.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(17, 319), module, Scope::X_INPUT));
	addInput(createInput<PJ301MPort>(Vec(63, 319), module, Scope::Y_INPUT));
	addInput(createInput<PJ301MPort>(Vec(154, 319), module, Scope::TRIG_INPUT));

	addChild(createValueLight<TinyLight<GreenValueLight>>(Vec(104, 251), &module->lights[0]));
	addChild(createValueLight<TinyLight<GreenValueLight>>(Vec(104, 296), &module->lights[1]));
	addChild(createValueLight<TinyLight<GreenValueLight>>(Vec(150, 251), &module->lights[2]));
	addChild(createValueLight<TinyLight<GreenValueLight>>(Vec(150, 296), &module->lights[3]));
}
