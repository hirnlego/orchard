#include "kxmx_bluemchen.h"
#include "dsp.h"
#include "Synthesis/blosc.h"
#include "Synthesis/oscillator.h"
#include "Synthesis/variablesawosc.h"
#include "Synthesis/variableshapeosc.h"
#include "Noise/whitenoise.h"
#include "Filters/svf.h"
#include "Filters/atone.h"
#include "Filters/tone.h"

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

constexpr int kGenerators{ 12 };

enum class Range
{
    FULL,
    HIGH,
    LOW,
};

struct Conf
{
    bool active;
    float volume;
    float pan;
    float pitch;
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
Conf conf[kGenerators];

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

float RandomFloat(float min, float max)
{
    return min + static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX / (max - min)));
}

int RandomizePitch(Range range)
{
    int rnd;

    if (Range::HIGH == range)
    {
        rnd = std::rand() % 62 + 53;
    }
    else if (Range::LOW == range)
    {
        rnd = std::rand() % 22 + 30;
    }
    else
    {
        rnd = std::rand() % 85 + 30;
    }

    return rnd;
}

float RandomizeFrequency(Range range)
{
    return mtof(RandomizePitch(range));
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
        active = true;
        conf[i].active = active;
        if (active) {
            ++actives;
        }
        conf[i].pan = RandomFloat(0.f, 1.f);
        conf[i].volume = RandomFloat(0.3f, 0.5f);

        if (0 == i || 3 == i || 6 == i || 8 == i || 10 == i)
        {
            conf[i].pitch = RandomizePitch(Range::HIGH);
        }
        else if (1 == i || 4 == i || 7 == i || 9 == i || 11 == i)
        {
            conf[i].pitch = RandomizePitch(Range::LOW);
        }
        else
        {
            conf[i].pitch = RandomizePitch(Range::FULL);
        }
    }
    for (int i = 0; i < kGenerators; i++)
    {
        conf[i].volume = 1.f / actives; //RandomFloat(0.3f, 0.5f);
    }

    hOsc1.SetFreq(mtof(conf[0].pitch));

    hOsc2.SetFreq(mtof(conf[3].pitch));
    hOsc2.SetWaveshape(RandomFloat(0.f, 1.f));
    hOsc2.SetPW(RandomFloat(-1.f, 1.f));

    hOsc3.SetFreq(mtof(conf[6].pitch));
    hOsc3.SetPw(RandomFloat(-1.f, 1.f));

    hOsc4.SetFreq(mtof(conf[8].pitch));
    hOsc4.SetPw(RandomFloat(-1.f, 1.f));

    hOsc5.SetFreq(mtof(conf[10].pitch));

    lOsc1.SetFreq(mtof(conf[1].pitch));

    lOsc2.SetFreq(mtof(conf[4].pitch));
    lOsc2.SetWaveshape(RandomFloat(0.f, 1.f));
    lOsc2.SetPW(RandomFloat(-1.f, 1.f));

    lOsc3.SetFreq(mtof(conf[7].pitch));
    lOsc3.SetPw(RandomFloat(-1.f, 1.f));

    lOsc4.SetFreq(mtof(conf[9].pitch));
    lOsc4.SetPw(RandomFloat(-1.f, 1.f));

    lOsc5.SetFreq(mtof(conf[11].pitch));

    float freq{ mtof(conf[2].pitch) };
    noiseFilterHP.SetFreq(freq);
    noiseFilterLP.SetFreq(freq);
    //geiger.SetFreq(mtof(conf[5].pitch));


    // Filter.
    filterType = static_cast<FilterType>(std::rand() % 3);
    freq = RandomizeFrequency(Range::FULL);
    float res{ RandomFloat(0.f, 1.f) };
    float drive{ RandomFloat(0.f, 1.f) };
    leftFiler.SetFreq(freq);
    leftFiler.SetRes(res);
    leftFiler.SetDrive(drive);
    rightFiler.SetFreq(freq);
    rightFiler.SetRes(res);
    rightFiler.SetDrive(drive);
}

void SetPitch(int pitch)
{
    hOsc1.SetFreq(mtof(fclamp(conf[0].pitch + pitch, 0, 120)));
    hOsc2.SetFreq(mtof(fclamp(conf[3].pitch + pitch, 0, 120)));
    hOsc3.SetFreq(mtof(fclamp(conf[6].pitch + pitch, 0, 120)));
    hOsc4.SetFreq(mtof(fclamp(conf[8].pitch + pitch, 0, 120)));
    hOsc5.SetFreq(mtof(fclamp(conf[10].pitch + pitch, 0, 120)));

    lOsc1.SetFreq(mtof(fclamp(conf[1].pitch + pitch, 0, 120)));
    lOsc2.SetFreq(mtof(fclamp(conf[4].pitch + pitch, 0, 120)));
    lOsc3.SetFreq(mtof(fclamp(conf[7].pitch + pitch, 0, 120)));
    lOsc4.SetFreq(mtof(fclamp(conf[9].pitch + pitch, 0, 120)));
    lOsc5.SetFreq(mtof(fclamp(conf[11].pitch + pitch, 0, 120)));

    float freq{mtof(fclamp(conf[2].pitch + pitch, 0, 120))};
    noiseFilterHP.SetFreq(freq);
    noiseFilterLP.SetFreq(freq);
}

void UpdateKnob1()
{
    SetPitch(fmap(knob1Value, -63, 63));
}

void UpdateKnob2()
{
}

void UpdateCv1()
{
    SetPitch(fmap(cv1.Value(), -63, 63));
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
        //UpdateKnob1();
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
            if (conf[j].active) 
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
                    sig = noiseFilterHP.Process(sig);
                    sig = noiseFilterLP.Process(sig);
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

                left += sig * conf[j].volume * conf[j].pan;
                right += sig * conf[j].volume * (1 - conf[j].pan);
            }
        }

        // Filter.
        leftFiler.Process(left);
        rightFiler.Process(right);
        switch (filterType)
        {
        case FilterType::LP:
            left = leftFiler.Low();
            right = rightFiler.Low();
            break;

        case FilterType::HP:
            left = leftFiler.High();
            right = rightFiler.High();
            break;

        case FilterType::BP:
            left = leftFiler.Band();
            right = rightFiler.Band();
            break;

        default:
            break;
        }
        
        // Resonator.

        // Delay.

        // Reverb.

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

    float sampleRate{ bluemchen.AudioSampleRate() };

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

    leftFiler.Init(sampleRate);
    rightFiler.Init(sampleRate);

    Randomize();

    bluemchen.StartAudio(AudioCallback);

    while (1)
    {
        UpdateControls();
        UpdateMenu();
    }
}