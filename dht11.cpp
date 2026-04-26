#include "dht11.h"

static void dht11_reset(void) {
  DHT11_MODE_OUT;
  DHT11_DQ_OUT(0);
  delay(20);
  DHT11_DQ_OUT(1);
  delayMicroseconds(30);
}

uint8_t dht11_check(void) {
  uint8_t retry = 0;
  uint8_t result = 0;

  DHT11_MODE_IN;

  while (DHT11_DQ_IN && retry < 100) {
    retry++;
    delayMicroseconds(1);
  }

  if (retry >= 100) {
    result = 1;
  } else {
    retry = 0;
    while (!DHT11_DQ_IN && retry < 100) {
      retry++;
      delayMicroseconds(1);
    }
    if (retry >= 100) {
      result = 1;
    }
  }

  return result;
}

static uint8_t dht11_read_bit(void) {
  uint8_t retry = 0;

  while (DHT11_DQ_IN && retry < 100) {
    retry++;
    delayMicroseconds(1);
  }

  retry = 0;

  while (!DHT11_DQ_IN && retry < 100) {
    retry++;
    delayMicroseconds(1);
  }

  delayMicroseconds(40);
  return DHT11_DQ_IN ? 1 : 0;
}

static uint8_t dht11_read_byte(void) {
  uint8_t data = 0;

  for (uint8_t i = 0; i < 8; i++) {
    data <<= 1;
    data |= dht11_read_bit();
  }

  return data;
}

uint8_t dht11_read_raw(Dht11RawReading *reading) {
  if (!reading) {
    return 1;
  }

  uint8_t buf[5];

  dht11_reset();
  noInterrupts();

  if (dht11_check() != 0) {
    interrupts();
    return 1;
  }

  for (uint8_t i = 0; i < 5; i++) {
    buf[i] = dht11_read_byte();
  }

  interrupts();

  if ((uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4]) {
    return 1;
  }

  reading->humidityInteger = buf[0];
  reading->humidityDecimal = buf[1];
  reading->temperatureInteger = buf[2];
  reading->temperatureDecimal = buf[3];
  reading->checksum = buf[4];
  return 0;
}

uint8_t dht11_read_data(uint8_t *temp, uint8_t *humi) {
  Dht11RawReading reading;
  if (dht11_read_raw(&reading) != 0) {
    return 1;
  }

  *humi = reading.humidityInteger;
  *temp = reading.temperatureInteger;
  return 0;
}

uint8_t dht11_init(void) {
  dht11_reset();
  return dht11_check();
}
