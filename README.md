# Samplelator
The NYC Makerfaire 2018 FCFL exhibit will be using one or more controllers, and this area is for the code that makes it come alive!

__sketch_teensy_color__

The directory sketch_teensy_color was branched off of *sketch_colorimeter* and contains ported code to the teensy board from arduino.

__sketch_colorimeter__

The directory sketch_colorimeter holds the Arduino code to read an AMS7262 6 channel color sensor. For this version, we use the Adafruit AS726x library to do the actual interfacing. The 6 channels are Violet, Blue, Green, Yellow, Orange, and Red. Because we are also using Neopixels as part of the display output, we primarily use the RGB components when displaying to the Neopixels. However, the actual determination of the color uses all 6 channels.

The 6 color channels can be thought of as discrete samples of a continuous waveform. Normally, you would compare the discrete signal characteristics with values from calibrated color samples in order to find the closest matching color. For this demo, we need to determine the color in a timely manner using an Arduino. So, we end up taking shortcuts to come up with the closest match. For example, we primarily use the peak channel (the channel with the strongest signal) and use that as the primary basis for selecting the color. Additionally, we use the color wheel and characteristics of the colors used in the device to come up with secondary modifications to the color matching. For example, the blue and violet colors are very close to each other in the color wheel, and have very similar peak values. However, because the blue color that we use has a stronger green component than orange, we can increase the discremenation between the violet color sample versus the blue color sample. Something similar happens with our yellow, orange, and red samples. Again, this is due to their proximity in the color wheel.
