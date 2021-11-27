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
#include "Utility/delayline.h"
#include "Utility/dsp.h"

#define MAX_DELAY static_cast<size_t>(48000 * 1.f)

using namespace kxmx;
using namespace daisy;
using namespace daisysp;

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
Oscillator hOsc5;              // Parabolic

Oscillator lOsc1;              // Sine
VariableSawOscillator lOsc2;   // Bipolar ramp
BlOsc lOsc3;                   // Triangle
BlOsc lOsc4;                   // Square
Oscillator lOsc5;              // Parabolic

BlOsc geiger;
WhiteNoise noise;

Svf leftFiler;
Svf rightFiler;

ATone noiseFilterHP;
Tone noiseFilterLP;

float sampleRate;

constexpr int kGenerators{ 12 };

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

    float Process(float feedback, float in)
    {
        //set delay times
        fonepole(currentDelay, delayTarget, .0002f);
        del->SetDelay(currentDelay);

        float read = del->Read();
        del->Write((feedback * read) + in);

        return read;
    }
};

delay leftDelay;
delay rightDelay;

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

};
// Generators configuration:
// 0 = hOsc1
// 1 = lOsc1
// 2 = noise
// 3 = hOsc2
// 4 = lOsc2
// 5 = geiger
// 6 = hOsc3
// 7 = lOsc3
// 8 = hOsc4
// 9 = lOsc4
// 10 = hOsc5
// 11 = lOsc5
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
    Print(std::to_string(static_cast<int>(knob2.Value() * 100)), 2);

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

int RandomPitch(Range range)
{
    int rnd;

    if (Range::HIGH == range)
    {
        // (midi 66-72)
        rnd = RandomFloat(42, 72);
    }
    else if (Range::LOW == range)
    {
        // (midi 36-65)
        rnd = RandomFloat(12, 41);
    }
    else
    {
        // (midi 36-96)
        rnd = RandomFloat(12, 72);
    }

    return rnd;
}

void Randomize()
{
    float volume{ 0.f };
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
        generatorsConf[i].pan = RandomFloat(0.f, 1.f);
        generatorsConf[i].volume = RandomFloat(0.3f, 0.5f);

        if (0 == i || 3 == i || 6 == i || 8 == i || 10 == i)
        {
            generatorsConf[i].pitch = RandomPitch(Range::HIGH);
        }
        else if (1 == i || 4 == i || 7 == i || 9 == i || 11 == i)
        {
            generatorsConf[i].pitch = RandomPitch(Range::LOW);
        }
        else
        {
            generatorsConf[i].pitch = RandomPitch(Range::FULL);
        }

        /*
        envelopes[i].SetTime(ADSR_SEG_ATTACK, RandomFloat(0.f, 2.f));
        envelopes[i].SetTime(ADSR_SEG_DECAY, RandomFloat(0.f, 2.f));
        envelopes[i].SetTime(ADSR_SEG_RELEASE, RandomFloat(0.f, 2.f));
        envelopes[i].SetSustainLevel(RandomFloat(0.f, 1.f));
        */

        envelopes[i].SetAttackTime(0.f);
        envelopes[i].SetDecayTime(0.1f);
        envelopes[i].SetSustainLevel(1.f);
        envelopes[i].SetReleaseTime(0.f);
    }
    for (int i = 0; i < kGenerators; i++)
    {
        if (generatorsConf[i].active) {
            generatorsConf[i].volume = 1.f / actives; //RandomFloat(0.3f, 0.5f);
        }
    }

    hOsc1.SetFreq(mtof(generatorsConf[0].pitch));

    hOsc2.SetFreq(mtof(generatorsConf[3].pitch));
    hOsc2.SetWaveshape(RandomFloat(0.f, 1.f));
    hOsc2.SetPW(RandomFloat(-1.f, 1.f));

    hOsc3.SetFreq(mtof(generatorsConf[6].pitch));
    hOsc3.SetPw(RandomFloat(-1.f, 1.f));

    hOsc4.SetFreq(mtof(generatorsConf[8].pitch));
    hOsc4.SetPw(RandomFloat(-1.f, 1.f));

    hOsc5.SetFreq(mtof(generatorsConf[10].pitch));

    lOsc1.SetFreq(mtof(generatorsConf[1].pitch));

    lOsc2.SetFreq(mtof(generatorsConf[4].pitch));
    lOsc2.SetWaveshape(RandomFloat(0.f, 1.f));
    lOsc2.SetPW(RandomFloat(-1.f, 1.f));

    lOsc3.SetFreq(mtof(generatorsConf[7].pitch));
    lOsc3.SetPw(RandomFloat(-1.f, 1.f));

    lOsc4.SetFreq(mtof(generatorsConf[9].pitch));
    lOsc4.SetPw(RandomFloat(-1.f, 1.f));

    lOsc5.SetFreq(mtof(generatorsConf[11].pitch));

    float freq{ mtof(generatorsConf[2].pitch) };
    noiseFilterHP.SetFreq(freq);
    noiseFilterLP.SetFreq(freq);
    generatorsConf[2].character = RandomFloat(1.f, 2.f);

    //geiger.SetFreq(mtof(conf[5].pitch));

    // Filter.
    effectsConf[0].active = true; //1 == std::rand() % 2;
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
    leftFiler.SetFreq(freq);
    leftFiler.SetRes(res);
    leftFiler.SetDrive(drive);
    rightFiler.SetFreq(freq);
    rightFiler.SetRes(res);
    rightFiler.SetDrive(drive);

    // Resonator.
    effectsConf[1].active = false; //1 == std::rand() % 2;
    effectsConf[1].dryWet = RandomFloat(0.f, 1.f);

    // Delay.
    effectsConf[2].active = true; //1 == std::rand() % 2;
    effectsConf[2].dryWet = RandomFloat(0.f, 1.f);
    effectsConf[2].param1 = RandomFloat(0.f, 1.f);
    leftDelay.delayTarget = RandomFloat(sampleRate * .05f, MAX_DELAY);
    rightDelay.delayTarget = RandomFloat(sampleRate * .05f, MAX_DELAY);

    // Reverb.
    effectsConf[3].active = true; //1 == std::rand() % 2;
    effectsConf[3].dryWet = RandomFloat(0.f, 1.f);
    float fb{RandomFloat(0.f, 1.f)};
    reverb.SetFeedback(fb);
    reverb.SetLpFreq(RandomFloat(0.f, sampleRate / 3.f));
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
    hOsc1.SetFreq(CalcFrequency(generatorsConf[0].pitch, pitch));
    hOsc2.SetFreq(CalcFrequency(generatorsConf[3].pitch, pitch));
    hOsc3.SetFreq(CalcFrequency(generatorsConf[6].pitch, pitch));
    hOsc4.SetFreq(CalcFrequency(generatorsConf[8].pitch, pitch));
    hOsc5.SetFreq(CalcFrequency(generatorsConf[10].pitch, pitch));

    lOsc1.SetFreq(CalcFrequency(generatorsConf[1].pitch, pitch));
    lOsc2.SetFreq(CalcFrequency(generatorsConf[4].pitch, pitch));
    lOsc3.SetFreq(CalcFrequency(generatorsConf[7].pitch, pitch));
    lOsc4.SetFreq(CalcFrequency(generatorsConf[9].pitch, pitch));
    lOsc5.SetFreq(CalcFrequency(generatorsConf[11].pitch, pitch));

    float f{CalcFrequency(generatorsConf[2].pitch, pitch)};
    noiseFilterHP.SetFreq(f);
    noiseFilterLP.SetFreq(f);
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
    float voct = fmap(cv1.Value(), 0.f, 60.f);
    SetPitch(voct);
    envelopeGate = true;
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
}

void UpdateMenu()
{
    if (bluemchen.encoder.FallingEdge())
    {
        Randomize();
    }
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        float left{0.f};
        float right{0.f};

        for (int j = 0; j < kGenerators; j++)
        {
            float sig;
            if (generatorsConf[j].active) 
            {
                if (j == 0) {
                    sig = hOsc1.Process();
                }
                else if (j == 1)
                {
                    sig = lOsc1.Process();
                }
                else if (j == 2)
                {
                    sig = noise.Process();
                    sig = generatorsConf[j].character * sig;
                    sig = noiseFilterHP.Process(sig);
                    sig = generatorsConf[j].character * sig;
                    sig = SoftClip(noiseFilterLP.Process(sig));
                }
                else if (j == 3)
                {
                    sig = hOsc2.Process();
                }
                else if (j == 4)
                {
                    sig = lOsc2.Process();
                }
                else if (j == 5)
                {
                    //sig = geiger.Process();
                }
                else if (j == 6)
                {
                    sig = hOsc3.Process();
                }
                else if (j == 7)
                {
                    sig = lOsc3.Process();
                }
                else if (j == 8)
                {
                    sig = hOsc4.Process();
                }
                else if (j == 9)
                {
                    sig = lOsc4.Process();
                }
                else if (j == 10)
                {
                    sig = hOsc5.Process();
                }
                else if (j == 11)
                {
                    sig = lOsc5.Process();
                }

                left += sig * generatorsConf[j].volume * (1 - generatorsConf[j].pan); // * envelopes[j].Process(envelopeGate);
                right += sig * generatorsConf[j].volume * generatorsConf[j].pan; // * envelopes[j].Process(envelopeGate);
            }
        }

        // Effects.
        float leftW{ 0.f };
        float rightW{ 0.f };

        // Filter.
        if (effectsConf[0].active) {
            leftFiler.Process(left);
            rightFiler.Process(right);
            switch (filterType)
            {
            case FilterType::LP:
                leftW = leftFiler.Low();
                rightW = rightFiler.Low();
                break;

            case FilterType::HP:
                leftW = leftFiler.High();
                rightW = rightFiler.High();
                break;

            case FilterType::BP:
                leftW = leftFiler.Band();
                rightW = rightFiler.Band();
                break;

            default:
                break;
            }
            left = effectsConf[0].dryWet * leftW * .3f + (1.0f - effectsConf[0].dryWet) * left;
            right = effectsConf[0].dryWet * rightW * .3f + (1.0f - effectsConf[0].dryWet) * left;
        }

        // Resonator.
        if (effectsConf[1].active) {
            left = effectsConf[1].dryWet * leftW * .3f + (1.0f - effectsConf[1].dryWet) * left;
            right = effectsConf[1].dryWet * rightW * .3f + (1.0f - effectsConf[1].dryWet) * left;
        }

        // Delay.
        if (effectsConf[2].active) {
            leftW = leftDelay.Process(effectsConf[2].param1, left);
            rightW = rightDelay.Process(effectsConf[2].param1, right);
            left = effectsConf[2].dryWet * leftW * .3f + (1.0f - effectsConf[2].dryWet) * left;
            right = effectsConf[2].dryWet * rightW * .3f + (1.0f - effectsConf[2].dryWet) * left;
        }

        // Reverb.
        if (effectsConf[3].active) {
            reverb.Process(left, right, &leftW, &rightW);
            left = effectsConf[3].dryWet * leftW * .3f + (1.0f - effectsConf[3].dryWet) * left;
            right = effectsConf[3].dryWet * rightW * .3f + (1.0f - effectsConf[3].dryWet) * left;
        }

        OUT_L[i] = left;
        OUT_R[i] = right;
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

    hOsc5.Init(sampleRate);
    hOsc5.SetWaveform(Oscillator::WAVE_SIN);
    hOsc5.SetAmp(1.f);

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

    lOsc5.Init(sampleRate);
    lOsc5.SetWaveform(Oscillator::WAVE_SIN);
    lOsc5.SetAmp(1.f);

    noise.Init();
    noise.SetAmp(1.f);
    noiseFilterHP.Init(sampleRate);
    noiseFilterLP.Init(sampleRate);

    geiger.Init(sampleRate);

    for (int i = 0; i < kGenerators; i++)
    {
        envelopes[i].Init(sampleRate);
    }

    leftFiler.Init(sampleRate);
    rightFiler.Init(sampleRate);

    leftDelayLine.Init();
    rightDelayLine.Init();
    leftDelay.del = &leftDelayLine;
    rightDelay.del = &rightDelayLine;

    float time = fmap(0.5f, 0.3f, 0.99f);
    float damp = fmap(0.f, 1000.f, 19000.f, Mapping::LOG);
    reverb.SetFeedback(time);
    reverb.SetLpFreq(damp);

    Randomize();

    bluemchen.StartAudio(AudioCallback);

    while (1)
    {
        UpdateControls();
        UpdateMenu();
    }
}