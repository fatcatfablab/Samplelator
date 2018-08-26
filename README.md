# Samplelator
The NYC Makerfaire 2018 FCFL exhibit will be using one or more controllers, and this area is for the code that makes it come alive!

__sketch_motor_control__
The motor used to spin the platter is controlled by this sketch. We use a Pololu Dual VNH5019 Motor Driver Shield for Arduino to generate the PWM. Note that this requires a separate controller from the teensy (color sensors controller) because the motor control is somewhat independent of the midi stuff, and also because the shield take up many pins from the main controller board.

__sketch_teensy_color__

The directory sketch_teensy_color was branched off of *sketch_colorimeter* and contains ported code to the teensy board from arduino. This sketch holds the code to read an AMS7262 6 channel color sensor. For this version, we use the Adafruit AS726x library to do the actual interfacing. The 6 channels are Violet, Blue, Green, Yellow, Orange, and Red. Because we are also using Neopixels as part of the display output, we primarily use the RGB components when displaying to the Neopixels. However, the actual determination of the color uses all 6 channels.

The 6 color channels can be thought of as discrete samples of a continuous waveform. Normally, you would compare the discrete signal characteristics with values from calibrated color samples in order to find the closest matching color. For this demo, we need to determine the color in a timely manner using an Arduino. So, we end up taking shortcuts to come up with the closest match. For example, we primarily use the peak channel (the channel with the strongest signal) and use that as the primary basis for selecting the color. Additionally, we use the color wheel and characteristics of the colors used in the device to come up with secondary modifications to the color matching. For example, the blue and violet colors are very close to each other in the color wheel, and have very similar peak values. However, because the blue color that we use has a stronger green component than orange, we can increase the discremenation between the violet color sample versus the blue color sample. Something similar happens with our yellow, orange, and red samples. Again, this is due to their proximity in the color wheel.

__sketch_colorimeter__

The directory sketch_colorimeter holds the Arduino code to read an AMS7262 6 channel color sensor. This is now deprecated sketch_teensy_color.

__sketch_break_detect__

The break detect sketch is a simple sketch to help debug the light break switch. This switch is used to detect when a color sample should be taken. It consists of an IR led plus detector, which pegs that pass in between to indicate a sample needs to be taken. The signal received is an analog value. Hysteris is applied to the analog signal to debounce the break detection and break restoration values. This sketch was eventually incorporated in the teensy color sketch.

__ketch_analog_scope__

This utility sketch is a quick and dirty method to display the values read from an analog pin. The display is a 32 bin value, where each bin (or star) represents a value of 8. It is derived from assuming the analog pin has 256 values ranging from 0 to 255, and is divided by 8 to fit the 32 character line display. The triggerring is based off of non-zero values. Theoretically more sophisticated triggering can be added to the logic to capture certain events. This sketch was used to help debug the light break detection device.
