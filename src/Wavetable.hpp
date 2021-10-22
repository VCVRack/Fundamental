#pragma once
#include <rack.hpp>
#include <osdialog.h>
#include "dr_wav.h"


static const char WAVETABLE_FILTERS[] = "WAV (.wav):wav,WAV";


/** Loads and stores wavetable samples and metadata */
struct Wavetable {
	/** All waves concatenated */
	std::vector<float> samples;
	/** Number of points in each wave */
	size_t waveLen = 0;
	/** Name of loaded wavetable. */
	std::string filename;

	Wavetable() {
		reset();
	}

	float &at(size_t sampleIndex, size_t waveIndex) {
		return samples[sampleIndex + waveIndex * waveLen];
	}
	float at(size_t sampleIndex, size_t waveIndex) const {
		return samples[sampleIndex + waveIndex * waveLen];
	}

	/** Returns the number of waves in the wavetable. */
	size_t getWaveCount() const {
		return samples.size() / waveLen;
	}

	void reset() {
		filename = "Basic.wav";
		waveLen = 256;
		samples.clear();
		samples.resize(waveLen * 4);

		for (size_t i = 0; i < waveLen; i++) {
			float p = float(i) / waveLen;
			float sin = std::sin(2 * float(M_PI) * p);
			at(i, 0) = sin;
			float tri = (p < 0.25f) ? 4*p : (p < 0.75f) ? 2 - 4*p : 4*p - 4;
			at(i, 1) = tri;
			float saw = (p < 0.5f) ? 2*p : 2*p - 2;
			at(i, 2) = saw;
			float sqr = (p < 0.5f) ? 1 : -1;
			at(i, 3) = sqr;
		}
	}

	json_t* toJson() const {
		json_t* rootJ = json_object();
		// waveLen
		json_object_set_new(rootJ, "waveLen", json_integer(waveLen));
		// filename
		json_object_set_new(rootJ, "filename", json_string(filename.c_str()));
		return rootJ;
	}

	void fromJson(json_t* rootJ) {
		// waveLen
		json_t* waveLenJ = json_object_get(rootJ, "waveLen");
		if (waveLenJ)
			waveLen = json_integer_value(waveLenJ);
		// filename
		json_t* filenameJ = json_object_get(rootJ, "filename");
		if (filenameJ)
			filename = json_string_value(filenameJ);
	}

	void load(std::string path) {
		drwav wav;
		// TODO Unicode on Windows
		if (!drwav_init_file(&wav, path.c_str(), NULL))
			return;

		samples.clear();
		samples.resize(wav.totalPCMFrameCount * wav.channels);

		drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, samples.data());

		drwav_uninit(&wav);
	}

	void loadDialog() {
		osdialog_filters* filters = osdialog_filters_parse(WAVETABLE_FILTERS);
		DEFER({osdialog_filters_free(filters);});

		char* pathC = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
		if (!pathC) {
			// Fail silently
			return;
		}
		std::string path = pathC;
		std::free(pathC);

		load(path);
		filename = system::getFilename(path);
	}

	void save(std::string path) const {
		drwav_data_format format;
		format.container = drwav_container_riff;
		format.format = DR_WAVE_FORMAT_PCM;
		format.channels = 1;
		format.sampleRate = 44100;
		format.bitsPerSample = 16;

		drwav wav;
		if (!drwav_init_file_write(&wav, path.c_str(), &format, NULL))
			return;

		size_t len = samples.size();
		int16_t* buf = new int16_t[len];
		drwav_f32_to_s16(buf, samples.data(), len);
		drwav_write_pcm_frames(&wav, len, buf);
		delete[] buf;

		drwav_uninit(&wav);
	}

	void saveDialog() const {
		osdialog_filters* filters = osdialog_filters_parse(WAVETABLE_FILTERS);
		DEFER({osdialog_filters_free(filters);});

		char* pathC = osdialog_file(OSDIALOG_SAVE, NULL, filename.c_str(), filters);
		if (!pathC) {
			// Cancel silently
			return;
		}
		DEFER({std::free(pathC);});

		// Automatically append .wav extension
		std::string path = pathC;
		if (system::getExtension(path) != ".wav") {
			path += ".wav";
		}

		save(path);
	}

	void appendContextMenu(Menu* menu) {
		menu->addChild(createMenuItem("Load wavetable", "",
			[=]() {loadDialog();}
		));

		menu->addChild(createMenuItem("Save wavetable", "",
			[=]() {saveDialog();}
		));

		int sizeOffset = 4;
		std::vector<std::string> sizeLabels;
		for (int i = sizeOffset; i <= 14; i++) {
			sizeLabels.push_back(string::f("%d", 1 << i));
		}
		menu->addChild(createIndexSubmenuItem("Wave points", sizeLabels,
			[=]() {return math::log2(waveLen) - sizeOffset;},
			[=](int i) {waveLen = 1 << (i + sizeOffset);}
		));
	}
};


static const Wavetable defaultWavetable;


template <class TModule>
struct WTDisplay : LedDisplay {
	TModule* module;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			// Get module data or defaults
			const Wavetable& wavetable = module ? module->wavetable : defaultWavetable;
			float lastPos = module ? module->lastPos : 0.f;

			// Draw filename text
			std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
			nvgFontSize(args.vg, 13);
			nvgFontFaceId(args.vg, font->handle);
			nvgFillColor(args.vg, SCHEME_YELLOW);
			nvgText(args.vg, 4.0, 13.0, wavetable.filename.c_str(), NULL);

			// Get wavetable metadata
			if (wavetable.waveLen < 2)
				return;

			size_t waveCount = wavetable.getWaveCount();
			if (waveCount < 1)
				return;
			if (lastPos > waveCount - 1)
				return;
			float posF = lastPos - std::trunc(lastPos);
			size_t pos0 = std::trunc(lastPos);

			// Draw scope
			nvgScissor(args.vg, RECT_ARGS(args.clipBox));
			nvgBeginPath(args.vg);
			Vec scopePos = Vec(0.0, 13.0);
			Rect scopeRect = Rect(scopePos, box.size - scopePos);
			scopeRect = scopeRect.shrink(Vec(4, 5));
			size_t iSkip = wavetable.waveLen / 128 + 1;

			for (size_t i = 0; i <= wavetable.waveLen; i += iSkip) {
				// Get wave value
				float wave;
				float wave0 = wavetable.at(i % wavetable.waveLen, pos0);
				if (posF > 0.f) {
					float wave1 = wavetable.at(i % wavetable.waveLen, (pos0 + 1));
					wave = crossfade(wave0, wave1, posF);
				}
				else {
					wave = wave0;
				}

				// Add point to line
				Vec p;
				p.x = float(i) / wavetable.waveLen;
				p.y = 0.5f - 0.5f * wave;
				p = scopeRect.pos + scopeRect.size * p;
				if (i == 0)
					nvgMoveTo(args.vg, VEC_ARGS(p));
				else
					nvgLineTo(args.vg, VEC_ARGS(p));
			}
			nvgLineCap(args.vg, NVG_ROUND);
			nvgMiterLimit(args.vg, 2.f);
			nvgStrokeWidth(args.vg, 1.5f);
			nvgStrokeColor(args.vg, SCHEME_YELLOW);
			nvgStroke(args.vg);
		}
		LedDisplay::drawLayer(args, layer);
	}

	// void onButton(const ButtonEvent& e) override {
	// 	if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
	// 		if (module)
	// 			module->loadWavetableDialog();
	// 		e.consume(this);
	// 	}
	// 	LedDisplay::onButton(e);
	// }

	void onPathDrop(const PathDropEvent& e) override {
		if (!module)
			return;
		if (e.paths.empty())
			return;
		std::string path = e.paths[0];
		if (system::getExtension(path) != ".wav")
			return;
		module->wavetable.load(path);
		module->wavetable.filename = system::getFilename(path);
		e.consume(this);
	}
};

