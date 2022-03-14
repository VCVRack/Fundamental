#include "plugin.hpp"


/** Based on "The Voss algorithm"
http://www.firstpr.com.au/dsp/pink-noise/
*/
template <int MAX_OCTAVES = 16>
struct PinkNoiseGenerator {
	int frame = -1;
	float values[MAX_OCTAVES] = {};
	int octaves = 8;

	float process() {
		int lastFrame = frame;
		frame++;
		if (frame >= (1 << octaves))
			frame = 0;
		int diff = lastFrame ^ frame;

		float sum = 0.f;
		for (int i = 0; i < octaves; i++) {
			if (diff & (1 << i)) {
				values[i] = random::uniform() - 0.5f;
			}
			sum += values[i];
		}
		return sum;
	}
};


struct InverseAWeightingFFTFilter {
	static constexpr int BUFFER_LEN = 1024;

	alignas(16) float inputBuffer[BUFFER_LEN] = {};
	alignas(16) float outputBuffer[BUFFER_LEN] = {};
	int frame = 0;
	dsp::RealFFT fft;

	InverseAWeightingFFTFilter() : fft(BUFFER_LEN) {}

	float process(float sampleRate, float x) {
		inputBuffer[frame] = x;
		if (++frame >= BUFFER_LEN) {
			frame = 0;
			alignas(16) float freqBuffer[BUFFER_LEN * 2];
			fft.rfft(inputBuffer, freqBuffer);

			for (int i = 0; i < BUFFER_LEN; i++) {
				float f = sampleRate / 2 / BUFFER_LEN * i;
				float amp = 0.f;
				if (80.f <= f && f <= 20000.f) {
					float f2 = f * f;
					// Inverse A-weighted curve
					amp = ((424.36f + f2) * std::sqrt((11599.3f + f2) * (544496.f + f2)) * (148693636.f + f2)) / (148693636.f * f2 * f2);
				}
				freqBuffer[2 * i + 0] *= amp / BUFFER_LEN;
				freqBuffer[2 * i + 1] *= amp / BUFFER_LEN;
			}

			fft.irfft(freqBuffer, outputBuffer);
		}
		return outputBuffer[frame];
	}
};


template <typename T>
static T softclip(T x) {
	// rational approximation of 12*tanh(x/12) with derivative 0 at x = 24
	x = simd::clamp(x, -24.f, 24.f);
	return x * (3888 + x * x) / (3888 + 9 * x * x);
}


struct Noise : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		WHITE_OUTPUT,
		PINK_OUTPUT,
		RED_OUTPUT,
		VIOLET_OUTPUT,
		BLUE_OUTPUT,
		GRAY_OUTPUT,
		BLACK_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	dsp::ClockDivider blackDivider;
	PinkNoiseGenerator<16> pinkNoiseGenerator;
	dsp::IIRFilter<2, 2> whiteFilter;
	dsp::IIRFilter<2, 2> pinkFilter;
	dsp::IIRFilter<2, 2> redFilter;
	dsp::IIRFilter<2, 2> violetFilter;
	dsp::IIRFilter<2, 2> blueFilter;
	float lastWhite = 0.f;
	float lastPink = 0.f;
	float gainWhite = 0.f;
	float gainWhiteDifferenced = 0.f;
	float gainPink = 0.f;
	float gainPinkDifferenced = 0.f;
	InverseAWeightingFFTFilter grayFilter;

	// For calibrating levels
	// float meanSqr = 0.f;

	Noise() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configOutput(WHITE_OUTPUT, "White noise");
		outputInfos[WHITE_OUTPUT]->description = "0 dB/octave power density";
		configOutput(PINK_OUTPUT, "Pink noise");
		outputInfos[PINK_OUTPUT]->description = "-3 dB/octave power density";
		configOutput(RED_OUTPUT, "Red noise");
		outputInfos[RED_OUTPUT]->description = "-6 dB/octave power density";
		configOutput(VIOLET_OUTPUT, "Violet noise");
		outputInfos[VIOLET_OUTPUT]->description = "+6 dB/octave power density";
		configOutput(BLUE_OUTPUT, "Blue noise");
		outputInfos[BLUE_OUTPUT]->description = "+3 dB/octave power density";
		configOutput(GRAY_OUTPUT, "Gray noise");
		outputInfos[GRAY_OUTPUT]->description = "Psychoacoustic equal loudness";
		configOutput(BLACK_OUTPUT, "Black noise");
		outputInfos[BLACK_OUTPUT]->description = "Uniform random numbers";

		SampleRateChangeEvent e;
		e.sampleRate = 44100.f;
		e.sampleTime = 1.f/44100.f;
		onSampleRateChange(e);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		// All noise is calibrated to 1 RMS.
		// Then they should be scaled to match the RMS of a sine wave with 5V amplitude.
		// Also normalised by bandwidth of sample rate
		gainWhite = 5.f * std::sqrt(e.sampleRate)/(std::sqrt(44100.f)*std::sqrt(2.f));
		gainWhiteDifferenced = gainWhite*e.sampleRate/44100.f;
		gainPink = 5.f/std::sqrt(2.f);
		gainPinkDifferenced = gainPink*e.sampleRate/44100.f;

		// DF1 Butterworth lowpass with cutoff 18000 Hz to keep amplitude down
		{
			const float g = std::tan(M_PI*fmin(18000.f*e.sampleTime, 0.49f));
			const float gnrm = 1.f/(1.f + g);
			const float b[] = {g*gnrm, g*gnrm};
			const float a[] = {(g - 1.f)*gnrm};
			whiteFilter.setCoefficients(b, a);
			pinkFilter.setCoefficients(b, a);
			violetFilter.setCoefficients(b, a);
			blueFilter.setCoefficients(b, a);
		}

		// DF1 Butterworth lowpass with cutoff 60 Hz
		{
			const float g = std::tan(M_PI*fmin(60.f*e.sampleTime, 0.49f));
			const float gnrm = 1.f/(1.f + g);
			const float b[] = {g*gnrm, g*gnrm};
			const float a[] = {(g - 1.f)*gnrm};
			redFilter.setCoefficients(b, a);
		}
		const int octaves = clamp(int(std::log2(e.sampleRate) + 1) - 8, 4, 16);
		pinkNoiseGenerator.octaves = octaves;
	}

	void process(const ProcessArgs& args) override {

		if (outputs[WHITE_OUTPUT].isConnected() || outputs[RED_OUTPUT].isConnected() || outputs[VIOLET_OUTPUT].isConnected() || outputs[GRAY_OUTPUT].isConnected()) {
			// White noise: equal power density
			float white = whiteFilter.process(random::normal());
			outputs[WHITE_OUTPUT].setVoltage(softclip(white * gainWhite));

			// Red/Brownian noise: -6dB/oct
			if (outputs[RED_OUTPUT].isConnected()) {
				float red = redFilter.process(white) / 0.0645f;
				outputs[RED_OUTPUT].setVoltage(softclip(red * gainWhite));
			}

			// Violet/purple noise: 6dB/oct
			if (outputs[VIOLET_OUTPUT].isConnected()) {
				float violet = violetFilter.process((white - lastWhite) / 1.41f);
				lastWhite = white;
				outputs[VIOLET_OUTPUT].setVoltage(softclip(violet * gainWhiteDifferenced));
			}

			// Gray noise: psychoacoustic equal loudness curve, specifically inverted A-weighted
			if (outputs[GRAY_OUTPUT].isConnected()) {
				float gray = grayFilter.process(args.sampleRate, white) / 1.67f;
				outputs[GRAY_OUTPUT].setVoltage(softclip(gray * gainWhite));
			}
		}

		if (outputs[PINK_OUTPUT].isConnected() || outputs[BLUE_OUTPUT].isConnected()) {
			// Pink noise: -3dB/oct
			float pink = pinkFilter.process(pinkNoiseGenerator.process() / 0.816f);
			outputs[PINK_OUTPUT].setVoltage(softclip(pink * gainPink));

			// Blue noise: 3dB/oct
			if (outputs[BLUE_OUTPUT].isConnected()) {
				float blue = blueFilter.process((pink - lastPink) / 0.705f);
				lastPink = pink;
				outputs[BLUE_OUTPUT].setVoltage(softclip(blue * gainPinkDifferenced));
			}
		}

		// Black noise: uniform noise
		// Note: I made this definition up.
		if (outputs[BLACK_OUTPUT].isConnected()) {
			float u = random::uniform();
			outputs[BLACK_OUTPUT].setVoltage(u * 10.f - 5.f);
		}

		// meanSqr += (std::pow(gray, 2.f) - meanSqr) * args.sampleTime / 10.f;
		// DEBUG("%f", std::sqrt(meanSqr));
	}
};


struct NoiseWidget : ModuleWidget {
	NoiseWidget(Noise* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Noise.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 21.897)), module, Noise::WHITE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 37.102)), module, Noise::PINK_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 52.31)), module, Noise::RED_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 67.53)), module, Noise::VIOLET_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 82.732)), module, Noise::BLUE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 97.923)), module, Noise::GRAY_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 113.115)), module, Noise::BLACK_OUTPUT));
	}
};


Model* modelNoise = createModel<Noise, NoiseWidget>("Noise");