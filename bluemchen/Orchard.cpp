#include "daisysp.h"
#include "kxmx_bluemchen.h"

using namespace kxmx;
using namespace daisy;
using namespace daisysp;

Bluemchen bluemchen;

Oscillator *highOscillators;
Oscillator *lowOscillators;


Particle geiger;
WhiteNoise noise;

Parameter knob1;
Parameter knob2;

Parameter knob1_dac;
Parameter knob2_dac;

Parameter cv1;
Parameter cv2;

void UpdateControls()
{
    bluemchen.ProcessAllControls();

    knob1.Process();
    knob2.Process();

    bluemchen.seed.dac.WriteValue(daisy::DacHandle::Channel::ONE, static_cast<uint16_t>(knob1_dac.Process()));
    bluemchen.seed.dac.WriteValue(daisy::DacHandle::Channel::TWO, static_cast<uint16_t>(knob2_dac.Process()));

    cv1.Process();
    cv2.Process();
}

void UpdateOled()
{
    //int width = bluemchen.display.Width();

    bluemchen.display.Fill(false);

    std::string str{"Starting..."};
    char *cstr{&str[0]};

    str = std::to_string(static_cast<int>(knob1.Value() * 100));
    bluemchen.display.SetCursor(0, 0);
    bluemchen.display.WriteString(cstr, Font_6x8, true);

    str = std::to_string(static_cast<int>(knob2.Value() * 100));
    bluemchen.display.SetCursor(0, 8);
    bluemchen.display.WriteString(cstr, Font_6x8, true);

    bluemchen.display.Update();
}

void UpdateMenu()
{   
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    float sig;
    for (size_t i = 0; i < size; i++)
    {
        for (int j = 0; j < 1; j++)
        {
            //sig += highOscillators[j].Process() * 0.5f;
        }

        geiger.SetFreq(knob1.Value() * 1000.f);
        geiger.SetResonance(knob2.Value());
        sig += geiger.Process() * 0.5f;

        OUT_L[i] = OUT_R[i] = sig;
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

    float sampleRate = bluemchen.AudioSampleRate();

    geiger.Init(sampleRate);
    geiger.SetDensity(0.1f);
    geiger.SetSpread(2.f);

    noise.Init();

    highOscillators[0].Init(sampleRate);
    highOscillators[0].SetWaveform(Oscillator::WAVE_SIN);
    highOscillators[0].SetFreq(440);
    highOscillators[0].SetAmp(0.5);

    bluemchen.StartAudio(AudioCallback);

    while (1) {
        UpdateControls();
        UpdateOled();
        UpdateMenu();
    }
}