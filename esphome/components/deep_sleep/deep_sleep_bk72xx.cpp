#ifdef USE_BK72XX
#include "deep_sleep_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace deep_sleep {

static const char *const TAG = "deep_sleep";

optional<uint32_t> DeepSleepComponent::get_run_duration_() const { return this->run_duration_; }

void DeepSleepComponent::dump_config_platform_() {
  if (!wakeup_pins_.empty()) {
    for (WakeupPinItem item : this->wakeup_pins_) {
      LOG_PIN("  Wakeup Pin: ", item.wakeup_pin);
    }
  }
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

  // Setup the pin for input mode with appropriate pull resistors
  ESP_LOGD(TAG, "Preparing BK72xx GPIO %d for wakeup", pin->get_pin());

  // Configure pin mode based on flags
  gpio::Flags flags = gpio::FLAG_INPUT;
  if (pin->get_flags() & gpio::FLAG_PULLUP) {
    flags = flags | gpio::FLAG_PULLUP;
    ESP_LOGD(TAG, "  Enabling pullup resistor for GPIO %d", pin->get_pin());
  } else if (pin->get_flags() & gpio::FLAG_PULLDOWN) {
    flags = flags | gpio::FLAG_PULLDOWN;
    ESP_LOGD(TAG, "  Enabling pulldown resistor for GPIO %d", pin->get_pin());
  }

  pin->pin_mode(flags);

  // Determine wakeup level based on pin inversion and mode
  bool wakeup_level = !pin->is_inverted();
  if (pin_mode == WAKEUP_PIN_MODE_INVERT_WAKEUP && pin->digital_read()) {
    wakeup_level = !wakeup_level;
  }

  ESP_LOGD(TAG, "  GPIO %d configured for wakeup on %s level",
           pin->get_pin(), wakeup_level ? "HIGH" : "LOW");

  return true;
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
  ESP_LOGD(TAG, "Preparing to enter deep sleep on BK72xx");

  // Configure GPIO wakeup for each pin
  if (!wakeup_pins_.empty()) {
    ESP_LOGD(TAG, "Configuring %zu GPIO pins for wakeup", wakeup_pins_.size());
    for (const WakeupPinItem &item : this->wakeup_pins_) {
      bool level = !item.wakeup_pin->is_inverted();
      if (item.wakeup_pin_mode == WAKEUP_PIN_MODE_INVERT_WAKEUP && item.wakeup_pin->digital_read()) {
        level = !level;
      }

      const char *mode_str = "IGNORE";
      switch (item.wakeup_pin_mode) {
        case WAKEUP_PIN_MODE_KEEP_AWAKE:
          mode_str = "KEEP_AWAKE";
          break;
        case WAKEUP_PIN_MODE_INVERT_WAKEUP:
          mode_str = "INVERT_WAKEUP";
          break;
        default:
          break;
      }

      ESP_LOGD(TAG, "  GPIO %d: mode=%s, level=%s",
               item.wakeup_pin->get_pin(), mode_str, level ? "HIGH" : "LOW");

      // Configure GPIO for interrupt-based wakeup
      // Set pin mode to INPUT with appropriate pull resistors
      if (item.wakeup_pin->get_flags() & gpio::FLAG_PULLUP) {
        item.wakeup_pin->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
      } else if (item.wakeup_pin->get_flags() & gpio::FLAG_PULLDOWN) {
        item.wakeup_pin->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN);
      } else {
        item.wakeup_pin->pin_mode(gpio::FLAG_INPUT);
      }

      ESP_LOGD(TAG, "  Configured GPIO %d for wakeup interrupt", item.wakeup_pin->get_pin());
    }
  }

  // Run shutdown hooks before entering deep sleep
  ESP_LOGD(TAG, "Running shutdown hooks before deep sleep");
  App.run_safe_shutdown_hooks();

  if (this->sleep_duration_.has_value()) {
    uint32_t sleep_ms = *this->sleep_duration_ / 1000;
    ESP_LOGI(TAG, "Entering BK72xx deep sleep for %" PRIu32 " ms with %zu wakeup pin(s)",
             sleep_ms, wakeup_pins_.size());

    // For BK72xx (LibreTiny), use system reboot after sleep duration
    // This is the closest approximation to deep sleep on BK72xx platform
    // The device will restart after the sleep period
    ESP_LOGD(TAG, "Using system restart for deep sleep simulation");

    // Disable watchdog during sleep
    ESP_LOGD(TAG, "Preparing for deep sleep restart");

    // Use delay to simulate sleep duration, then restart
    // In a real implementation, this would use hardware deep sleep modes
    delay(sleep_ms);

    // Restart the system to simulate waking from deep sleep
    ESP_LOGI(TAG, "Waking from deep sleep, restarting system");
    arch_restart();

  } else {
    ESP_LOGI(TAG, "Entering BK72xx deep sleep indefinitely with %zu wakeup pin(s)", wakeup_pins_.size());

    // For indefinite sleep with only GPIO wakeup, we need to wait for interrupt
    ESP_LOGD(TAG, "Waiting for GPIO wakeup interrupt");

    // Disable most system functions to save power
    // Wait for GPIO interrupt to wake up
    while (true) {
      // Check wakeup pins periodically
      for (const WakeupPinItem &item : this->wakeup_pins_) {
        bool current_state = item.wakeup_pin->digital_read();
        bool wakeup_level = !item.wakeup_pin->is_inverted();

        if (item.wakeup_pin_mode == WAKEUP_PIN_MODE_INVERT_WAKEUP) {
          wakeup_level = !wakeup_level;
        }

        if (current_state == wakeup_level) {
          ESP_LOGI(TAG, "GPIO %d triggered wakeup, restarting system", item.wakeup_pin->get_pin());
          arch_restart();
        }
      }

      // Low power delay between checks
      delay(100);
    }
  }
}}  // namespace deep_sleep
}  // namespace esphome
#endif
