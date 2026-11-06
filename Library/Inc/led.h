#ifndef LED_H
#define LED_H
#include "main.h"


void led_R_on();
void led_G_on();
void led_B_on();

void led_R_off();
void led_G_off();
void led_B_off();

void led_R_flicker(int16_t time_on , int16_t time_off , int16_t time);
void led_G_flicker(int16_t time_on , int16_t time_off , int16_t time);
void led_B_flicker(int16_t time_on , int16_t time_off , int16_t time);

#endif

