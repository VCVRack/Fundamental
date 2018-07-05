// LICENSE TERMS: Copyright 2012 Teemu Voipio
//
// You can use this however you like for pretty much any purpose,
// as long as you don't claim you wrote it. There is no warranty.
//
// Distribution of substantial portions of this code in source form
// must include this copyright notice and list of conditions.
//
// Improved by Ivan COHEN from musical entropy
// More about this algorithm here :
// http://www.kvraudio.com/forum/viewtopic.php?t=349859
// http://www.kvraudio.com/forum/viewtopic.php?t=207647


#include "Fundamental.hpp"


// The clipping function of a transistor pair is approximately tanh(x)
// This one is a Pade-approx for tanh(sqrt(x))/sqrt(x)
inline float clip(float x) {
    float a = x*x;        
    return ((a + 105)*a + 945) / ((15*a + 420)*a + 945);
}

struct LadderFilter {
    float g = 0.1f;
    float resonance = 0.5f;
    float state[4] = {};
    float zi = 0.f;
    float output[3] = {};

    void process(float input) {
        // input with half delay, for non-linearities
        const float ih = 0.5f * (input + zi);

        // evaluate the non-linear gains
        const float t0 = g * clip(ih - resonance * state[3]);
        const float t1 = g * clip(state[0]);
        const float t2 = g * clip(state[1]);
        const float t3 = g * clip(state[2]);
        const float t4 = g * clip(state[3]);

        // update last LP1 output
	const float t2t3 = t2*t3;
	
        float y3 = (s[3]*(1+t3) + s[2]*t3)*(1+t2);
        y3 = (y3 + t2t3*s[1])*(1+t1);
        y3 = (y3 + t1*t2t3*(s[0]+t0*input));
        y3 = y3 / ((1+t1)*(1+t2)*(1+t3)*(1+t4) + resonance*t0*t1*t2t3);

        // update other LP1 outputs 
        const float xx = t0 * (input - resonance * y3);
        const float y0 = t1 * (s[0] + xx) / (1+t1);         
        const float y1 = t2 * (s[1] + y0) / (1+t2);
        const float y2 = t3 * (s[2] + y1) / (1+t3);

        // update states
        s[0] += 2 * (xx - y0);
        s[1] += 2 * (y0 - y1);
        s[2] += 2 * (y1 - y2);
        s[3] += 2 * (y2 - t4*y3);

        // returns LP, HP and BP outputs
	float y1t2 = y1/t2;
	float y2t3 = y2/t3;
	    
        output[0] = y3;
        output[1] = xx/t0 - 4*y0/t1 + 6*y1t2 - 4*y2t3 + y3;
        output[2] = y1t2 - 2*y2t3 + y3;

        // update delay input state
        zi = input;
    }

    void reset() {
        for (int i = 0; i < 4; i++) {
            state[i] = 0.0f;
        }
        zi = 0.f;
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
    void onReset() override {
        filter.reset();
    }
};


void VCF::step() {
    float input = inputs[IN_INPUT].value / 5.0f;
    float drive = params[DRIVE_PARAM].value + inputs[DRIVE_INPUT].value / 10.0f;
    float gain = powf(100.0f, drive);
    input *= gain;
    // Add -60dB noise to bootstrap self-oscillation
    input += 1e-6f * (2.0f*randomUniform() - 1.0f);

    // Set resonance
    float res = params[RES_PARAM].value + inputs[RES_INPUT].value / 5.0f;
    res = clamp(res, 0.0f, 1.0f);   // resonance must be between 0 and 1
    filter.resonance = res;

    // Set cutoff frequency
    float cutoffExp = params[FREQ_PARAM].value + params[FREQ_CV_PARAM].value * inputs[FREQ_INPUT].value / 5.0f;
    cutoffExp = clamp(cutoffExp, 0.0f, 1.0f);
    const float minCutoff = 15.0f;
    const float maxCutoff = 20000.0f;
    float cutoff = minCutoff * powf(maxCutoff / minCutoff, cutoffExp);
    filter.g = tanf(float_Pi * cutoff / engineGetSampleRate());

    // Push a sample to the state filter
    filter.process(input);

    // Set outputs
    outputs[LPF_OUTPUT].value = 5.0f * filter.output[0];
    outputs[HPF_OUTPUT].value = 5.0f * filter.output[1];
    //outputs[BPF_OUTPUT].value = 5.0f * filter.output[2];
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

    addParam(ParamWidget::create<RoundHugeBlackKnob>(Vec(33, 61), module, VCF::FREQ_PARAM, 0.0f, 1.0f, 0.5f));
    addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(12, 143), module, VCF::FINE_PARAM, 0.0f, 1.0f, 0.5f));
    addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(71, 143), module, VCF::RES_PARAM, 0.0f, 1.0f, 0.0f));
    addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(12, 208), module, VCF::FREQ_CV_PARAM, -1.0f, 1.0f, 0.0f));
    addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(71, 208), module, VCF::DRIVE_PARAM, 0.0f, 1.0f, 0.0f));

    addInput(Port::create<PJ301MPort>(Vec(10, 276), Port::INPUT, module, VCF::FREQ_INPUT));
    addInput(Port::create<PJ301MPort>(Vec(48, 276), Port::INPUT, module, VCF::RES_INPUT));
    addInput(Port::create<PJ301MPort>(Vec(85, 276), Port::INPUT, module, VCF::DRIVE_INPUT));
    addInput(Port::create<PJ301MPort>(Vec(10, 320), Port::INPUT, module, VCF::IN_INPUT));

    addOutput(Port::create<PJ301MPort>(Vec(48, 320), Port::OUTPUT, module, VCF::LPF_OUTPUT));
    addOutput(Port::create<PJ301MPort>(Vec(85, 320), Port::OUTPUT, module, VCF::HPF_OUTPUT));
}


Model *modelVCF = Model::create<VCF, VCFWidget>("Fundamental", "VCF", "VCF", FILTER_TAG);
