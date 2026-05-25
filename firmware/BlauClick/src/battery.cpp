#include <Arduino.h>
#include "config.h"
#include "globals.h"
#include "battery.h"


// ════════════════════════════════════════════════════════════════
//  BATERIA
// ════════════════════════════════════════════════════════════════

float getBatteryVoltage() {
  if (PIN_EN_VBAT != PIN_UNUSED) {
    pinMode(PIN_EN_VBAT, OUTPUT);
    digitalWrite(PIN_EN_VBAT, LOW);
  }

  const float voltageDividerRatio = 2.0;
  uint32_t sum_mV = 0;

#ifdef USE_ESP_ADC_CAL
  static esp_adc_cal_characteristics_t adc_chars_local;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12,
#if defined(CONFIG_IDF_TARGET_ESP32S2)
                           ADC_WIDTH_BIT_13,
#else
                           ADC_WIDTH_BIT_12,
#endif
                           ESP_ADC_CAL_VAL_EFUSE_VREF, &adc_chars_local);
  for (int i = 0; i < BATTERY_SAMPLES; i++) {
    uint32_t raw = analogRead(PIN_VBAT);
    sum_mV += esp_adc_cal_raw_to_voltage(raw, &adc_chars_local);
    delay(2);
  }
#else
  for (int i = 0; i < BATTERY_SAMPLES; i++) {
    sum_mV += analogReadMilliVolts(PIN_VBAT);
    delay(2);
  }
#endif

  return (sum_mV / BATTERY_SAMPLES * voltageDividerRatio) / 1000.0;
}

int calculateBatteryPercentage(float voltage) {
  const float minV = BATTERY_MIN_MV / 1000.0f;
  const float maxV = BATTERY_MAX_MV / 1000.0f;
  if (voltage >= maxV) return 100;
  if (voltage <= minV) return 0;
  return constrain((int)((voltage - minV) / (maxV - minV) * 100), 0, 100);
}

bool isDeviceCharging() {
  if (PIN_CHARGE == PIN_UNUSED) return false;
  return digitalRead(PIN_CHARGE) == HIGH;
}
