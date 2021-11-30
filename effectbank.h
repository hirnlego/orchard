#pragma once

#include "Filters/svf.h"
#include "Effects/reverbsc.h"
#include "Utility/delayline.h"
#include "Utility/dsp.h"

#include "resonator.h"


#define MAX_DELAY static_cast<size_t>(48000 * 1.f)

using namespace daisysp;

namespace orchard
{   
    ReverbSc DSY_SDRAM_BSS reverb;
    DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS leftDelayLine;
    DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS rightDelayLine;


    DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS leftResoPoleDelayLine[3];
    DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS rightResoPoleDelayLine[3];

    struct delay
    {
        DelayLine<float, MAX_DELAY> *del;
        float currentDelay;
        float delayTarget;

        void Reset()
        {
            del->Reset();
        }

        float Process(float feedback, float in)
        {
            fonepole(currentDelay, delayTarget, .0002f);
            del->SetDelay(currentDelay);

            float read = del->Read();
            del->Write((feedback * read) + in);

            return read;
        }
    }; 
    
    struct EffectConf
    {
        bool active;
        float dryWet;
        float param1;
    };

    enum class FilterType
    {
        HP,
        LP,
        BP,
    };
 
    class EffectBank
    {
    public:
        EffectBank() {}
        ~EffectBank() {}

        void Init(float sampleRate)
        {  
            leftFilter.Init(sampleRate);
            rightFilter.Init(sampleRate);

            for (int i = 0; i < 3; i++)
            {
                leftResoPoleDelayLine[i].Init();
                rightResoPoleDelayLine[i].Init();
            }
            resonator.Init(sampleRate);
            resonator.AddPole(&leftResoPoleDelayLine[0], &rightResoPoleDelayLine[0]);
            resonator.AddPole(&leftResoPoleDelayLine[1], &rightResoPoleDelayLine[1]);
            resonator.AddPole(&leftResoPoleDelayLine[2], &rightResoPoleDelayLine[2]);

            leftDelayLine.Init();
            rightDelayLine.Init();
            leftDelay.del = &leftDelayLine;
            rightDelay.del = &rightDelayLine;

            reverb.Init(sampleRate);
        }

        void Randomize()
        {
            // Filter.
            effectsConf[0].active = true; //1 == std::rand() % 2;
            if (effectsConf[0].active) {
                effectsConf[0].dryWet = RandomFloat(0.f, 1.f);
                filterType = static_cast<FilterType>(std::rand() % 3);
                int pitch;
                switch (filterType)
                {
                case FilterType::HP:
                    pitch = RandomPitch(Range::HIGH);
                    break;

                case FilterType::LP:
                    pitch = RandomPitch(Range::LOW);
                    break;

                case FilterType::BP:
                    pitch = RandomPitch(Range::FULL);
                    break;

                default:
                    break;
                }
                float freq = mtof(pitch);
                float res{ RandomFloat(0.f, 1.f) };
                float drive{ RandomFloat(0.f, 1.f) };
                leftFilter.SetFreq(freq);
                leftFilter.SetRes(res);
                leftFilter.SetDrive(drive);
                rightFilter.SetFreq(freq);
                rightFilter.SetRes(res);
                rightFilter.SetDrive(drive);
            }

            // Resonator.
            effectsConf[1].active = true; //1 == std::rand() % 2;
            if (effectsConf[1].active) {
                effectsConf[1].dryWet = RandomFloat(0.f, 1.f);
                resonator.SetDecay(RandomFloat(0.f, 0.4f));
                resonator.SetDetune(RandomFloat(0.f, 0.1f));
                resonator.SetReso(RandomFloat(0.f, 0.4f));
                resonator.SetPitch(0, RandomPitch(Range::FULL));
                resonator.SetPitch(1, RandomPitch(Range::FULL));
                resonator.SetPitch(2, RandomPitch(Range::FULL));
                resonator.SetDamp(RandomFloat(100.f, 5000.f));
            /*
            effectsConf[1].dryWet = 1.f;
            resonator.SetDecay(0.4f);
            resonator.SetDetune(0.f);
            resonator.SetReso(0.4f);
            resonator.SetPitch(0, 60.f);
            resonator.SetPitch(1, 60.f);
            resonator.SetPitch(2, 60.f);
            resonator.SetDamp(500.f);
            */
            }

            // Delay.
            effectsConf[2].active = false; //1 == std::rand() % 2;
            if (effectsConf[2].active) {
                leftDelay.Reset();
                rightDelay.Reset();
                effectsConf[2].dryWet = RandomFloat(0.f, 1.f);
                effectsConf[2].param1 = RandomFloat(0.f, 0.9f);
                leftDelay.delayTarget = RandomFloat(sampleRate * .05f, MAX_DELAY);
                rightDelay.delayTarget = RandomFloat(sampleRate * .05f, MAX_DELAY);
            }

            // Reverb.
            effectsConf[3].active = true; //1 == std::rand() % 2;
            if (effectsConf[3].active) {
                effectsConf[3].dryWet = RandomFloat(0.f, 1.f);
                float fb{RandomFloat(0.f, 0.9f)};
                reverb.SetFeedback(fb);
                reverb.SetLpFreq(RandomFloat(0.f, 5000.f));
            }
        }

        void Process(float &left, float &right)
        {
            // Effects.
            float leftW{ 0.f };
            float rightW{ 0.f };

            // Filter.
            if (effectsConf[0].active) {
                leftFilter.Process(left);
                rightFilter.Process(right);
                switch (filterType)
                {
                case FilterType::LP:
                    leftW = leftFilter.Low();
                    rightW = rightFilter.Low();
                    break;

                case FilterType::HP:
                    leftW = leftFilter.High();
                    rightW = rightFilter.High();
                    break;

                case FilterType::BP:
                    leftW = leftFilter.Band();
                    rightW = rightFilter.Band();
                    break;

                default:
                    break;
                }
                left = effectsConf[0].dryWet * leftW * .3f + (1.0f - effectsConf[0].dryWet) * left;
                right = effectsConf[0].dryWet * rightW * .3f + (1.0f - effectsConf[0].dryWet) * right;
            }

            // Resonator.
            if (effectsConf[1].active) {
                leftW = left;
                rightW = right;
                resonator.Process(leftW, rightW);
                left = effectsConf[1].dryWet * SoftClip(leftW) * .3f + (1.0f - effectsConf[1].dryWet) * left;
                right = effectsConf[1].dryWet * SoftClip(rightW) * .3f + (1.0f - effectsConf[1].dryWet) * right;
            }

            // Delay.
            if (effectsConf[2].active) {
                leftW = leftDelay.Process(effectsConf[2].param1, left);
                rightW = rightDelay.Process(effectsConf[2].param1, right);
                left = effectsConf[2].dryWet * leftW * .3f + (1.0f - effectsConf[2].dryWet) * left;
                right = effectsConf[2].dryWet * rightW * .3f + (1.0f - effectsConf[2].dryWet) * right;
            }

            // Reverb.
            if (effectsConf[3].active) {
                reverb.Process(left, right, &leftW, &rightW);
                left = effectsConf[3].dryWet * leftW * .3f + (1.0f - effectsConf[3].dryWet) * left;
                right = effectsConf[3].dryWet * rightW * .3f + (1.0f - effectsConf[3].dryWet) * right;
            }
        }

    private:            
        Svf leftFilter;
        Svf rightFilter;
        delay leftDelay;
        delay rightDelay;
        Resonator resonator;
        EffectConf effectsConf[4];
        FilterType filterType;
    };
}