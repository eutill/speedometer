# speedometer
## Description
Accurately measures a pulse on external interrupt (e.g. from a diode) and displays the duration on three 7-segment displays driven by three 74HC595 shift registers.
The provided code allows the use of either an ATmega 328P (and related), an ATtiny 45/85 and an ATtiny 44/84(A).

## Motivation
Old analog cameras have a shutter that is often entirely mechanical. After decades of use or storage, even more so in unknown conditions, the reliability of indicated shutter speeds is questionable. This is the code for a device which measures the shutter speed by pointing a constant-on light source from the backplane of the camera through the shutter at a photodiode. When shooting, the photodiode gets hit by the light source and the resulting pulse gets evaluated by this piece of code.

## Timing and precision
Depending on target, CPU clock speed (`F_CPU`), prescaler values and register names are naturally different in order for the measurement to be precise.

The 8-bit `Timer0` is being used.

With 16 MHz clock speed and a prescaler value of 64, a granularity of 4 µs can be achieved. In this case, the uint16_t counter variable which counts timer rollovers will itself roll over after ~67 seconds.
With 8 MHz clock speed and a prescaler value of 8, a granularity of 1 µs can be achieved. The aforementioned counter variable will roll-over after 16 seconds.

Since in photography, the most common highest camera speeds available are at 1/1000s - 1/4000s, this device must and can achieve an accuracy in the order of 10e-5 s (10 µs). Therefore, its three-digit display outputs values, depending on the order of magnitude, in the following format, from lowest to highest:
- `1.23` for 1.23 ms (and, accordingly, `0.25` for 0.25 ms)
- `12.3` for 12.3 ms
- `123` for 123 ms
- `1.23.` for 1.23 s
- `12.3.` for 12.3 s
- `123.` for 123 s, provided that no roll-over happens beforehand (see above).

Basically, an appended dot at the end of the value is not meant to be a decimal dot, but signifies that the value being displayed is expressed in seconds rather than milliseconds.

## Sequential mode
In its standard operating mode, the device takes one-shot measurements. The operator would need to copy the results manually and evaluate them separately. This is where the sequential mode comes into play: It is meant to take multiple measurements at a single shutter speed setting and then display the arithmetic mean, minimum and maximum values in order for the operator to get a sense of the deviation and distribution of the results. In this mode, the number of measurements taken before evaluation is up to the user. It is engaged and disengaged with a simple pushbutton press.
