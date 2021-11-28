#pragma once

#include "Filters/svf.h"
#include "Utility/delayline.h"
#include "Utility/dsp.h"

using namespace daisysp;

#define MAX_DELAY static_cast<size_t>(48000 * 1.f)

namespace orchard
{
    constexpr int kPoles{3};

    struct Pole
    {
        DelayLine<float, MAX_DELAY> *del;
        float currentDelay;
        float delayTarget;

        Svf filt;

        float damp{0.f};   // 0.0 and sample_rate / 3
        float reso{0.f};   // 0.0 : 0.4
        float decay{0.f};  // 0.0 : ?
        float detune{0.f}; // 0.0 : 0.07
        float pitch{60.f};

        void Init(float sampleRate, DelayLine<float, MAX_DELAY> *del)
        {
            //resoPoleDelayLine.Init();
            //del = &resoPoleDelayLine;

            filt.Init(sampleRate);
            filt.SetFreq(damp);
            filt.SetRes(reso);

            pitch *= -0.5017f;
            pitch += 17.667f;
            pitch -= detune;
            delayTarget = pow10f(pitch / 20.0f); // ms
            delayTarget *= sampleRate * 0.001f;  // ms to samples ?
        }

        float Process(float in)
        {
            fonepole(currentDelay, delayTarget, .0002f);
            del->SetDelay(currentDelay);

            float sig = del->Read();
            filt.Process(sig);
            sig = filt.Low();
            del->Write((decay * sig) + in);

            return sig;
        }
    };

    class Resonator
    {

    public:
        Resonator() {}
        ~Resonator() {}

        void Init(float sampleRate, DelayLine<float, MAX_DELAY> *del)
        {
            for (int i = 0; i < kPoles; i++)
            {
                poles_[i].Init(sampleRate, del);
            }
        }
        void SetDamp(float damp) { damp_ = damp; }
        void SetReso(float reso) { reso_ = reso; }
        void SetDecay(float decay) { decay_ = decay; }
        void SetDetune(float detune) { detune_ = detune; }
        void SetPitch(int pole, float pitch) { pitches_[pole] = pitch; }

        void Process(float &left, float &right) {
            
        }

    private:
        Pole poles_[kPoles];
        float damp_{0.f};   // 0.0 and sample_rate / 3
        float reso_{0.f};   // 0.0 : 0.4
        float decay_{0.f};  // 0.0 : ?
        float detune_{0.f}; // 0.0 : 0.07
        float pitches_[kPoles];
    };
}