/*
The filter DSP code has been derived from
Miller Puckette's code hosted at
https://github.com/ddiakopoulos/MoogLadders/blob/master/src/RKSimulationModel.h
which is licensed for use under the following terms (MIT license):


Copyright (c) 2015, Miller Puckette. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "Fundamental.hpp"


// The clipping function of a transistor pair is approximately tanh(x)
// TODO: Put this in a lookup table. 5th order approx doesn't seem to cut it
inline float clip(float x) {
	return tanhf(x);
}

struct LadderFilter {
	float cutoff = 1000.0;
	float resonance = 1.0;
	float state[4] = {};

	void calculateDerivatives(float input, float *dstate, const float *state) {
		float cutoff2Pi = 2*M_PI * cutoff;

		float satstate0 = clip(state[0]);
		float satstate1 = clip(state[1]);
		float satstate2 = clip(state[2]);

		dstate[0] = cutoff2Pi * (clip(input - resonance * state[3]) - satstate0);
		dstate[1] = cutoff2Pi * (satstate0 - satstate1);
		dstate[2] = cutoff2Pi * (satstate1 - satstate2);
		dstate[3] = cutoff2Pi * (satstate2 - clip(state[3]));
	}

	void process(float input, float dt) {
		float deriv1[4], deriv2[4], deriv3[4], deriv4[4], tempState[4];

		calculateDerivatives(input, deriv1, state);
		for (int i = 0; i < 4; i++)
			tempState[i] = state[i] + 0.5 * dt * deriv1[i];

		calculateDerivatives(input, deriv2, tempState);
		for (int i = 0; i < 4; i++)
			tempState[i] = state[i] + 0.5 * dt * deriv2[i];

		calculateDerivatives(input, deriv3, tempState);
		for (int i = 0; i < 4; i++)
			tempState[i] = state[i] + dt * deriv3[i];

		calculateDerivatives(input, deriv4, tempState);
		for (int i = 0; i < 4; i++)
			state[i] += (1.0 / 6.0) * dt * (deriv1[i] + 2.0 * deriv2[i] + 2.0 * deriv3[i] + deriv4[i]);
	}
	void reset() {
		for (int i = 0; i < 4; i++) {
			state[i] = 0.0;
		}
	}
};


struct VCF : Module {
	enum ParamIds {
		FREQ_PARAM,
		FINE_PARAM,
		RES_PARAM,
		FREQ_CV_PARAM,
		DRIVE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FREQ_INPUT,
		RES_INPUT,
		DRIVE_INPUT,
		IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		LPF_OUTPUT,
		HPF_OUTPUT,
		NUM_OUTPUTS
	};

	LadderFilter filter;

	VCF() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step() override;
	void reset() override {
		filter.reset();
	}
};


void VCF::step() {
	float input = inputs[IN_INPUT].value / 5.0;
	float drive = params[DRIVE_PARAM].value + inputs[DRIVE_INPUT].value / 10.0;
	float gain = powf(100.0, drive);
	input *= gain;
	// Add -60dB noise to bootstrap self-oscillation
	input += 1.0e-6 * (2.0*randomf() - 1.0);

	// Set resonance
	float res = params[RES_PARAM].value + inputs[RES_INPUT].value / 5.0;
	res = 5.5 * clampf(res, 0.0, 1.0);
	filter.resonance = res;

	// Set cutoff frequency
	float cutoffExp = params[FREQ_PARAM].value + params[FREQ_CV_PARAM].value * inputs[FREQ_INPUT].value / 5.0;
	cutoffExp = clampf(cutoffExp, 0.0, 1.0);
	const float minCutoff = 15.0;
	const float maxCutoff = 8400.0;
	filter.cutoff = minCutoff * powf(maxCutoff / minCutoff, cutoffExp);

	// Push a sample to the state filter
	filter.process(input, 1.0/engineGetSampleRate());

	// Set outputs
	outputs[LPF_OUTPUT].value = 5.0 * filter.state[3];
	outputs[HPF_OUTPUT].value = 5.0 * (input - filter.state[3]);
}


VCFWidget::VCFWidget() {
	VCF *module = new VCF();
	setModule(module);
	box.size = Vec(15*8, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/VCF.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(createParam<RoundHugeBlackKnob>(Vec(33, 61), module, VCF::FREQ_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<RoundBlackKnob>(Vec(12, 143), module, VCF::FINE_PARAM, 0.0, 1.0, 0.5));
	addParam(createParam<RoundBlackKnob>(Vec(71, 143), module, VCF::RES_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<RoundBlackKnob>(Vec(12, 208), module, VCF::FREQ_CV_PARAM, -1.0, 1.0, 0.0));
	addParam(createParam<RoundBlackKnob>(Vec(71, 208), module, VCF::DRIVE_PARAM, 0.0, 1.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(10, 276), module, VCF::FREQ_INPUT));
	addInput(createInput<PJ301MPort>(Vec(48, 276), module, VCF::RES_INPUT));
	addInput(createInput<PJ301MPort>(Vec(85, 276), module, VCF::DRIVE_INPUT));
	addInput(createInput<PJ301MPort>(Vec(10, 320), module, VCF::IN_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(48, 320), module, VCF::LPF_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(85, 320), module, VCF::HPF_OUTPUT));
}
