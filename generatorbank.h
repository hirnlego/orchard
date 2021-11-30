#pragma once

#include "Synthesis/blosc.h"
#include "Synthesis/oscillator.h"
#include "Synthesis/variablesawosc.h"
#include "Synthesis/variableshapeosc.h"
#include "Noise/whitenoise.h"
#include "Filters/atone.h"
#include "Filters/tone.h"
#include "Control/adsr.h"
#include "Utility/dsp.h"

using namespace daisysp;

namespace orchard
{    
    constexpr int kGenerators{ 9 };

    
    struct GeneratorConf
    {
        bool active;
        float volume;
        float pan;
        int pitch;
        int interval;
        float character;
        int ringSource;
        float ringAmt;

    };


    class GeneratorBank
    {

    public:
        GeneratorBank() {}
        ~GeneratorBank() {}

        void Init(float sampleRate)
        {        
            hOsc1.Init(sampleRate);
            hOsc1.SetWaveform(Oscillator::WAVE_SIN);
            hOsc1.SetAmp(1.f);

            hOsc2.Init(sampleRate);

            hOsc3.Init(sampleRate);
            hOsc3.SetWaveform(BlOsc::WAVE_TRIANGLE);
            hOsc3.SetAmp(1.f);

            hOsc4.Init(sampleRate);
            hOsc4.SetWaveform(BlOsc::WAVE_SQUARE);
            hOsc4.SetAmp(1.f);

            lOsc1.Init(sampleRate);
            lOsc1.SetWaveform(Oscillator::WAVE_SIN);
            lOsc1.SetAmp(1.f);

            lOsc2.Init(sampleRate);

            lOsc3.Init(sampleRate);
            lOsc3.SetWaveform(BlOsc::WAVE_TRIANGLE);
            lOsc3.SetAmp(1.f);

            lOsc4.Init(sampleRate);
            lOsc4.SetWaveform(BlOsc::WAVE_SQUARE);
            lOsc4.SetAmp(1.f);

            noise.Init();
            noise.SetAmp(1.f);
            noiseFilterHP.Init(sampleRate);
            noiseFilterLP.Init(sampleRate);

            for (int i = 0; i < kGenerators; i++)
            {
                envelopes[i].Init(sampleRate);
            }
        }
        
        void SetCharacter(float character)
        {
            hOsc2.SetWaveshape(character);
            hOsc2.SetPW(1.f - character);
            hOsc3.SetPw(character);
            hOsc4.SetPw(character);

            lOsc2.SetWaveshape(character);
            lOsc2.SetPW(1.f - character);
            lOsc3.SetPw(character);
            lOsc4.SetPw(character);
        }

        void SetPitch(float pitch)
        {
            hOsc1.SetFreq(CalcFrequency(0, pitch));
            hOsc2.SetFreq(CalcFrequency(2, pitch));
            hOsc3.SetFreq(CalcFrequency(4, pitch));
            hOsc4.SetFreq(CalcFrequency(6, pitch));

            lOsc1.SetFreq(CalcFrequency(1, pitch));
            lOsc2.SetFreq(CalcFrequency(3, pitch));
            lOsc3.SetFreq(CalcFrequency(5, pitch));
            lOsc4.SetFreq(CalcFrequency(7, pitch));

            float f{CalcFrequency(8, pitch)};
            noiseFilterHP.SetFreq(f);
            noiseFilterLP.SetFreq(f);
        }
        
        void Randomize()
        {
            //float volume{ 0.f };
            int actives{ 0 };
            int half{ kGenerators / 2 };
            for (int i = 0; i < kGenerators; i++)
            {
                bool active{ 1 == std::rand() % 2 };
                // Limit the number of inactive generators to half of their total number.
                if (i >= half && !active && actives < half) {
                    active = true;
                }
                generatorsConf[i].active = active;
                if (active) {
                    ++actives;
                }
                generatorsConf[i].pan = RandomFloat(0.3f, 0.7f);

                if (i < kGenerators - 1) 
                {
                    if (i % 2 == 0)
                    {
                        generatorsConf[i].interval = RandomInterval(Range::HIGH);
                    }
                    else
                    {
                        generatorsConf[i].interval = RandomInterval(Range::LOW);
                    }
                }
                else
                {
                    generatorsConf[i].interval = RandomInterval(Range::FULL);
                }

                envelopes[i].SetTime(ADSR_SEG_ATTACK, RandomFloat(0.f, 2.f));
                envelopes[i].SetTime(ADSR_SEG_DECAY, RandomFloat(0.f, 2.f));
                envelopes[i].SetTime(ADSR_SEG_RELEASE, RandomFloat(0.f, 2.f));
                envelopes[i].SetSustainLevel(RandomFloat(0.f, 1.f));

                /*
                envelopes[i].SetAttackTime(0.1f);
                envelopes[i].SetDecayTime(0.1f);
                envelopes[i].SetSustainLevel(1.f);
                envelopes[i].SetReleaseTime(0.1f);
                */

                //generatorsConf[i].ringSource = std::floor(RandomFloat(0.f, kGenerators - 1));
            }
            for (int i = 0; i < kGenerators; i++)
            {
                if (generatorsConf[i].active) {
                    generatorsConf[i].volume = 1.f / actives; //RandomFloat(0.3f, 0.5f);
                }
            }

            hOsc2.SetWaveshape(RandomFloat(0.f, 1.f));
            hOsc2.SetPW(RandomFloat(-1.f, 1.f));

            hOsc3.SetPw(RandomFloat(-1.f, 1.f));

            hOsc4.SetPw(RandomFloat(-1.f, 1.f));

            lOsc2.SetWaveshape(RandomFloat(0.f, 1.f));
            lOsc2.SetPW(RandomFloat(-1.f, 1.f));

            lOsc3.SetPw(RandomFloat(-1.f, 1.f));

            lOsc4.SetPw(RandomFloat(-1.f, 1.f));

            generatorsConf[8].character = RandomFloat(1.f, 2.f);

            SetPitch(basePitch);
        }

        void Process(float &left, float &right)
        {
            // Process all generators.
            float sigs[kGenerators];
            for (int i = 0; i < kGenerators; i++)
            {
                if (generatorsConf[i].active) 
                {
                    if (i == 0) {
                        sigs[i] = hOsc1.Process();
                    }
                    else if (i == 1)
                    {
                        sigs[i] = lOsc1.Process();
                    }
                    else if (i == 2)
                    {
                        sigs[i] = hOsc2.Process();
                    }
                    else if (i == 3)
                    {
                        sigs[i] = lOsc2.Process();
                    }
                    else if (i == 4)
                    {
                        sigs[i] = hOsc3.Process();
                    }
                    else if (i == 5)
                    {
                        sigs[i] = lOsc3.Process();
                    }
                    else if (i == 6)
                    {
                        sigs[i] = hOsc4.Process();
                    }
                    else if (i == 7)
                    {
                        sigs[i] = lOsc4.Process();
                    }
                    else if (i == 8)
                    {
                        sigs[i] = noise.Process();
                        sigs[i] = generatorsConf[i].character * sigs[i];
                        sigs[i] = noiseFilterHP.Process(sigs[i]);
                        sigs[i] = generatorsConf[i].character * sigs[i];
                        sigs[i] = SoftClip(noiseFilterLP.Process(sigs[i]));
                    }

                    left += sigs[i] * generatorsConf[i].volume * (1 - generatorsConf[i].pan) * envelopes[i].Process(envelopeGate);
                    right += sigs[i] * generatorsConf[i].volume * generatorsConf[i].pan * envelopes[i].Process(envelopeGate);
                }
            }

            /*
            // Apply ring modulation.
            for (int i = 0; i < kGenerators; i++)
            {
                if (generatorsConf[i].ringSource > 0 && generatorsConf[i].active && generatorsConf[generatorsConf[i].ringSource].active)
                {
                    sigs[i] = SoftClip(sigs[i] * sigs[generatorsConf[i].ringSource]);
                }
                left += sigs[i] * generatorsConf[i].volume * (1 - generatorsConf[i].pan) * envelopes[i].Process(envelopeGate);
                right += sigs[i] * generatorsConf[i].volume * generatorsConf[i].pan * envelopes[i].Process(envelopeGate);
            }
            */
        }


    private:
        float CalcFrequency(int generator, float pitch)
        {
            float midi_nn = fclamp(pitch + generatorsConf[generator].interval, 0.f, 120.f);

            return mtof(midi_nn);
        }

        Oscillator hOsc1;              // Sine
        VariableSawOscillator hOsc2;   // Bipolar ramp
        BlOsc hOsc3;                   // Triangle
        BlOsc hOsc4;                   // Square

        Oscillator lOsc1;              // Sine
        VariableSawOscillator lOsc2;   // Bipolar ramp
        BlOsc lOsc3;                   // Triangle
        BlOsc lOsc4;                   // Square

        WhiteNoise noise;
    
        Adsr envelopes[kGenerators];
        // Generators configuration:
        // 0 = hOsc1
        // 1 = lOsc1
        // 2 = hOsc2
        // 3 = lOsc2
        // 4 = hOsc3
        // 5 = lOsc3
        // 6 = hOsc4
        // 7 = lOsc4
        // 8 = noise
        GeneratorConf generatorsConf[kGenerators];
    };
}