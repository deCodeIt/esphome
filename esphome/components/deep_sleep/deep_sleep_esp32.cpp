#ifdef USE_ESP32
#include "soc/soc_caps.h"
#include "driver/gpio.h"
#include "deep_sleep_component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace deep_sleep {

static const char *const TAG = "deep_sleep";

optional<uint32_t> DeepSleepComponent::get_run_duration_() const {
  if (this->wakeup_cause_to_run_duration_.has_value()) {
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    switch (wakeup_cause) {
      case ESP_SLEEP_WAKEUP_EXT0:
      case ESP_SLEEP_WAKEUP_EXT1:
      case ESP_SLEEP_WAKEUP_GPIO:
        return this->wakeup_cause_to_run_duration_->gpio_cause;
      case ESP_SLEEP_WAKEUP_TOUCHPAD:
        return this->wakeup_cause_to_run_duration_->touch_cause;
      default:
        return this->wakeup_cause_to_run_duration_->default_cause;
    }
  }
  return this->run_duration_;
}

#if !defined(USE_ESP32_VARIANT_ESP32C3) && !defined(USE_ESP32_VARIANT_ESP32C6)
void DeepSleepComponent::set_ext1_wakeup(Ext1Wakeup ext1_wakeup) { this->ext1_wakeup_ = ext1_wakeup; }

#if !defined(USE_ESP32_VARIANT_ESP32H2)
void DeepSleepComponent::set_touch_wakeup(bool touch_wakeup) { this->touch_wakeup_ = touch_wakeup; }
#endif

#endif

void DeepSleepComponent::set_run_duration(WakeupCauseToRunDuration wakeup_cause_to_run_duration) {
  wakeup_cause_to_run_duration_ = wakeup_cause_to_run_duration;
}

bool DeepSleepComponent::prepare_pin_(InternalGPIOPin *pin, WakeupPinMode pin_mode) {
  if (pin_mode == WAKEUP_PIN_MODE_KEEP_AWAKE && !this->sleep_duration_.has_value() && pin->digital_read()) {
    // Defer deep sleep until inactive
    if (!this->next_enter_deep_sleep_) {
      this->status_set_warning();
      ESP_LOGW(TAG, "Waiting for %s to switch state to enter deep sleep...", pin->dump_summary().c_str());
    }
    this->next_enter_deep_sleep_ = true;
    return false;
  }
  bool level = !pin->is_inverted();
  if (pin_mode == WAKEUP_PIN_MODE_INVERT_WAKEUP && pin->digital_read()) {
    level = !level;
  }
#if !defined(USE_ESP32_VARIANT_ESP32C3) && !defined(USE_ESP32_VARIANT_ESP32C2)
  esp_sleep_enable_ext0_wakeup(gpio_num_t(pin->get_pin()), level);
#else
  esp_deep_sleep_enable_gpio_wakeup(1 << pin->get_pin(), static_cast<esp_deepsleep_gpio_wake_up_mode_t>(level));
#endif
  return true;
}

void DeepSleepComponent::dump_config_platform_() {
  if (!wakeup_pins_.empty()) {
    for (WakeupPinItem item : this->wakeup_pins_) {
      LOG_PIN("  Wakeup Pin: ", item.wakeup_pin);
    }
  }
  if (this->wakeup_cause_to_run_duration_.has_value()) {
    ESP_LOGCONFIG(TAG,
                  "  Default Wakeup Run Duration: %" PRIu32 " ms\n"
                  "  Touch Wakeup Run Duration: %" PRIu32 " ms\n"
                  "  GPIO Wakeup Run Duration: %" PRIu32 " ms",
                  this->wakeup_cause_to_run_duration_->default_cause, this->wakeup_cause_to_run_duration_->touch_cause,
                  this->wakeup_cause_to_run_duration_->gpio_cause);
  }
}

bool DeepSleepComponent::prepare_to_sleep_() {
  if (!wakeup_pins_.empty()) {
    for (WakeupPinItem item : this->wakeup_pins_) {
      if (!prepare_pin_(item.wakeup_pin, item.wakeup_pin_mode))
        return false;
    }
  }
  return true;
}

void DeepSleepComponent::deep_sleep_() {
  if (this->sleep_duration_.has_value())
    esp_sleep_enable_timer_wakeup(*this->sleep_duration_);

  // Handle GPIO wakeup pins - wakeup configuration already handled by prepare_pin_()
  if (!wakeup_pins_.empty()) {
    for (WakeupPinItem item : this->wakeup_pins_) {
      const auto gpio_pin = gpio_num_t(item.wakeup_pin->get_pin());
      if (item.wakeup_pin->get_flags() & gpio::FLAG_PULLUP) {
        gpio_sleep_set_pull_mode(gpio_pin, GPIO_PULLUP_ONLY);
      } else if (item.wakeup_pin->get_flags() & gpio::FLAG_PULLDOWN) {
        gpio_sleep_set_pull_mode(gpio_pin, GPIO_PULLDOWN_ONLY);
      }
      gpio_sleep_set_direction(gpio_pin, GPIO_MODE_INPUT);
      gpio_hold_en(gpio_pin);
#if !SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP
      // Some ESP32 variants support holding a single GPIO during deep sleep without this function
      // For those variants, gpio_hold_en() is sufficient to hold the pin state during deep sleep
      gpio_deep_sleep_hold_en();
#endif
    }
  }

#if !defined(USE_ESP32_VARIANT_ESP32C3) && !defined(USE_ESP32_VARIANT_ESP32C6) && !defined(USE_ESP32_VARIANT_ESP32H2)
  if (this->ext1_wakeup_.has_value()) {
    esp_sleep_enable_ext1_wakeup(this->ext1_wakeup_->mask, this->ext1_wakeup_->wakeup_mode);
  }

  if (this->touch_wakeup_.has_value() && *(this->touch_wakeup_)) {
    esp_sleep_enable_touchpad_wakeup();
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  }
#endif

#if defined(USE_ESP32_VARIANT_ESP32H2)
  if (this->ext1_wakeup_.has_value()) {
    esp_sleep_enable_ext1_wakeup(this->ext1_wakeup_->mask, this->ext1_wakeup_->wakeup_mode);
  }
#endif

  esp_deep_sleep_start();
}

}  // namespace deep_sleep
}  // namespace esphome
#endif
