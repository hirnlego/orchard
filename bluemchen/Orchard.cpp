#include <time.h>
#include "kxmx_bluemchen.h"
#include "Synthesis/blosc.h"
#include "Synthesis/oscillator.h"
#include "Synthesis/variablesawosc.h"
#include "Synthesis/variableshapeosc.h"
#include "Control/adsr.h"
#include "Noise/whitenoise.h"
#include "Filters/svf.h"
#include "Filters/atone.h"
#include "Filters/tone.h"
#include "Effects/reverbsc.h"
#include "Dynamics/balance.h"
#include "Utility/delayline.h"
#include "Utility/dsp.h"

#include "../resonator.h"

#define MAX_DELAY static_cast<size_t>(48000 * 1.f)

using namespace kxmx;
using namespace daisy;
using namespace daisysp;
using namespace orchard;

Bluemchen bluemchen;

Parameter knob1;
Parameter knob2;

Parameter knob1_dac;
Parameter knob2_dac;

Parameter cv1;
Parameter cv2;

float knob1Value;
float knob2Value;

Oscillator hOsc1;              // Sine
VariableSawOscillator hOsc2;   // Bipolar ramp
BlOsc hOsc3;                   // Triangle
BlOsc hOsc4;                   // Square

Oscillator lOsc1;              // Sine
VariableSawOscillator lOsc2;   // Bipolar ramp
BlOsc lOsc3;                   // Triangle
BlOsc lOsc4;                   // Square

WhiteNoise noise;

Svf leftFilter;
Svf rightFilter;

ATone noiseFilterHP;
Tone noiseFilterLP;

Balance balancer;

float sampleRate;

constexpr int kGenerators{ 9 };

Adsr envelopes[kGenerators];

bool envelopeGate{ false };

ReverbSc DSY_SDRAM_BSS reverb;
DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS leftDelayLine;
DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS rightDelayLine;

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

delay leftDelay;
delay rightDelay;

DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS leftResoPoleDelayLine[3];
DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS rightResoPoleDelayLine[3];
Resonator resonator;

enum class Range
{
    FULL,
    HIGH,
    LOW,
};

struct GeneratorConf
{
    bool active;
    float volume;
    float pan;
    int pitch;
    float character;
    int ringSource;
    float ringAmt;

};
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

struct EffectConf
{
    bool active;
    float dryWet;
    float param1;
};
EffectConf effectsConf[4];

enum class FilterType
{
    HP,
    LP,
    BP,
};
FilterType filterType;

int basePitch;

std::string str{"Starting..."};
char *cstr{&str[0]};

void Print(std::string text, int line = 1)
{
    str = text;
    bluemchen.display.SetCursor(0, (line - 1) * 8);
    bluemchen.display.WriteString(cstr, Font_6x8, true);
}

void UpdateOled()
{
    //int width = bluemchen.display.Width();

    bluemchen.display.Fill(false);

    Print(std::to_string(static_cast<int>(knob1.Value() * 100)));
    Print(std::to_string(basePitch), 2);

    bluemchen.display.Update();
}

float CalcFrequency(int basePitch, float pitch)
{
    float midi_nn = fclamp(basePitch + pitch, 0.f, 120.f);

    return mtof(midi_nn);
}

float RandomFloat(float min, float max)
{
    return min + static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX / (max - min)));
}

enum class Scale
{
    IONIAN,
    DORIAN,
    PHRYGIAN,
    LYDIAN,
    MIXOLYDIAN,
    AEOLIAN,
    LOCRIAN,
    LAST_SCALE,
};
constexpr int scaleIntervals{15};
// Ionian w-w-h-w-w-w-h
// Dorian   w-h-w-w-w-h-w
// Phrygian   h-w-w-w-h-w-w
// Lydian       w-w-w-h-w-w-h
// Mixolydian     w-w-h-w-w-h-w
// Aeolian          w-h-w-w-h-w-w
// Locrian            h-w-w-h-w-w-w
int scales[static_cast<unsigned int>(Scale::LAST_SCALE)][scaleIntervals]{
    {-12, -10, -8, -7, -5, -3, -1, 0, 2, 4, 5, 7, 9, 11, 12},
    {-12, -10, -9, -7, -5, -3, -2, 0, 2, 3, 5, 7, 9, 10, 12},
    {-12, -11, -9, -7, -5, -4, -2, 0, 1, 3, 5, 7, 8, 10, 12},
    {-12, -10, -8, -6, -5, -3, -1, 0, 2, 4, 6, 7, 9, 11, 12},
    {-12, -10, -8, -7, -5, -3, -2, 0, 2, 4, 5, 7, 9, 10, 12},
    {-12, -10, -9, -7, -5, -4, -2, 0, 2, 3, 5, 7, 8, 10, 12},
    {-12, -11, -9, -7, -6, -4, -2, 0, 1, 3, 5, 6, 8, 10, 12},
};

Scale currentScale{Scale::PHRYGIAN};


int RandomPitch(Range range)
{
    int rnd;
    
    int full[]{-12, -11, -9, -7, -5, -4, -2, 0, 1, 3, 5, 7, 8, 10, 12};

    if (Range::HIGH == range)
    {
        // (midi 66-72)
        //rnd = RandomFloat(42, 72);
        int half{static_cast<int>(std::ceil(scaleIntervals / 2))};
        rnd = full[(half + (std::rand() % half)) - 1];
    }
    else if (Range::LOW == range)
    {
        // (midi 36-65)
        //rnd = RandomFloat(12, 41);
        int half{static_cast<int>(std::ceil(scaleIntervals / 2))};
        rnd = full[std::rand() % half - 1];
    }
    else
    {
        // (midi 36-96)
        //rnd = RandomFloat(12, 72);
        rnd = full[std::rand() % scaleIntervals];
    }

    return rnd;
}

bool useEnvelope{false};
bool randomize{false};

void SetPitch(float pitch)
{
    hOsc1.SetFreq(CalcFrequency(generatorsConf[0].pitch, pitch));
    hOsc2.SetFreq(CalcFrequency(generatorsConf[2].pitch, pitch));
    hOsc3.SetFreq(CalcFrequency(generatorsConf[4].pitch, pitch));
    hOsc4.SetFreq(CalcFrequency(generatorsConf[6].pitch, pitch));

    lOsc1.SetFreq(CalcFrequency(generatorsConf[1].pitch, pitch));
    lOsc2.SetFreq(CalcFrequency(generatorsConf[3].pitch, pitch));
    lOsc3.SetFreq(CalcFrequency(generatorsConf[5].pitch, pitch));
    lOsc4.SetFreq(CalcFrequency(generatorsConf[7].pitch, pitch));

    float f{CalcFrequency(generatorsConf[8].pitch, pitch)};
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
        //generatorsConf[i].volume = RandomFloat(0.3f, 0.5f);

        if (i < kGenerators - 1) 
        {
            if (i % 2 == 0)
            {
                generatorsConf[i].pitch = RandomPitch(Range::HIGH);
            }
            else
            {
                generatorsConf[i].pitch = RandomPitch(Range::LOW);
            }
        }
        else
        {
            generatorsConf[i].pitch = RandomPitch(Range::FULL);
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

    //hOsc1.SetFreq(mtof(generatorsConf[0].pitch));

    //hOsc2.SetFreq(mtof(generatorsConf[2].pitch));
    hOsc2.SetWaveshape(RandomFloat(0.f, 1.f));
    hOsc2.SetPW(RandomFloat(-1.f, 1.f));

    //hOsc3.SetFreq(mtof(generatorsConf[4].pitch));
    hOsc3.SetPw(RandomFloat(-1.f, 1.f));

    //hOsc4.SetFreq(mtof(generatorsConf[8].pitch));
    hOsc4.SetPw(RandomFloat(-1.f, 1.f));

    //lOsc1.SetFreq(mtof(generatorsConf[1].pitch));

    //lOsc2.SetFreq(mtof(generatorsConf[3].pitch));
    lOsc2.SetWaveshape(RandomFloat(0.f, 1.f));
    lOsc2.SetPW(RandomFloat(-1.f, 1.f));

    //lOsc3.SetFreq(mtof(generatorsConf[5].pitch));
    lOsc3.SetPw(RandomFloat(-1.f, 1.f));

    //lOsc4.SetFreq(mtof(generatorsConf[7].pitch));
    lOsc4.SetPw(RandomFloat(-1.f, 1.f));

    float freq{ mtof(generatorsConf[8].pitch) };
    //noiseFilterHP.SetFreq(freq);
    //noiseFilterLP.SetFreq(freq);
    generatorsConf[8].character = RandomFloat(1.f, 2.f);

    SetPitch(basePitch);

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
        freq = mtof(pitch);
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

    randomize = false;
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

void UpdateKnob1()
{
    SetPitch(fmap(knob1Value, -30.f, 30.f));
}

void UpdateKnob2()
{
    SetCharacter(knob2Value);
}

void UpdateCv1()
{
    // 0-5v -> 5 octaves
    float voct = fmap(cv1.Value(), 24.f, 84.f);
    SetPitch(voct);
    //envelopeGate = true;
}

void UpdateControls()
{
    bluemchen.ProcessAllControls();

    knob1.Process();
    knob2.Process();

    bluemchen.seed.dac.WriteValue(daisy::DacHandle::Channel::ONE, static_cast<uint16_t>(knob1_dac.Process()));
    bluemchen.seed.dac.WriteValue(daisy::DacHandle::Channel::TWO, static_cast<uint16_t>(knob2_dac.Process()));

    cv1.Process();
    cv2.Process();

    envelopeGate = useEnvelope ? cv1.Value() > 0.5f : true;
    //SetPitch(fmap(cv2.Value(), 24.f, 84.f));

    if (std::abs(knob2Value - knob2.Value()) > 0.01f)
    {
        basePitch = 24 + knob2.Value() * 60;
        knob2Value = knob2.Value();
        SetPitch(basePitch);
    }

    //resonator.SetDecay(fmap(knob1.Value(), 0.f, 0.5f));
    //resonator.SetDetune(fmap(knob1.Value(), 0.f, 1.f));
    //resonator.SetReso(fmap(knob1.Value(), 0.f, 0.4f));
    //resonator.SetPitch(0, fmap(knob2.Value(), 30.f, 60.f));
    //resonator.SetPitch(1, fmap(knob1.Value(), 20.f, 40.f));
    //resonator.SetPitch(2, fmap(knob1.Value(), 40.f, 80.f));
    //resonator.SetDamp(fmap(knob2.Value(), 40.f, sampleRate / 3.f));

    /*
    if (knob1.Value() != knob1Value)
    {
        knob1Value = knob1.Value();
        UpdateKnob1();
    }
        if (knob2.Value() != knob2Value)
        {
            knob2Value = knob2.Value();
            UpdateKnob2();
        }

    UpdateCv1();
    */
}

void UpdateMenu()
{
    if (bluemchen.encoder.FallingEdge())
    {
        randomize = true;
    }
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        float left{0.f};
        float right{0.f};

        // Process all generators.
        float sigs[kGenerators];
        for (int j = 0; j < kGenerators; j++)
        {
            if (generatorsConf[j].active) 
            {
                if (j == 0) {
                    sigs[j] = hOsc1.Process();
                }
                else if (j == 1)
                {
                    sigs[j] = lOsc1.Process();
                }
                else if (j == 2)
                {
                    sigs[j] = hOsc2.Process();
                }
                else if (j == 3)
                {
                    sigs[j] = lOsc2.Process();
                }
                else if (j == 4)
                {
                    sigs[j] = hOsc3.Process();
                }
                else if (j == 5)
                {
                    sigs[j] = lOsc3.Process();
                }
                else if (j == 6)
                {
                    sigs[j] = hOsc4.Process();
                }
                else if (j == 7)
                {
                    sigs[j] = lOsc4.Process();
                }
                else if (j == 8)
                {
                    sigs[j] = noise.Process();
                    sigs[j] = generatorsConf[j].character * sigs[j];
                    sigs[j] = noiseFilterHP.Process(sigs[j]);
                    sigs[j] = generatorsConf[j].character * sigs[j];
                    sigs[j] = SoftClip(noiseFilterLP.Process(sigs[j]));
                }

                left += sigs[j] * generatorsConf[j].volume * (1 - generatorsConf[j].pan) * envelopes[j].Process(envelopeGate);
                right += sigs[j] * generatorsConf[j].volume * generatorsConf[j].pan * envelopes[j].Process(envelopeGate);
            }
        }

        /*
        // Apply ring modulation.
        for (int j = 0; j < kGenerators; j++)
        {
            if (generatorsConf[j].ringSource > 0 && generatorsConf[j].active && generatorsConf[generatorsConf[j].ringSource].active)
            {
                sigs[j] = SoftClip(sigs[j] * sigs[generatorsConf[j].ringSource]);
            }
            left += sigs[j] * generatorsConf[j].volume * (1 - generatorsConf[j].pan) * envelopes[j].Process(envelopeGate);
            right += sigs[j] * generatorsConf[j].volume * generatorsConf[j].pan * envelopes[j].Process(envelopeGate);
        }
        */

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

        OUT_L[i] = balancer.Process(left, 0.25f);
        OUT_R[i] = balancer.Process(right, 0.25f);
    }

    if (randomize) {
        Randomize();
    }
}

int main(void)
{
    bluemchen.Init();
    bluemchen.StartAdc();

    knob1.Init(bluemchen.controls[bluemchen.CTRL_1], 0.0f, 1.0f, Parameter::LINEAR);
    knob2.Init(bluemchen.controls[bluemchen.CTRL_2], 0.0f, 1.0f, Parameter::LINEAR);

    knob1_dac.Init(bluemchen.controls[bluemchen.CTRL_1], 0.0f, 1.0f, Parameter::LINEAR);
    knob2_dac.Init(bluemchen.controls[bluemchen.CTRL_2], 0.0f, 1.0f, Parameter::LINEAR);

    cv1.Init(bluemchen.controls[bluemchen.CTRL_3], 0.0f, 1.0f, Parameter::LINEAR);
    cv2.Init(bluemchen.controls[bluemchen.CTRL_4], 0.0f, 1.0f, Parameter::LINEAR);

    sampleRate = bluemchen.AudioSampleRate();

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

    balancer.Init(sampleRate);

    // New seed.
    srand(time(NULL));
    Randomize();

    bluemchen.StartAudio(AudioCallback);

    while (1)
    {
        UpdateControls();
        UpdateMenu();
        UpdateOled();
    }
}