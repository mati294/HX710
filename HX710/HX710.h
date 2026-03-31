/*
 * HX710.h
 *
 *  Created for HX710B sensor reading (not HX711)
 *  Author: Mateusz mati294
 *  28.03.2026
 *  This library provides functions to interface with the HX710B 0-40KPa cheap differential pressure sensor for HVAC.
 *  Inspired by mix of library for HX710 C++ and some AI sloop but adapted for C and STM32 HAL.
 *  This library is focused on read realy smal changes in pressure with some filtering and calibration options.
 *
 *  Warning: For working results sensor must be standing still, any vibrations will cause significant noise in readings, this is not a problem of library but of sensor itself.
 *  Even touching it will cause significant noise, then you must tare it again to get correct readings.
 *  Sensor is working on error scale so when you presure it with some value and then release it, it will show negative value, 
 *  then you have option to tare it or accept it as negative pressure when in reality is 0 pressure, this is not a problem of library but of sensor itself.
 *
 *  Note: Some of this options and functions are not necresary
 *
 *
 *
 *
 *\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
 * See what resistors you have on you sensor, 0ohm is for realy smal value (some Pa probably) in datasheet is 100ohm, adjust resistor for your scale.
 *\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
 */

#ifndef APPLICATION_CORE_HX710
#define APPLICATION_CORE_HX710

#include <stdint.h>
#include <stdbool.h>
#include "stm32c0xx_hal.h" // Adjust this include based on your STM32 series


typedef long variableType1; // This is for reading raw value from sensor
typedef float variableType2; // This is for final value after scaling and offset, you can change this to double if you need more precision but it will consume more resources.



#define DIF_MODE_10HZ 25 //this is probably working, not tested yet
#define SPEC_40HZ 26 //HX710B DVDD-AVDD // HX710A TEMP // see datasheet
#define DIF_MODE_40HZ 27 //this is working 
// gain for HX710* is ONLY 128 this is not configurable, see datasheet.
// Pls use and commit for only one mode for filtering. Others may not working due to cheap sensor :(

#define HX710_ERROR -999999.0 // Return this value on communication error/timeout (24-bit sensor, so this invalid value is easily recognizable)
#define HX710_QUALITI_TIMEOUT 5 //*CYCLES* Return 1 on timeout in tare_inteligent(). Value is in cycles.
#define HX710_QUALITI 50 // tare_inteligent() quality, this is range for stable value, adjust based on your sensor behavior and noise level, default is 50, in reality stable value will be betwen -20 to 20 or less.
#define HX710_SAMPLES 4 // get_value_small() samples, this you want to adjust, more isnt better. Draining performance (HX710_SAMPLES * read() * HX710_Delay) Devault values (4 * read(x) * 100ms) x = max 40 cycles for timeout.
#define HX710_MAX_SAMPLES 64 // Max samples for get_value_small, this is for filtering out outliers. Devault is 64, yes this is random max but you not need more.
#define HX710_TIMEOUT 20 //*CYCLES*  Timeout for waiting for sensor to become ready, adjust as needed based on your application and sensor behavior. This is in number of cycles, not time, so adjust based on your MCU frequency and sensor response time.
#define HX710_Delay 100 //*TIME* This is problematic and should be adjusted to your syclk MHz, current value is working for 48MHz. 
//This is delay between read() calls, if you are reading too FAST OR TO SLOOW you will get wrong unstable values, this is not a problem of library but cheap probably not orginal HX710* itself.
//Tested value for 48MHz and DIF_MODE_40HZ is 100. This and dupa variable you must test yourself
//To se if this value is working read_average with 100 times should give you stable value, if with 0 pressure there are big diverences (+10000 and then -10000) then you should increase or decrease this value.
// If in debag mode read() is giving you consistent values but get_value() is giving you unstable values then HX710_Delay is problem, if value are unstable then check cables, 0 pressure to open air and if this is not channging anything check dupa variable :)

#define HX710_MAD_FILTER 3 // 3 Is OK

#define interrupts() __enable_irq()
#define noInterrupts() __disable_irq()

typedef struct
{
  GPIO_TypeDef  *clk_gpio;
  GPIO_TypeDef  *dat_gpio;
  uint16_t      clk_pin; // Output pin for clock signal, no aditional resistor needed
  uint16_t      dat_pin; // Input pin for data signal, no aditional resistor needed
  variableType2       	Offset; //  For tara purpose, this is applied before scaling
  variableType2        Scale; //  For user defined scale factor, this is applied after offset, default is 1.0 for raw values
  variableType2		GlobalOffset; //For user defined zero point, this is applied after filtering and scaling
  
  // Low-pass filter for mode 
  variableType2 		filtered_value;
  variableType2 		filtered_value_small;
  variableType2 		filter_coef;
  
  int8_t        dupa; //This is problematic and should be adjusted to your syclk MHz, current value is working for 48MHz. Name is appropriate :)
// dupa variable must be 1~2 µs, see datasheet. This value may not work due to not original sensor or some other reason
}hx710_t;
    
/* Setup functions */
void hx710_init(hx710_t *hx710, GPIO_TypeDef *clk_gpio, uint16_t clk_pin, GPIO_TypeDef *dat_gpio, uint16_t dat_pin);
/*
Współczynnik alfa (α):
Wartość	Efekt	Zastosowanie
0.01 - 0.05	Bardzo agresywna filtracja, powolna odpowiedź	Szumne dane, długie uśrednianie
0.05 - 0.15	Mocna filtracja	Rekomendowane dla czujników ciśnienia
0.15 - 0.3	Średnia filtracja	Normalne szumy
0.3 - 0.5	Słaba filtracja, szybka odpowiedź	Szybkie zmiany
0.7 - 1.0	Prawie bez filtracji	Zaawansowana tylko
*/

/* Init */
void set_filter_coefficient(hx710_t *hx710, variableType2 alpha);
void set_scale(hx710_t *hx710, variableType2 scale);
void set_global_offset(hx710_t *hx710, variableType2 global_offset);
void set_offset(hx710_t *hx710, variableType2 offset);
variableType2 tare(hx710_t *hx710, uint8_t times, uint8_t mode);
bool tare_inteligent(hx710_t *hx710, uint8_t times, uint8_t mode); // Reapet tare until value is stable and in range of -50 to 50. In reality stable value will be betwen -20 to 20 or less. You can adjust this. Return 1 on success, 0 on failure (timeout)

/* To use */
variableType2 get_value(hx710_t *hx710, uint8_t times, uint8_t mode); // Return value with offset and GlobalOffset, get data from readavg()

variableType2 get_value_small(hx710_t *hx710, uint8_t times, uint8_t mode); // Filter for exclude damaged value/ comm error. Configurable

//Return ROUND value ^ decimal_places, 
variableType2 get_value_small_filtered(hx710_t *hx710, uint8_t times, uint8_t mode, uint8_t decimal_places); // Filter for smothing value, 0 +-160 raw ^ decimal_places is normal for 0 pressure





//variableType2 get_weight(hx710_t *hx710, int8_t times, uint8_t mode); //do wywalenia kiedys
//variableType2 get_pressure_pa(hx710_t *hx710, uint8_t times, uint8_t mode); //do wywalenia kiedys
//variableType2 get_pressure_pa_filtered(hx710_t *hx710, int8_t times, uint8_t mode, uint8_t decimal_places); //do wywalenia kiedys




/* Internal */
bool is_ready(hx710_t *hx710);
bool wait_ready(hx710_t *hx710);
variableType1 read(hx710_t *hx710, uint8_t mode);
uint8_t shiftIn(hx710_t *hx710);
static variableType2 trunc_to_decimals(variableType2 value, int decimals);
static int cmp_variableType2(const void *a, const void *b);
variableType2 read_avg(hx710_t *hx710, uint8_t times, uint8_t mode); // Basic smoothing, get data from read(), times is (5-8) is OK, more is time consuming and may not give better results due to cheap sensor.



#endif /* APPLICATION_CORE_HX710 */
