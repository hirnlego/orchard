#include <time.h>
#include "kxmx_bluemchen.h"

#include "Dynamics/balance.h"

#include "Utility/dsp.h"

#include "../generatorbank.h"
#include "../effectbank.h"



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


Balance balancer;

float sampleRate;

GeneratorBank generatorBank;
EffectBank effectBank;

bool envelopeGate{ false };



enum class Range
{
    FULL,
    HIGH,
    LOW,
};


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


int RandomInterval(Range range)
{
    int rnd;
    
    if (Range::HIGH == range)
    {
        int half{static_cast<int>(std::ceil(scaleIntervals / 2))};
        rnd = half + (std::rand() % half)) - 1;
    }
    else if (Range::LOW == range)
    {
        int half{static_cast<int>(std::ceil(scaleIntervals / 2))};
        rnd = std::rand() % half - 1;
    }
    else
    {
        rnd = std::rand() % scaleIntervals;
    }

    return rnd;
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



bool useEnvelope{false};
bool randomize{false};

void Randomize()
{
    generatorBank.Randomize();
    effectBank.Randomize();
    randomize = false;
}

void UpdateKnob1()
{
    generatorBank.SetPitch(fmap(knob1Value, -30.f, 30.f));
}

void UpdateKnob2()
{
    SetCharacter(knob2Value);
}

void UpdateCv1()
{
    // 0-5v -> 5 octaves
    float voct = fmap(cv1.Value(), 24.f, 84.f);
    generatorBank.SetPitch(voct);
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
    //generatorBank.SetPitch(fmap(cv2.Value(), 24.f, 84.f));

    if (std::abs(knob2Value - knob2.Value()) > 0.01f)
    {
        basePitch = 24 + knob2.Value() * 60;
        knob2Value = knob2.Value();
        generatorBank.SetPitch(basePitch);
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

        generatorBank.Process(left, right);
        effectBank.Process(left, right);

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

    generatorBank.Init(sampleRate);
    effectBank.Init(sampleRate);
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