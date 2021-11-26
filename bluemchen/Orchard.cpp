#include "kxmx_bluemchen.h"
#include "dsp.h"
#include "Synthesis/blosc.h"
#include "Synthesis/oscillator.h"
#include "Synthesis/variablesawosc.h"
#include "Synthesis/variableshapeosc.h"
#include "Noise/whitenoise.h"
#include "Filters/svf.h"

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
Oscillator hOsc2;              // Parabolic
BlOsc hOsc3;                   // Triangle
VariableSawOscillator hOsc4;   // Bipolar ramp
VariableShapeOscillator hOsc5; // Ramp/pulse

Oscillator lOsc1;              // Sine
Oscillator lOsc2;              // Parabolic
BlOsc lOsc3;                   // Triangle
VariableSawOscillator lOsc4;   // Bipolar ramp
VariableShapeOscillator lOsc5; // Ramp/pulse

BlOsc geiger;
WhiteNoise noise;

Svf leftFiler;
Svf rightFiler;

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
Conf conf[12];

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
    for (int i = 0; i < 12; i++)
    {
        conf[i].active = std::rand() % 2;
        conf[i].pan = RandomFloat(0.f, 1.f);
        conf[i].volume = RandomFloat(0.f, 1.f);
        if (i < 5) {
            conf[i].pitch = RandomizePitch(Range::HIGH);
        }
        else if (i < 10)
        {
            conf[i].pitch = RandomizePitch(Range::LOW);
        }
        else
        {
            conf[i].pitch = RandomizePitch(Range::FULL);
        }
    }

    hOsc1.SetFreq(mtof(conf[0].pitch));
    hOsc2.SetFreq(mtof(conf[1].pitch));
    hOsc3.SetFreq(mtof(conf[2].pitch));
    hOsc4.SetFreq(mtof(conf[3].pitch));
    hOsc4.SetPW(RandomFloat(-1.f, 1.f));
    hOsc4.SetWaveshape(RandomFloat(0.f, 1.f));
    hOsc5.SetFreq(mtof(conf[4].pitch));
    hOsc5.SetPW(RandomFloat(-1.f, 1.f));

    lOsc1.SetFreq(mtof(conf[5].pitch));
    lOsc2.SetFreq(mtof(conf[6].pitch));
    lOsc3.SetFreq(mtof(conf[7].pitch));
    lOsc4.SetFreq(mtof(conf[8].pitch));
    lOsc4.SetPW(RandomFloat(-1.f, 1.f));
    lOsc4.SetWaveshape(RandomFloat(0.f, 1.f));
    lOsc5.SetFreq(mtof(conf[9].pitch));
    lOsc5.SetPW(RandomFloat(-1.f, 1.f));

    //geiger.SetFreq(mtof(conf[11].pitch));

    // Filter.
    filterType = static_cast<FilterType>(std::rand() % 3);
    float freq = RandomizeFrequency(Range::FULL);
    float res = RandomFloat(0.f, 1.f);
    float drive = RandomFloat(0.f, 1.f);
    leftFiler.SetFreq(freq);
    leftFiler.SetRes(res);
    leftFiler.SetDrive(drive);
    rightFiler.SetFreq(freq);
    rightFiler.SetRes(res);
    rightFiler.SetDrive(drive);
}

void SetPitch(int pitch)
{
    hOsc1.SetFreq(mtof(conf[0].pitch + pitch));
    hOsc2.SetFreq(mtof(conf[1].pitch + pitch));
    hOsc3.SetFreq(mtof(conf[2].pitch + pitch));
    hOsc4.SetFreq(mtof(conf[3].pitch + pitch));
    hOsc5.SetFreq(mtof(conf[4].pitch + pitch));

    lOsc1.SetFreq(mtof(conf[5].pitch + pitch));
    lOsc2.SetFreq(mtof(conf[6].pitch + pitch));
    lOsc3.SetFreq(mtof(conf[7].pitch + pitch));
    lOsc4.SetFreq(mtof(conf[8].pitch + pitch));
    lOsc5.SetFreq(mtof(conf[9].pitch + pitch));
}

void UpdateKnob1()
{
    SetPitch(fmap(knob1Value, -63, 63));
}

void UpdateKnob2()
{
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

        for (int j = 0; j < 12; j++)
        {
            float sig;
            if (conf[j].active) 
            {
                if (j == 0) {
                    sig = hOsc1.Process();
                }
                else if (j == 1)
                {
                    sig = hOsc2.Process();
                }
                else if (j == 2)
                {
                    sig = hOsc3.Process();
                }
                else if (j == 3)
                {
                    sig = hOsc4.Process();
                }
                else if (j == 4)
                {
                    sig = hOsc5.Process();
                }
                else if (j == 5)
                {
                    sig = lOsc1.Process();
                }
                else if (j == 6)
                {
                    sig = lOsc2.Process();
                }
                else if (j == 7)
                {
                    sig = lOsc3.Process();
                }
                else if (j == 8)
                {
                    sig = lOsc4.Process();
                }
                else if (j == 9)
                {
                    sig = lOsc5.Process();
                }
                else if (j == 10)
                {
                    sig = noise.Process();
                }
                else if (j == 11)
                {
                    sig = geiger.Process();
                }

                left += sig * conf[j].volume * conf[j].pan;
                right += sig * conf[j].volume * (1 - conf[j].pan);
            }
        }

        // Effects.
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
    hOsc1.SetWaveform(Oscillator::WAVE_SIN);
    hOsc2.SetAmp(1.f);

    hOsc3.Init(sampleRate);
    hOsc3.SetWaveform(BlOsc::WAVE_TRIANGLE);
    hOsc3.SetAmp(1.f);

    hOsc4.Init(sampleRate);

    hOsc5.Init(sampleRate);
    hOsc5.SetWaveshape(1.f);

    lOsc1.Init(sampleRate);
    lOsc1.SetWaveform(Oscillator::WAVE_SIN);
    lOsc1.SetAmp(1.f);

    lOsc2.Init(sampleRate);
    lOsc2.SetWaveform(Oscillator::WAVE_SIN);
    lOsc2.SetAmp(1.f);

    lOsc3.Init(sampleRate);
    lOsc3.SetWaveform(BlOsc::WAVE_TRIANGLE);
    lOsc3.SetAmp(1.f);

    lOsc4.Init(sampleRate);

    lOsc5.Init(sampleRate);
    lOsc5.SetWaveshape(1.f);

    noise.Init();
    noise.SetAmp(1.f);

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