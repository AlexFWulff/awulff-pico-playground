# pico-light-voice1

This program combines a voice recognition model with Neopixels to make a low-cost lighting controller. Much of the ML code is from Edge Impulse's pico [standalone repo](https://github.com/edgeimpulse/example-standalone-inferencing-pico), and the Neopixel code is from [this fantastic port](https://github.com/martinkooij/pi-pico-adafruit-neopixels) that converts the Neopixel routines to efficient PIO code.

To use this starting point, you need to drop in the `edge-impulse-sdk`, `model-parameters`, and `tflite-model` folders generated when exporting your model for C++.

## Program operation

This code really pushes the Pico to its limits! Simultaneously it's sampling from the ADC, controlling Neopixels using PIO, doing ML processing on one core, and operating a lighting state machine on the second core. Pretty neat!

## Modifications

If you train your model on floating-point WAV files sampled at 5 kHz (see the pico-daq folder in this repository) then you shouldn't need to change much other than the results of the inferencing.

If you trained your data on some other format, you will need to modify how data gets copied into the `features` buffer as well as things like the sample rate.