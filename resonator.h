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
        DelayLine<float, MAX_DELAY> *leftDel;
        DelayLine<float, MAX_DELAY> *rightDel;
        float currentDelay;
        float delayTarget;

        Svf filt;

        float sampleRate_;
        float decay_{0.f};  // 0.0 : ?
        float detune_{0.f}; // 0.0 : 0.07
        float pitch_{60.f};

        void Init(float sampleRate, DelayLine<float, MAX_DELAY> *lDel, DelayLine<float, MAX_DELAY> *rDel)
        {
            leftDel = lDel;
            rightDel = rDel;
            sampleRate_ = sampleRate;
            filt.Init(sampleRate_);
        }

        void SetDetune(float detune)
        {
            detune_ = detune;
            pitch_ -= detune_;
            delayTarget = pow10f(pitch_ / 20.0f); // ms
            delayTarget *= sampleRate_ * 0.001f;  // ms to samples ?
        }

        void SetDamp(float damp)
        {
            filt.SetFreq(pitch_ + damp);
        }

        void SetPitch(float pitch)
        {
            pitch_ = pitch;
            pitch_ *= -0.5017f;
            pitch_ += 17.667f;
            pitch_ -= detune_;
            delayTarget = pow10f(pitch_ / 20.0f); // ms
            delayTarget *= sampleRate_ * 0.001f; // ms to samples ?
        }

        void Process(float &left, float &right)
        {
            fonepole(currentDelay, delayTarget, .0002f);
            leftDel->SetDelay(currentDelay);
            rightDel->SetDelay(currentDelay);

            float leftW = leftDel->Read();
            float rightW = rightDel->Read();
            filt.Process(leftW);
            leftW = filt.Low();
            filt.Process(rightW);
            rightW = filt.Low();
            leftDel->Write((decay_ * leftW) + left);
            rightDel->Write((decay_ * rightW) + right); 
            left = leftW;
            right = rightW;
        }
    };

    class Resonator
    {

    public:
        Resonator() {}
        ~Resonator() {}

        void Init(float sampleRate, DelayLine<float, MAX_DELAY> (*leftDel)[3], DelayLine<float, MAX_DELAY> (*rightDel)[3])
        {
            for (int i = 0; i < kPoles; i++)
            {
                poles_[i].Init(sampleRate, leftDel[i], rightDel[i]);
            }
        }
        void SetDamp(float damp) 
        {
            for (int i = 0; i < kPoles; i++)
            {
                poles_[i].SetDamp(damp);
            }
        }
        void SetReso(float reso) 
        {
            for (int i = 0; i < kPoles; i++)
            {
                poles_[i].filt.SetRes(reso);
            }
        }
        void SetDecay(float decay) 
        {
            for (int i = 0; i < kPoles; i++)
            {
                poles_[i].decay_ = decay;
            }
        }
        void SetDetune(float detune) 
        {
            for (int i = 0; i < kPoles; i++)
            {
                poles_[i].SetDetune(detune);
            }
        }
        void SetPitch(int pole, float pitch) 
        {
            poles_[pole].SetPitch(pitch);
        }

        void Process(float &left, float &right) 
        {
            for (int i = 0; i < kPoles; i++)
            {
                poles_[i].Process(left, right);
            }
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