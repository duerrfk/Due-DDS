Due-DDS implemente Direct Digital Synthesis (DDS) for the Arduino Due (SAM3X8E microcontroller), which can output virtually any periodic signal. The Due's 12 bit digital-to-analog converter (DAC) is used to convert digital values to anolog signals. 

Due to the limited speed of the DAC, the output frequency or, alternatively, the time resolution are limited. For instance, you can output a periodic signal of 1024 samples at about 1 kHz. To further increase the output frequency, you need to decrease the number of samples. See below for a detailed explanation.

# Configuring

To configure the signal frequency and time resolution, you need to define two variables in the source code, namely  variables ```F_SIGNAL``` and ```TABLE_SIZE``` at the top of file ```dds.c```.

# Building 

Although we use the Arduino Due hardware, this implementation does not use the Arduino IDE. Instead the implementation is based on the ATMEL Software Framework (ASF) (tested with ASF v3.29.0.41). You need to set the path to the ASF installation in the Makefile (variable ```ASF_ROOT```).

Moreover, you need to set the path the the ARM compiler using variable ```CROSS``` in the Makefile (tested with GNU ARM Embedded Toolchain version 5.2.2015q4). 

To compile, simply execute ```make``` from the source folder. 

To upload the resulting binary ```dds.bin``, you can use the BOSSA tool:

```
$ stty -F /dev/ttyACM0 1200
$ ~/local/arduino-1.5.8/hardware/tools/bossac --port=ttyACM0 -U false -e -w -v -b dds.bin -R
```

BOSSA is included with the Ardruino 1.5.8 IDE (folder ```hardware/tools```) or can be downloaded from http://www.shumatech.com/web/products/bossa. The first command opens a serial connection at 1200 Baud, which resets the microcontroller to make it ready for flashing new code (connect to the programming port for this to work). You can also manually press the reset button on the Due board for the same effect.

# Implementation of Direct Digital Synthesis (DDS)

A counter is incremented with each tick of a timer. When the counter reaches a threshold, the phase angle is incremented by a certain phase step, and a digital-to-analog conversion is triggered. The phase angle is mapped to a digital value through a phase-to-value table. 
  
The output frequency of the periodic signal is calculated as

    f_signal = (f_clock/(c+1))/2.0 / n

with
  
* f_clock: frequency of timer.
* c: timer counter threshold (compare value) when to increment the phase and trigger a digital-to-analog conversion.
* n: number of entries (samples) in phase-to-value table.

f_clock is set to the highest possible frequency (f_clock = 84 MHz/2 = 42 MHz) to allow for the highest possible time resolution of the output signal. Higher values of n increase the time resolution. c and n together define the signal frequency.

To determine the maximum output frequency at a certain resolution (number of samples n), we not only need to consider the above values but also the processing latency to feed the DAC with digital values to be converted and the DAC latency for converting a digital to an analog value. The DAC latency of the SAM3X8E microcontroller is 25 DAC clock periods, where the DAC clock runs at half the speed of the master clock, i.e., 42 MHz. The DAC is fed concurrently through a FIFO buffer while the DAC converts the next value. As long as one value can be provided to the DAC at a frequency of at least 42 MHz/25 = 1.68 MHz---which is feasible for the Due running at 84 MHz---, processing latency will not become the bottleneck. For instance, we can generate a 1 kHz signal with 1024 samples since:

1000 Hz * 1024 = 1.024 MHz < 1.68 MHz

However, to generate a 2 kHz signal, we have to decrease the resolution to 512 samples.
