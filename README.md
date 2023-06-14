# Speedometer
Accurately measures an impulse on external interrupt (e.g. from a diode) and displays the duration on three 7-segment displays.

Depending on target, CPU clock speed (`F_CPU`), prescaler values and register names need to be adjusted in order to work properly.

The 8-bit `Timer0` is being used.

With 16 MHz clock speed and a prescaler value of 64, a granularity of 4 µs can be achieved. In this case, the uint16_t counter variable which counts timer rollovers will itself roll over after ~67 seconds.
With 8 MHz clock speed and a prescaler value of 8, a granularity of 1 µs can be achieved. The aforementioned counter variable will roll-over after 16 seconds.

As in photography, the most common highest camera speeds available are at 1/1000s - 1/4000s, it is convenient to have the 3-digit display `ms. 100µs 10µs` as its most precise scale.

