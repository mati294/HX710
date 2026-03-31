# HX710
HX710* AB capability for precise data acquisition from low-cost pressure sensors (0–40 kPa).  
This library is used in HVAC applications and works reliably.

Works in C on STM32, but can be ported to other platforms.

---

## Physical setup
**IMPORTANT**

The pressure sensor uses two resistors — you must know their values.  
In the datasheet they are specified as 100 Ω, but in reality they are often 0 Ω.  

Higher resistance = higher measurable kPa range before overflow, but worse precision.

HX710* allows reading negative values and values beyond the nominal range, but precision decreases.  
Theoretically, better results can be achieved when powering from a 5V source.

For precise measurements (e.g. HVAC, air ducts, 0–2 Pa range):
- the sensor must be stationary  
- cables and tubes must be fixed and stable  
- use rigid tubing (e.g. 4 mm air system tubing is recommended)

Even small mechanical stress can introduce errors in the range of 1–8Kunit (not KPa) .

**IMPORTANT**

---

## Sensor placement and power

Shielding the sensor with a plastic layer and aluminum foil (connected to GND) is safe.  

If possible, check the power supply with an oscilloscope for spikes and noise.

- If MCU runs at 3.3V → sensor can also run at 3.3V (digital side)  
- Same applies for 5V  
- Mixed 3.3V/5V operation is not tested  

You can add an additional ceramic capacitor on the power line (not signal).

Shorter cables are better.  
Using an additional shielding wire connected to GND is good practice.

---

For rough kPa readings, you can ignore the precision notes above.

---

## Software / Code

There is **no gain setting** — gain is fixed at 128 (per datasheet).

Available modes:
```c
#define DIF_MODE_10HZ 25 // probably works, not tested
#define SPEC_40HZ 26     // HX710B DVDD-AVDD / HX710A TEMP
#define DIF_MODE_40HZ 27 // tested and working


Sources:
https://www.micros.com.pl/mediaserver/info-uphx710b%20smd.pdf
https://electronics.stackexchange.com/questions/622752/real-input-range-of-differential-adc-hx710b
