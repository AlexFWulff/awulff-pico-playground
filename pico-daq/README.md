# pico-daq

This program samples from the ADC, converts the sampled values to base64, and then dumps it out over Serial. You can then read in the base 64 values into a program to convert them to whatever you'd like.

## A note on the sampling

At 5 kHz w/ 16-bit samples (the default in the program) the Pico cannot write out data as fast as it reads it. Therefore, at the end of each sampling window, the program will pause ADC sampling as it finishes writing out the rest of the samples. This will lead to some jumps in the data. For my purposes of collecting audio for training a ML model this is fine. If this is not ok, try using 8-bit samples or turn down the sample rate until the LED on the Pico starts to flash.

## Logging Base64 Values

On macOS and Linux you can use the `screen` tool to save off the base64 values:

    screen -L /dev/tty.usbmodem21301 115200

This command opens a serial console for the Pico and then will save the resulting text to the file `screenlog.0`, which you can then convert back to whatever format you'd like. Obviously you'd need to change `/dev/....` to the address of your Pico.

## Converting to WAV

I included a sample program to read in the base64 text values and convert it to WAV files that can be played on your computer. See `py/b64_16_to_wav.py`.