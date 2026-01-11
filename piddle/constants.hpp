#pragma once
#define COUNT_OF(x) (sizeof(x) / sizeof(0[x]))

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

const int LEDS_PER_STRIP = 151;

const int VOLTAGE_PIN = 36;
const int INFRARED_PIN = 15;

/*
Special pins:
0	Boot strapping pin (must be low during boot). Button.
1	UART0_TXD (default UART TX pin).
2	LED_BUILTIN (on some boards), ADC2_CH2, Touch2, HSPI_WP.
3	UART0_RXD (default UART RX pin).
4	ADC2_CH0, Touch0, HSPI_HD, PWM, and digital I/O.
5	HSPI_CS, ADC2_CH1, Touch1, digital I/O.
6	Flash pin (SCK). Not available for general use.
7	Flash pin (SDO). Not available for general use.
8	Flash pin (SDI). Not available for general use.
9	Flash pin (HD). Not available for general use.
10	Flash pin (WP). Not available for general use.
11	Flash pin (CMD). Not available for general use.
12	ADC2_CH5, Touch5, HSPI_MISO, RTC_GPIO5.
13	ADC2_CH4, Touch4, HSPI_MOSI, RTC_GPIO4.
14	ADC2_CH6, Touch6, HSPI_CLK, RTC_GPIO6.
15	ADC2_CH3, Touch3, HSPI_CS0, RTC_GPIO3.
16	UART2_RXD, digital I/O, RTC_GPIO0.
17	UART2_TXD, digital I/O, RTC_GPIO1.
18	VSPI_CLK, PWM, digital I/O.
19	VSPI_MISO, PWM, digital I/O.
21	I2C_SDA, digital I/O.
22	I2C_SCL, digital I/O.
23	VSPI_MOSI, PWM, digital I/O.
25	DAC1, ADC2_CH8, digital I/O.
26	DAC2, ADC2_CH9, digital I/O.
27	ADC2_CH7, Touch7, PWM, digital I/O.
32	ADC1_CH4, Touch9, PWM, digital I/O.
33	ADC1_CH5, Touch8, PWM, digital I/O.
34	ADC1_CH6 (input only).
35	ADC1_CH7 (input only).
36	ADC1_CH0 (input only, VP).
39	ADC1_CH3 (input only, VN).
*/
// This is the most pins I could get to work with FastLED
// constexpr int LED_PINS[] = {4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 23, 25, 26, 27, 32, 33};
constexpr int LED_PINS[] = {
  // Left side
  13, 12, 14, 27, 26, 25, 33, 32,
  // Right side
  23, 22, 21, 19, 18, 17, 16
};
const int STRIP_COUNT = COUNT_OF(LED_PINS);
