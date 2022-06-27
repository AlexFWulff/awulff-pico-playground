# pico-voice-v1

This program is a starting point for building voice models with Pico
using Edge Impulse. Much of this code is from Edge Impulse's pico [standalone repo](https://github.com/edgeimpulse/example-standalone-inferencing-pico).

To use this starting point, you need to drop in the `edge-impulse-sdk`, `model-parameters`, and `tflite-model` folders generated when exporting your model for C++.

## Modifications

If you train your model on floating-point WAV files sampled at 5 kHz (see the pico-daq folder in this repository) then you shouldn't need to change much other than the results of the inferencing.

If you trained your data on some other format, you will need to modify how data gets copied into the `features` buffer as well as things like the sample rate.