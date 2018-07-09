#include "Fundamental.hpp"
#include "dsp/functions.hpp"
#include "dsp/resampler.hpp"


// ===================================================================================
/**
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
struct LadderFilterRK4
{
public:
	// ===================================================================================
	void process(float input, float dt, float &outLP, float &outHP) 
	{
		float deriv1[4], deriv2[4], deriv3[4], deriv4[4], tempState[4];

		calculateDerivatives(input, deriv1, state);

		for (int i = 0; i < 4; i++)
			tempState[i] = state[i] + 0.5f * dt * deriv1[i];
		calculateDerivatives(input, deriv2, tempState);

		for (int i = 0; i < 4; i++)
			tempState[i] = state[i] + 0.5f * dt * deriv2[i];
		calculateDerivatives(input, deriv3, tempState);
		
		for (int i = 0; i < 4; i++)
			tempState[i] = state[i] + dt * deriv3[i];
		calculateDerivatives(input, deriv4, tempState);

		for (int i = 0; i < 4; i++)
			state[i] += (1.0f / 6.0f) * dt * (deriv1[i] + 2.0f * deriv2[i] + 2.0f * deriv3[i] + deriv4[i]);

		outLP = state[3];
		outHP = clip((input - resonance*state[3]) - 4 * state[0] + 6*state[1] - 4*state[2] + state[3]);
	}

	void reset() 
	{
		for (int i = 0; i < 4; i++)
			state[i] = 0.0f;
	}

	// ===================================================================================
	// parameters
	float cutoff = 1000.0f;
	float resonance = 1.0f;

private:
	// ===================================================================================
	// The clipping function of a transistor pair is approximately tanh(x)
	inline float clip(float x) 
	{
	       return std::tanh(x);
	}

	void calculateDerivatives(float input, float *dstate, const float *_state) 
	{
		float cutoff2Pi = 2 * M_PI * cutoff;

		float satstatein = clip(input - resonance * _state[3]);
		float satstate0  = clip(_state[0]);
		float satstate1  = clip(_state[1]);
		float satstate2  = clip(_state[2]);
		float satstate3  = clip(_state[3]);

		dstate[0] = cutoff2Pi * (satstatein - satstate0);
		dstate[1] = cutoff2Pi * (satstate0  - satstate1);
		dstate[2] = cutoff2Pi * (satstate1  - satstate2);
		dstate[3] = cutoff2Pi * (satstate2  - satstate3);
	}

	// ===================================================================================
	// state variables
	float state[4] = {};
};


// ===================================================================================
/** 
	Same than before but with oversampling x4
*/
struct LadderFilterRK4Oversampled
{
public:
	// ===================================================================================
	void process(float input, float dt, float &outLP, float &outHP) 
	{
		// upsampling
		float inup[4], outLPup[4], outHPup[4];
		upsampler.process(input, inup);

		// processing
		float deriv1[4], deriv2[4], deriv3[4], deriv4[4], tempState[4];

		for (int n = 0; n < 4; n++)
		{
			for (int i = 0; i < 4; i++)
				tempState[i] = state[i];			
			calculateDerivatives(inup[n], deriv1, tempState);

			for (int i = 0; i < 4; i++)
				tempState[i] = state[i] + 0.5f * dt * 0.25f * deriv1[i];
			calculateDerivatives(inup[n], deriv2, tempState);

			for (int i = 0; i < 4; i++)
				tempState[i] = state[i] + 0.5f * dt * 0.25f * deriv2[i];
			calculateDerivatives(inup[n], deriv3, tempState);
			
			for (int i = 0; i < 4; i++)
				tempState[i] = state[i] + dt * 0.25f * deriv3[i];
			calculateDerivatives(inup[n], deriv4, tempState);

			for (int i = 0; i < 4; i++)
				state[i] += (1.0f / 6.0f) * dt * 0.25f * (deriv1[i] + 2.0f * deriv2[i] + 2.0f * deriv3[i] + deriv4[i]);

			outLPup[n] = state[3];
			outHPup[n] = clip((inup[n] - resonance*state[3]) - 4 * state[0] + 6*state[1] - 4*state[2] + state[3]);
		}

		// downsampling
		outLP = decimatorLP.process(outLPup);
		outHP = decimatorHP.process(outHPup);		
	}

	void reset() 
	{
		for (int i = 0; i < 4; i++)
			state[i] = 0.0f;

		upsampler.reset();
		decimatorLP.reset();
		decimatorHP.reset();
	}

	// ===================================================================================
	// parameters
	float cutoff = 1000.0f;
	float resonance = 1.0f;

private:
	// ===================================================================================
	// The clipping function of a transistor pair is approximately tanh(x)
	inline float clip(float x) 
	{
	       return std::tanh(x);
	}

	void calculateDerivatives(float input, float *dstate, const float *_state) 
	{
		float cutoff2Pi = 2 * M_PI * cutoff;

		float satstatein = clip(input - resonance * _state[3]);
		float satstate0  = clip(_state[0]);
		float satstate1  = clip(_state[1]);
		float satstate2  = clip(_state[2]);
		float satstate3  = clip(_state[3]);

		dstate[0] = cutoff2Pi * (satstatein - satstate0);
		dstate[1] = cutoff2Pi * (satstate0  - satstate1);
		dstate[2] = cutoff2Pi * (satstate1  - satstate2);
		dstate[3] = cutoff2Pi * (satstate2  - satstate3);
	}

	// ===================================================================================
	// oversampling classes
	Upsampler<4, 32> upsampler;
	Decimator<4, 24> decimatorLP, decimatorHP;

	// state variables
	float state[4] = {};
};

// ===================================================================================
/**
	This code is derived from the algorithm described in "Modeling and Measuring a 
	Moog Voltage-Controlled Filter, by Effrosyni Paschou, Fabián Esqueda, Vesa Välimäki 
	and John Mourjopoulos, for APSIPA Annual Summit and Conference 2017.

	It is a 0df algorithm using Newton-Raphson's method (4 iterations) for solving 
	implicit equations, and midpoint integration method.

	Derived from the article and adapted to VCV Rack by Ivan COHEN.
*/
struct LadderFilter0dfMidPoint 
{
public:
	// ===================================================================================
        // parameters
        float cutoff = 1000.f;
        float resonance = 1.0f;
    
        LadderFilter0dfMidPoint() 
        {
                A = gainfactor / (2.f * VT);
        }
        
        void process(float input, float dt, float &outLP, float &outHP) 
        {
                float F[4], V[4];
                const float wc = 2*M_PI*cutoff / A;        

                input *= gainfactor;

                for(int i = 0; i < 4; i++)
                	V[i] = lastV[i];

                for(int i = 0; i < 4; i++)
                {
                	const float S0 = A * (input + lastinput + resonance * (V[3] + lastV[3])) * 0.5f;
                	const float S1 = A * (V[0] + lastV[0]) * 0.5f;
                	const float S2 = A * (V[1] + lastV[1]) * 0.5f;
                	const float S3 = A * (V[2] + lastV[2]) * 0.5f;
                	const float S4 = A * (V[3] + lastV[3]) * 0.5f;

                	const float t0 = clip(S0);
                	const float t1 = clip(S1);
                	const float t2 = clip(S2);
                	const float t3 = clip(S3);
                	const float t4 = clip(S4);

                	// F function (the one for which we need to find the roots with NR)
                	F[0] = V[0] - lastV[0] + wc*dt*(t0 + t1);
                	F[1] = V[1] - lastV[1] - wc*dt*(t1 - t2);
                	F[2] = V[2] - lastV[2] - wc*dt*(t2 - t3);
                	F[3] = V[3] - lastV[3] - wc*dt*(t3 - t4);

                	const float g = wc*dt*A*0.5f;

                	// derivatives of ti
                	const float J0 = 1 - t0 * t0;
                	const float J1 = 1 - t1 * t1;
                	const float J2 = 1 - t2 * t2;
                	const float J3 = 1 - t3 * t3;
                	const float J4 = 1 - t4 * t4;

                	// Jacobian matrix elements
                	const float a11 = 1 + g*J1;
                	const float a14 = resonance*g*J0;
                	const float a21 = -g*J1;
                	const float a22 = 1 + g*J2;
                	const float a32 = -g*J2;
                	const float a33 = 1 + g*J3;
                	const float a43 = -g*J3;
                	const float a44 = 1 + g*J4;

                	// Newton-Raphson algorithm with Jacobian inverting
                	const float deninv = 1.f / (a11*a22*a33*a44 - a14*a21*a32*a43);

                	float delta0 = ( F[0]*a22*a33*a44 - F[1]*a14*a32*a43 + F[2]*a14*a22*a43 - F[3]*a14*a22*a33) * deninv;
                	float delta1 = (-F[0]*a21*a33*a44 + F[1]*a11*a33*a44 - F[2]*a14*a21*a43 + F[3]*a14*a21*a33) * deninv;
                	float delta2 = ( F[0]*a21*a32*a44 - F[1]*a11*a32*a44 + F[2]*a11*a22*a44 - F[3]*a14*a21*a32) * deninv;
                	float delta3 = (-F[0]*a21*a32*a43 + F[1]*a11*a32*a43 - F[2]*a11*a22*a43 + F[3]*a11*a22*a33) * deninv;

                	delta0 = isfinite(delta0) ? delta0 : 0.f;
                	delta1 = isfinite(delta1) ? delta1 : 0.f;
                	delta2 = isfinite(delta2) ? delta2 : 0.f;
                	delta3 = isfinite(delta3) ? delta3 : 0.f;

                	V[0] -= delta0;
                	V[1] -= delta1;
                	V[2] -= delta2;
                	V[3] -= delta3;
                }

                lastinput = input;
                for(int i = 0; i < 4; i++)
                	lastV[i] = V[i];

                // outputs are inverted
                outLP = -clip(V[3] / gainfactor);
                outHP = -clip(((input + resonance*V[3]) + 4 * V[0] - 6*V[1] + 4*V[2] - V[3]) / gainfactor);
        }

        void reset() 
        {
                for (int i = 0; i < 4; i++)
                        lastV[i] = 0.0f;

                lastinput = 0.f;
        }

private:
	// ===================================================================================
	inline float clip(float x) 
	{
		return std::tanh(x);
	}

	// ===================================================================================
	// constants
	float VT = 26e-3f;
	float gainfactor = 0.2f;
	float A;

	// state variables
	float lastinput = 0.f;
	float lastV[4] = {};
};


// ===================================================================================
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

        //LadderFilter0dfMidPoint filter;
        LadderFilterRK4Oversampled filter;
        //LadderFilterRK4 filter;

        VCF() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
        void step() override;
        void onReset() override {
                filter.reset();
        }
};


void VCF::step() 
{
        // input stage
        float input = inputs[IN_INPUT].value / 5.f;
        float gain = std::pow(1.f + params[DRIVE_PARAM].value, 5);

        if (inputs[DRIVE_INPUT].active)
                gain *= inputs[DRIVE_INPUT].value / 10.f;
            
        input *= gain;
        // Add -60dB noise to bootstrap self-oscillation
        input += 1e-6f * (2.f*randomUniform() - 1.f);

        // Set resonance
        float res = clamp(params[RES_PARAM].value + inputs[RES_INPUT].value / 10.f, 0.f, 1.f);
        filter.resonance = std::pow(res, 2) * 10.f;

        // Set cutoff frequency
        float pitch = inputs[FREQ_INPUT].value * quadraticBipolar(params[FREQ_CV_PARAM].value);
        pitch += params[FREQ_PARAM].value * 10.f - 3.f;
        pitch += quadraticBipolar(params[FINE_PARAM].value * 2.f - 1.f) * 7.f/12.f;
        float cutoff = 261.626f * std::pow(2.f, pitch);
        filter.cutoff = clamp(cutoff, 1.f, 20000.f);
            
        // Push a sample to the state filter
        float outLP, outHP;
        filter.process(input, engineGetSampleTime(), outLP, outHP);

        // Set outputs
        outputs[LPF_OUTPUT].value = 5.0f * outLP;
        outputs[HPF_OUTPUT].value = 5.0f * outHP;
}


struct VCFWidget : ModuleWidget {
	VCFWidget(VCF *module);
};

VCFWidget::VCFWidget(VCF *module) : ModuleWidget(module) {
	setPanel(SVG::load(assetPlugin(plugin, "res/VCF.svg")));

	addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(ParamWidget::create<RoundHugeBlackKnob>(Vec(33, 61), module, VCF::FREQ_PARAM, 0.f, 1.f, 0.5f));
	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(12, 143), module, VCF::FINE_PARAM, 0.f, 1.f, 0.5f));
	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(71, 143), module, VCF::RES_PARAM, 0.f, 1.f, 0.f));
	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(12, 208), module, VCF::FREQ_CV_PARAM, -1.f, 1.f, 0.f));
	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(71, 208), module, VCF::DRIVE_PARAM, 0.f, 1.f, 0.f));

	addInput(Port::create<PJ301MPort>(Vec(10, 276), Port::INPUT, module, VCF::FREQ_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(48, 276), Port::INPUT, module, VCF::RES_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(85, 276), Port::INPUT, module, VCF::DRIVE_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(10, 320), Port::INPUT, module, VCF::IN_INPUT));

	addOutput(Port::create<PJ301MPort>(Vec(48, 320), Port::OUTPUT, module, VCF::LPF_OUTPUT));
	addOutput(Port::create<PJ301MPort>(Vec(85, 320), Port::OUTPUT, module, VCF::HPF_OUTPUT));
}


Model *modelVCF = Model::create<VCF, VCFWidget>("Fundamental", "VCF", "VCF", FILTER_TAG);
