/*
 * HX710.c
 *
 *  Created for HX710B sensor reading (not HX711)
 */

#include "HX710.h"
#include <math.h> // optional
#include <stdlib.h>



static int cmp_variableType2(const void *a, const void *b)
{
    variableType2 diff = (*(variableType2*)a - *(variableType2*)b);
    return (diff > 0) - (diff < 0);
}


void hx710_init(hx710_t *hx710, GPIO_TypeDef *clk_gpio, uint16_t clk_pin, GPIO_TypeDef *dat_gpio, uint16_t dat_pin){
  // Setup the pin connections with the STM Board
  hx710->clk_gpio = clk_gpio;
  hx710->clk_pin = clk_pin;
  hx710->dat_gpio = dat_gpio;
  hx710->dat_pin = dat_pin;

  // Initialize core configuration from hx710_t struct
  hx710->Offset = 0.0;
  hx710->Scale = 1.0;
  hx710->GlobalOffset = 0.0;

  // Initialize filter values
  hx710->filtered_value = 0.0;
  hx710->filtered_value_small = 0.0;
  hx710->filter_coef = 0.1;  // default filter coefficient (0.0 - 1.0)

  // Timing adjustment (1-2 us per cycle, depends on MCU frequency)
  hx710->dupa = 4;

  GPIO_InitTypeDef gpio = {0};
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Pin = clk_pin;
  HAL_GPIO_Init(clk_gpio, &gpio);

  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Pin = dat_pin;
  HAL_GPIO_Init(dat_gpio, &gpio);
}


void set_scale(hx710_t *hx710, variableType2 scale){
  // Set the scale. To calibrate the cell, run the program with a scale of 1, call the tare_inteligent() and then the get_units function. 
  // Divide the obtained weight by the real weight. The result is the parameter to pass to scale
  // Only if you realy need this, for raw values use scale = 1.0
	hx710->Scale = scale;
}
void set_offset(hx710_t *hx710, variableType2 offset){
	hx710->Offset = offset;
}
void set_global_offset(hx710_t *hx710, variableType2 offset){
	hx710->GlobalOffset = offset;
}
void set_filter_coefficient(hx710_t *hx710, variableType2 alpha) {
  // Set filter coefficient for low-pass filter
  // alpha range: 0.0 - 1.0
  // Lower values = more smoothing (slower response)
  // Higher values = less smoothing (faster response)
  // Recommended: 0.05 - 0.2 for stable readings
	if(alpha < 0.0) alpha = 0.0;
	if(alpha > 1.0) alpha = 1.0;
	
	hx710->filter_coef = alpha;
}






static void HX710B_delay_us(uint32_t us)
{
    for(volatile int delay = 0; delay < us; delay++) { 
        __NOP(); 
    }
}

uint8_t shiftIn(hx710_t *hx710) {
    uint8_t value = 0;
    uint8_t i;

    for(i = 0; i < 8; ++i) {
    	HAL_GPIO_WritePin(hx710->clk_gpio, hx710->clk_pin, SET);
        // Small delay for signal stabilization (~1-2 μs on STM32)
        //for(volatile int delay = 0; delay < hx710->dupa; delay++) { __NOP(); }
        HX710B_delay_us(hx710->dupa);
        value |= HAL_GPIO_ReadPin(hx710->dat_gpio, hx710->dat_pin) << (7 - i);
        HAL_GPIO_WritePin(hx710->clk_gpio, hx710->clk_pin, RESET);
        //for(volatile int delay = 0; delay < hx710->dupa; delay++) { __NOP(); }
        HX710B_delay_us(hx710->dupa);
    }
	return value;
}

bool is_ready(hx710_t *hx710) {
	if(HAL_GPIO_ReadPin(hx710->dat_gpio, hx710->dat_pin) == GPIO_PIN_RESET){
		return true;
	} else {
		return false;
	}
}

bool wait_ready(hx710_t *hx710) {
	// Wait for the chip to become ready.
    uint32_t timeout = HX710_TIMEOUT; // cycles
	while (!is_ready(hx710) && timeout > 1) {
        timeout--;
        if (timeout == 0) {
            return true; // timeout error
        }
		HX710B_delay_us(16);
		HAL_GPIO_WritePin(hx710->clk_gpio, hx710->clk_pin, GPIO_PIN_SET);
        HX710B_delay_us(16);
		HAL_GPIO_WritePin(hx710->clk_gpio, hx710->clk_pin, GPIO_PIN_RESET);
	}
    return false; // ready ok
}

variableType1 read(hx710_t *hx710, uint8_t mode){

	unsigned long value = 0;
	uint8_t data[3] = { 0 };
	uint8_t filler = 0x00;
	uint8_t error = wait_ready(hx710);
    if (error) {
        return HX710_ERROR; // or some error code 0xFFF or somthing to variableType2
    }
	noInterrupts();

	data[2] = shiftIn(hx710);
	data[1] = shiftIn(hx710);
	data[0] = shiftIn(hx710);

	for (unsigned int i = 0; i < mode; i++) {
		HAL_GPIO_WritePin(hx710->clk_gpio, hx710->clk_pin, SET);
        HX710B_delay_us(hx710->dupa);
		//for(volatile int delay = 0; delay < hx710->dupa; delay++) { __NOP(); }
		HAL_GPIO_WritePin(hx710->clk_gpio, hx710->clk_pin, RESET);
		//for(volatile int delay = 0; delay < hx710->dupa; delay++) { __NOP(); }
        HX710B_delay_us(hx710->dupa);
	}
	interrupts();
	// Replicate the most significant bit to pad out a 32-bit signed integer
	if (data[2] & 0x80) {
		filler = 0xFF;
	} else {
		filler = 0x00;
	}

	// Construct a 32-bit signed integer
	value = ( (unsigned long)(filler) << 24
			| (unsigned long)(data[2]) << 16
			| (unsigned long)(data[1]) << 8
			| (unsigned long)(data[0]) );

    // You can optimalize this.
	long x = (long)(value); // Cast to signed long
	variableType1 y = (variableType1)x; // Convert to variableType1, this is for debug purposes.
	return y;
}


variableType2 read_avg(hx710_t *hx710, uint8_t times, uint8_t mode) {
	variableType2 sum = 0;
	for (uint8_t i = 0; i < times; i++) {
		sum += read(hx710, mode);
		HAL_Delay(HX710_Delay);
	}
	return sum / times;
}
variableType2 get_value(hx710_t *hx710, uint8_t times, uint8_t mode) {
	variableType2 x = read_avg(hx710, times, mode) - hx710->Offset;
    x = x * hx710->Scale;
    x = x - hx710->GlobalOffset;
	return x;
}


variableType2 tare(hx710_t *hx710, uint8_t times, uint8_t mode) {
	set_offset(hx710, 0);
	read(hx710, mode); // Change mode
	variableType2 sum;// = (variableType2)read_average(hx710, times, mode); //Faster but susceptible for communication errors, 
	sum = get_value_small(hx710,  times, mode);
    if (sum == HX710_ERROR) {
        return sum; // Return error code or handle as needed
    }
	set_offset(hx710, sum);
	return sum;
}

bool tare_inteligent(hx710_t *hx710, uint8_t times, uint8_t mode) {
    variableType2 value = 0;
    uint8_t attempts = HX710_QUALITI_TIMEOUT; // Max attempts to achieve stable tare value
    do {
        attempts--;
        if (attempts == 0) {
            return false; // Failed to get stable tare value within quality timeout
        }
        variableType2 x =  tare(hx710, times, mode);
        value = get_value_small(hx710, times, mode);
        if (value == HX710_ERROR||x == HX710_ERROR) {
          value =HX710_QUALITI + 100; // Force retry if there was a communication error
        }
    } while (value < -HX710_QUALITI || value > HX710_QUALITI); // Adjust this range based on your sensor's behavior and noise level
    return true; // Successfully got a stable tare value
}








/*
🎚️ Współczynnik alfa (α):
Wartość	Efekt	Zastosowanie
0.01 - 0.05	Bardzo agresywna filtracja, powolna odpowiedź	Szumne dane, długie uśrednianie
0.05 - 0.15	Mocna filtracja	Rekomendowane dla czujników ciśnienia
0.15 - 0.3	Średnia filtracja	Normalne szumy
0.3 - 0.5	Słaba filtracja, szybka odpowiedź	Szybkie zmiany
0.7 - 1.0	Prawie bez filtracji	Zaawansowana tylko
*/






variableType2 get_value_small(hx710_t *hx710, uint8_t times, uint8_t mode) {
if(times > HX710_MAX_SAMPLES) times = HX710_MAX_SAMPLES;
variableType2 data[times]; // max np HX710_MAX_SAMPLES 


    // gathering samples
    for(uint8_t i = 0; i < times; i++)
    {
        data[i] = get_value(hx710, HX710_SAMPLES, mode);
        if (data[i] == HX710_ERROR) { // Check for communication/timeout error
            //!!! if only one sample is error all data is error even if other samples are loking good. Check cables connections and if this repeats go to .h file and adjust HX710_SAMPLES and HX710_Delay values.
            return data[i]; // Return error code or handle as needed
        }
        HAL_Delay(10); // You may adjut this.
    }

    // sortowanie
    qsort(data, times, sizeof(variableType2), cmp_variableType2);

    // mediana
    variableType2 median = data[times / 2];

    // MAD (median absolute deviation)
    variableType2 dev[times];
    for(uint8_t i = 0; i < times; i++)
        dev[i] = fabs(data[i] - median);

    qsort(dev, times, sizeof(variableType2), cmp_variableType2);
    variableType2 mad = dev[times / 2];

    // próg odrzucania (3 * MAD)
    variableType2 threshold = 3.0 * mad;

    // liczenie średniej bez outlierów
    variableType2 sum = 0;
    uint8_t count = 0;

    for(uint8_t i = 0; i < times; i++)
    {
        if(fabs(data[i] - median) <= threshold)
        {
            sum += data[i];
            count++;
        }
    }

    if(count == 0) // Good spot for breakpoint.
        return median; // fallback

    return sum / count;
}


variableType2 get_value_small_filtered(hx710_t *hx710, uint8_t times, uint8_t mode, uint8_t decimal_places) {
  // Get filtered pressure
  // Uses low-pass filter for stable readings
	variableType2 current_value = get_value_small(hx710, times, mode);
    if (current_value == HX710_ERROR) { // Check for communication/timeout error
        return current_value; // Return error code or handle as needed
    }
	variableType2 alpha = 0;

	variableType2 x=0;

		alpha = hx710->filter_coef;
		hx710->filtered_value_small = alpha * current_value + (1.0 - alpha) * hx710->filtered_value_small;
		x = hx710->filtered_value_small;

	// Apply global offset for manual zero adjustment
	x -= hx710->GlobalOffset;

	// Truncate to specified decimal places (remove fractional part beyond decimals)
	//x = floorf_n(x, decimal_places);
	variableType2 y = 1;
	for (uint8_t i = 0; i < decimal_places ; i++) {
		y *= 10;
	}

	x = x * y;
	return round(x);

}




