#ifndef DHT11_H
#define DHT11_H

#include <Arduino.h>
#include <driver/gpio.h>

#ifndef DHT11_DQ_PIN
#define DHT11_DQ_PIN GPIO_NUM_0
#endif

#define DHT11_DQ_OUT(x) gpio_set_level(DHT11_DQ_PIN, x)
#define DHT11_DQ_IN gpio_get_level(DHT11_DQ_PIN)
#define DHT11_MODE_IN gpio_set_direction(DHT11_DQ_PIN, GPIO_MODE_INPUT)
#define DHT11_MODE_OUT gpio_set_direction(DHT11_DQ_PIN, GPIO_MODE_OUTPUT)

uint8_t dht11_init(void);
uint8_t dht11_check(void);
uint8_t dht11_read_data(uint8_t *temp, uint8_t *humi);

#endif
