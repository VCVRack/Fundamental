#include "Fundamental.hpp"
#include "dsp.hpp"


#define OVERSAMPLE 16
#define QUALITY 16


struct VCO : Module {
	enum ParamIds {
		MODE_PARAM,
		SYNC_PARAM,
		FREQ_PARAM,
		FINE_PARAM,
		FM_PARAM,
		PW_PARAM,
		PW_CV_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,
		FM_INPUT,
		PW_INPUT,
		SYNC_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		SIN_OUTPUT,
		TRI_OUTPUT,
		SAW_OUTPUT,
		SQR_OUTPUT,
		NUM_OUTPUTS
	};

	float lastSync = 0.0;
	float phase = 0.0;
	bool syncDirection = false;

	Decimator<OVERSAMPLE, QUALITY> sinDecimator;
	Decimator<OVERSAMPLE, QUALITY> triDecimator;
	Decimator<OVERSAMPLE, QUALITY> sawDecimator;
	Decimator<OVERSAMPLE, QUALITY> sqrDecimator;
	RCFilter sqrFilter;

	float lights[1] = {};

	// For analog detuning effect
	float pitchSlew = 0.0;
	int pitchSlewIndex = 0;

	VCO();
	void step();
};


VCO::VCO() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);
}

void VCO::step() {
	bool analog = params[MODE_PARAM] < 1.0;
	// TODO Soft sync features
	bool soft = params[SYNC_PARAM] < 1.0;

	if (analog) {
		// Adjust pitch slew
		if (++pitchSlewIndex > 32) {
			const float pitchSlewTau = 100.0; // Time constant for leaky integrator in seconds
			pitchSlew += (randomNormal() - pitchSlew / pitchSlewTau) / gSampleRate;
			pitchSlewIndex = 0;
		}
	}

	// Compute frequency
	float pitch = params[FREQ_PARAM];
	if (analog) {
		// Apply pitch slew
		const float pitchSlewAmount = 3.0;
		pitch += pitchSlew * pitchSlewAmount;
	}
	else {
		// Quantize coarse knob if digital mode
		pitch = roundf(pitch);
	}
	pitch += 12.0 * getf(inputs[PITCH_INPUT]);
	pitch += 3.0 * quadraticBipolar(params[FINE_PARAM]);
	if (inputs[FM_INPUT]) {
		pitch += quadraticBipolar(params[FM_PARAM]) * 12.0 * *inputs[FM_INPUT];
	}
	float freq = 261.626 * powf(2.0, pitch / 12.0);

	// Pulse width
	const float pwMin = 0.01;
	float pw = clampf(params[PW_PARAM] + params[PW_CV_PARAM] * getf(inputs[PW_INPUT]) / 10.0, pwMin, 1.0 - pwMin);

	// Advance phase
	float deltaPhase = clampf(freq / gSampleRate, 1e-6, 0.5);

	// Detect sync
	int syncIndex = -1; // Index in the oversample loop where sync occurs [0, OVERSAMPLE)
	float syncCrossing = 0.0; // Offset that sync occurs [0.0, 1.0)
	if (inputs[SYNC_INPUT]) {
		float sync = *inputs[SYNC_INPUT] - 0.01;
		if (sync > 0.0 && lastSync <= 0.0) {
			float deltaSync = sync - lastSync;
			syncCrossing = 1.0 - sync / deltaSync;
			syncCrossing *= OVERSAMPLE;
			syncIndex = (int)syncCrossing;
			syncCrossing -= syncIndex;
		}
		lastSync = sync;
	}

	if (syncDirection)
		deltaPhase *= -1.0;

	// Oversample loop
	float sin[OVERSAMPLE];
	float tri[OVERSAMPLE];
	float saw[OVERSAMPLE];
	float sqr[OVERSAMPLE];
	sqrFilter.setCutoff(40.0 / gSampleRate);

	for (int i = 0; i < OVERSAMPLE; i++) {
		if (syncIndex == i) {
			if (soft) {
				syncDirection = !syncDirection;
				deltaPhase *= -1.0;
			}
			else {
				// phase = syncCrossing * deltaPhase / OVERSAMPLE;
				phase = 0.0;
			}
		}

		if (outputs[SIN_OUTPUT]) {
			if (analog)
				// Quadratic approximation of sine, slightly richer harmonics
				sin[i] = 1.08 * ((phase < 0.25) ? (-1.0 + (4*phase)*(4*phase)) : (phase < 0.75) ? (1.0 - (4*phase-2)*(4*phase-2)) : (-1.0 + (4*phase-4)*(4*phase-4)));
			else
				sin[i] = -cosf(2*M_PI * phase);
		}
		if (outputs[TRI_OUTPUT]) {
			if (analog)
				tri[i] = 1.35 * interpf(triTable, phase * 2047.0);
			else
				tri[i] = (phase < 0.5) ? (-1.0 + 4.0*phase) : (1.0 - 4.0*(phase - 0.5));
		}
		if (outputs[SAW_OUTPUT]) {
			if (analog)
				saw[i] = 1.5 * interpf(sawTable, phase * 2047.0);
			else
				saw[i] = -1.0 + 2.0*phase;
		}
		if (outputs[SQR_OUTPUT]) {
			sqr[i] = (phase < 1.0 - pw) ? -1.0 : 1.0;
			if (analog) {
				// Simply filter here
				sqrFilter.process(sqr[i]);
				sqr[i] = sqrFilter.highpass() / 2.0;
			}
		}

		// Advance phase
		phase += deltaPhase / OVERSAMPLE;
		phase = eucmodf(phase, 1.0);
	}

	// Set output
	if (outputs[SIN_OUTPUT])
		*outputs[SIN_OUTPUT] = 5.0 * sinDecimator.process(sin);
	if (outputs[TRI_OUTPUT])
		*outputs[TRI_OUTPUT] = 5.0 * triDecimator.process(tri);
	if (outputs[SAW_OUTPUT])
		*outputs[SAW_OUTPUT] = 5.0 * sawDecimator.process(saw);
	if (outputs[SQR_OUTPUT])
		*outputs[SQR_OUTPUT] = 5.0 * sqrDecimator.process(sqr);

	lights[0] = rescalef(pitch, -48.0, 48.0, -1.0, 1.0);
}


VCOWidget::VCOWidget() {
	VCO *module = new VCO();
	setModule(module);
	box.size = Vec(15*10, 380);

	{
		Panel *panel = new LightPanel();
		panel->box.size = box.size;
		panel->backgroundImage = Image::load("plugins/Fundamental/res/VCO.png");
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(createParam<CKSS>(Vec(15, 77), module, VCO::MODE_PARAM, 0.0, 1.0, 1.0));
	addParam(createParam<CKSS>(Vec(120, 77), module, VCO::SYNC_PARAM, 0.0, 1.0, 1.0));

	addParam(createParam<Davies1900hLargeBlackKnob>(Vec(48, 61), module, VCO::FREQ_PARAM, -54.0, 54.0, 0.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(23, 143), module, VCO::FINE_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(91, 143), module, VCO::PW_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<Davies1900hBlackKnob>(Vec(23, 208), module, VCO::FM_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(91, 208), module, VCO::PW_CV_PARAM, 0.0, 1.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(11, 276), module, VCO::PITCH_INPUT));
	addInput(createInput<PJ301MPort>(Vec(45, 276), module, VCO::FM_INPUT));
	addInput(createInput<PJ301MPort>(Vec(80, 276), module, VCO::SYNC_INPUT));
	addInput(createInput<PJ301MPort>(Vec(114, 276), module, VCO::PW_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(11, 320), module, VCO::SIN_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(45, 320), module, VCO::TRI_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(80, 320), module, VCO::SAW_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(114, 320), module, VCO::SQR_OUTPUT));

	addChild(createValueLight<SmallLight<GreenRedPolarityLight>>(Vec(99, 41), &module->lights[0]));
}