# Orchard
An oscillators plantation for the Daisy platform.

Inspired by my old Reaktor ensemble "Aerosynth":
https://www.native-instruments.com/en/reaktor-community/reaktor-user-library/entry/show/3431/

There are 10 independent oscillator, 5 working at high frequency range and 5 at low.

- 2 sine
- 1 saw
- 1 triangle
- 1 pulse

Additionally there are a noise and a geiger generator.

For each oscillator and generator can be specified:

- amplitude
- panning
- pitch (in the given range)
- "character", when present (e.g. pulse width)
- bend (? check Aero 2 here https://www.native-instruments.com/en/reaktor-community/reaktor-user-library/entry/show/3279/)
- ring modulation matrix (it may be possible to specify the modulating osc)

For all the oscillators and the noise generator only:

- attack
- decay
- sustain

Other features:

- global pitch quantization
- global randomization or specific for:
    
    - amplitude
    - pan
    - pitch 
    - "character"
    - bend
    - envelope, ring mod and FXs (in Aero 2 under a generic "params") 

Effects:

- multimode filter
- three-bands resonator
- simple delay
- reverb

All effects with dry/wet and on/off state.