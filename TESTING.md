# African Drum – test & release checklist

Things that can only be verified on real Mac / Windows machines (not done in the Linux build sandbox):

1. Load VST3/AU in Reaper, Logic, Ableton, FL Studio, Cubase; check 16 stereo outputs, MIDI in, state recall.
2. `auval -v aumu Agdr Agsp` (AU) and run **pluginval** (strictness 5) on every format.
3. MIDI: GM map (default), CC4 hi-hat pedal (closed / half-open / open), velocity layers + Sensitivity slider.
4. Mixer: cymbal mic faders (Dry/Studio/Room/Hall), TUNER per strip, SAVE/LOAD MIX (5 factory presets per kit).
5. Licence build: `cmake -DAG_REQUIRE_LICENSE=ON -DAG_LICENSE_SECRET=<secret>`; make keys with
   `AG_LICENSE_SECRET=<secret> python3 Tools/License/keygen.py "Name" email@x.com`.
   Offline keys are only a deterrent. Change the secret before release.
6. Mac: codesign + notarize; Windows: sign installer (CI secrets).
