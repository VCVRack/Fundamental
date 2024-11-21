#pragma once
#include <rack.hpp>
#include <osdialog.h>
#include "dr_wav.h"
#include <thread>


static const char WAVETABLE_LOAD_FILTERS[] = "WAV:wav,WAV,aif,AIF,aiff,AIFF,f32,s8,i8,s16,i16,s24,i24,s32,i32";
static const char WAVETABLE_SAVE_FILTERS[] = "WAV:wav,WAV";
static std::string wavetableDir;


/** Loads and stores wavetable samples and metadata */
struct Wavetable {
	/** All waves concatenated
	(waveCount, waveLen)
	*/
	std::vector<float> samples;
	/** Number of points in each wave */
	size_t waveLen = 0;
	/** Name of loaded wavetable. */
	std::string filename;

	// Interpolated wavetables
	/** Upsampling factor. No upsampling if 0. */
	size_t quality = 0;
	/** Number of filtered wavetables. Automatically computed from waveLen. */
	size_t octaves = 0;
	/** Waves bandlimited at each octave
	(octave, waveCount, waveLen * quality)
	*/
	std::vector<float> interpolatedSamples;

	bool loading = false;

	Wavetable() {}

	float &at(size_t waveIndex, size_t sampleIndex) {
		return samples[waveLen * waveIndex + sampleIndex];
	}
	float at(size_t waveIndex, size_t sampleIndex) const {
		return samples[waveLen * waveIndex + sampleIndex];
	}
	float interpolatedAt(size_t octave, size_t waveIndex, size_t sampleIndex) const {
		return interpolatedSamples[samples.size() * quality * octave + waveLen * quality * waveIndex + sampleIndex];
	}

	void reset() {
		filename = "Basic.wav";
		waveLen = 1024;
		loading = true;
		DEFER({loading = false;});
		// HACK Sleep 100us so DSP thread is likely to finish processing before we resize the vector
		std::this_thread::sleep_for(std::chrono::duration<double>(100e-6));
		samples.resize(waveLen * 4);

		// Sine
		for (size_t i = 0; i < waveLen; i++) {
			float p = float(i) / waveLen;
			at(0, i) = std::sin(2 * float(M_PI) * p);
		}
		// Triangle
		for (size_t i = 0; i < waveLen; i++) {
			float p = float(i) / waveLen;
			at(1, i) = (p < 0.25f) ? 4*p : (p < 0.75f) ? 2 - 4*p : 4*p - 4;
		}
		// Sawtooth
		for (size_t i = 0; i < waveLen; i++) {
			float p = float(i) / waveLen;
			at(2, i) = (p < 0.5f) ? 2*p : 2*p - 2;
		}
		// Square
		for (size_t i = 0; i < waveLen; i++) {
			float p = float(i) / waveLen;
			at(3, i) = (p < 0.5f) ? 1 : -1;
		}
		interpolate();
	}

	void setQuality(size_t quality) {
		if (quality == this->quality)
			return;
		this->quality = quality;
		interpolate();
	}

	void setWaveLen(size_t waveLen) {
		if (waveLen == this->waveLen)
			return;
		this->waveLen = waveLen;
		interpolate();
	}

	/** Returns the number of waves in the wavetable. */
	size_t getWaveCount() const {
		if (waveLen == 0)
			return 0;
		return samples.size() / waveLen;
	}

	void interpolate() {
		if (quality == 0)
			return;
		// pffft only supports >=32 points
		if (waveLen < 32)
			return;
		// pffft only supports multiples of 32 points
		if ((waveLen % 32) != 0)
			return;

		size_t waveCount = getWaveCount();
		if (waveCount == 0)
			return;

		octaves = math::log2(waveLen) - 1;
		interpolatedSamples.clear();
		interpolatedSamples.resize(octaves * samples.size() * quality);

		float* in = new float[waveLen];
		float* inF = new float[2 * waveLen];
		dsp::RealFFT inFFT(waveLen);

		float* outF = new float[2 * waveLen * quality]();
		dsp::RealFFT outFFT(waveLen * quality);

		for (size_t i = 0; i < waveCount; i++) {
			// Compute FFT of wave
			for (size_t j = 0; j < waveLen; j++) {
				in[j] = samples[waveLen * i + j] / waveLen;
			}
			inFFT.rfft(in, inF);
			// Compute FFT-filtered versions of each wave
			for (size_t octave = 0; octave < octaves; octave++) {
				size_t bins = 1 << octave;
				// Only overwrite the first waveLen bins
				for (size_t j = 0; j < waveLen; j++) {
					outF[2 * j + 0] = (j <= bins) ? inF[2 * j + 0] : 0.f;
					outF[2 * j + 1] = (j <= bins) ? inF[2 * j + 1] : 0.f;
				}
				outFFT.irfft(outF, &interpolatedSamples[samples.size() * quality * octave + waveLen * quality * i]);
			}
		}

		delete[] in;
		delete[] inF;
		delete[] outF;
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
			setWaveLen(json_integer_value(waveLenJ));
		// filename
		json_t* filenameJ = json_object_get(rootJ, "filename");
		if (filenameJ)
			filename = json_string_value(filenameJ);
	}

	void load(std::string path) {
		loading = true;
		DEFER({loading = false;});
		// HACK Sleep 100us so DSP thread is likely to finish processing before we resize the vector
		std::this_thread::sleep_for(std::chrono::duration<double>(100e-6));

		std::string ext = string::lowercase(system::getExtension(path));
		if (ext == ".wav") {
			// Load WAV
			drwav wav;
#if defined ARCH_WIN
			if (!drwav_init_file_w(&wav, string::UTF8toUTF16(path).c_str(), NULL))
#else
			if (!drwav_init_file(&wav, path.c_str(), NULL))
#endif
				return;

			size_t len = wav.totalPCMFrameCount * wav.channels;
			if (len == 0 || len >= (1 << 20))
				return;

			samples.clear();
			samples.resize(len);

			// If sample rate is a power of 2, set waveLen to it.
			if ((wav.sampleRate & (wav.sampleRate - 1)) == 0)
				waveLen = wav.sampleRate;

			drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, samples.data());
			drwav_uninit(&wav);
		}
		else {
			// Load bytes from file
			std::vector<uint8_t> data = system::readFile(path);
			samples.clear();

			if (ext == ".f32") {
				size_t len = data.size() / sizeof(float);
				samples.resize(len);
				// This is the same as memcpy but consistent with the other conversions.
				dsp::convert((const float*) data.data(), samples.data(), len);
			}
			else if (ext == ".s8" || ext == ".i8") {
				size_t len = data.size() / sizeof(int8_t);
				samples.resize(len);
				dsp::convert((const int8_t*) data.data(), samples.data(), len);
			}
			else if (ext == ".s16" || ext == ".i16") {
				size_t len = data.size() / sizeof(int16_t);
				samples.resize(len);
				dsp::convert((const int16_t*) data.data(), samples.data(), len);
			}
			else if (ext == ".s24" || ext == ".i24") {
				size_t len = data.size() / sizeof(dsp::Int24);
				samples.resize(len);
				dsp::convert((const dsp::Int24*) data.data(), samples.data(), len);
			}
			else if (ext == ".s32" || ext == ".i32") {
				size_t len = data.size() / sizeof(int32_t);
				samples.resize(len);
				dsp::convert((const int32_t*) data.data(), samples.data(), len);
			}
			else {
				return;
			}
		}

		interpolate();
	}

	void loadDialog() {
		osdialog_filters* filters = osdialog_filters_parse(WAVETABLE_LOAD_FILTERS);
		DEFER({osdialog_filters_free(filters);});

		char* pathC = osdialog_file(OSDIALOG_OPEN, wavetableDir.empty() ? NULL : wavetableDir.c_str(), NULL, filters);
		if (!pathC) {
			// Fail silently
			return;
		}
		std::string path = pathC;
		std::free(pathC);
		wavetableDir = system::getDirectory(path);

		load(path);
		filename = system::getFilename(path);
	}

	void save(std::string path) const {
		if (samples.size() == 0)
			return;

		drwav_data_format format;
		format.container = drwav_container_riff;
		format.format = DR_WAVE_FORMAT_PCM;
		format.channels = 1;
		format.sampleRate = waveLen;
		format.bitsPerSample = 16;

		drwav wav;
		if (!drwav_init_file_write(&wav, path.c_str(), &format, NULL))
			return;

		int16_t* buf = new int16_t[samples.size()];
		drwav_f32_to_s16(buf, samples.data(), samples.size());
		drwav_write_pcm_frames(&wav, samples.size(), buf);
		delete[] buf;

		drwav_uninit(&wav);
	}

	void saveDialog() const {
		osdialog_filters* filters = osdialog_filters_parse(WAVETABLE_SAVE_FILTERS);
		DEFER({osdialog_filters_free(filters);});

		char* pathC = osdialog_file(OSDIALOG_SAVE, wavetableDir.empty() ? NULL : wavetableDir.c_str(), filename.c_str(), filters);
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
		wavetableDir = system::getDirectory(path);

		save(path);
	}

	void appendContextMenu(Menu* menu) {
		menu->addChild(createMenuItem("Initialize wavetable", "",
			[=]() {reset();}
		));

		menu->addChild(createMenuItem("Load wavetable", "",
			[=]() {loadDialog();}
		));

		menu->addChild(createMenuItem("Save wavetable", "",
			[=]() {saveDialog();}
		));

		int sizeOffset = 5;
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


static Wavetable defaultWavetable;


template <class TModule>
struct WTDisplay : LedDisplay {
	TModule* module;

	void drawLayer(const DrawArgs& args, int layer) override {
		nvgScissor(args.vg, RECT_ARGS(args.clipBox));

		if (layer == 1) {
			if (defaultWavetable.samples.empty())
				defaultWavetable.reset();

			// Get module data or defaults
			const Wavetable& wavetable = module ? module->wavetable : defaultWavetable;
			float lastPos = module ? module->lastPos : 0.f;

			// Draw filename text
			std::string fontPath = asset::system("res/fonts/ShareTechMono-Regular.ttf");
			std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
			if (!font)
				return;
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
				float wave0 = wavetable.at(pos0, i % wavetable.waveLen);
				if (posF > 0.f) {
					float wave1 = wavetable.at(pos0 + 1, i % wavetable.waveLen);
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

		nvgResetScissor(args.vg);
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

