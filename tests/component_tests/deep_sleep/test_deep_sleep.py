"""Tests for the deep sleep component."""


def test_deep_sleep_setup(generate_main):
    """
    When the deep sleep is set in the yaml file, it should be registered in main
    """
    main_cpp = generate_main("tests/component_tests/deep_sleep/test_deep_sleep1.yaml")

    assert "deepsleep = new deep_sleep::DeepSleepComponent();" in main_cpp
    assert "App.register_component(deepsleep);" in main_cpp


def test_deep_sleep_sleep_duration(generate_main):
    """
    When deep sleep is configured with sleep duration, it should be set.
    """
    main_cpp = generate_main("tests/component_tests/deep_sleep/test_deep_sleep1.yaml")

    assert "deepsleep->set_sleep_duration(60000);" in main_cpp


def test_deep_sleep_run_duration_simple(generate_main):
    """
    When deep sleep is configured with run duration, it should be set.
    """
    main_cpp = generate_main("tests/component_tests/deep_sleep/test_deep_sleep1.yaml")

    assert "deepsleep->set_run_duration(10000);" in main_cpp


def test_deep_sleep_run_duration_dictionary(generate_main):
    """
    When deep sleep is configured with dictionary run duration, it should be set.
    """
    main_cpp = generate_main("tests/component_tests/deep_sleep/test_deep_sleep2.yaml")

    assert (
        "deepsleep->set_run_duration(deep_sleep::WakeupCauseToRunDuration{\n"
        "    .default_cause = 10000,\n"
        "    .touch_cause = 10000,\n"
        "    .gpio_cause = 30000,\n"
        "});"
    ) in main_cpp


def test_bk72xx_deep_sleep_single_pin(generate_main):
    """
    When BK72xx deep sleep is configured with single pin, it should work.
    """
    main_cpp = generate_main(
        "tests/component_tests/deep_sleep/test_bk72xx_deep_sleep1.yaml"
    )

    assert (
        "libretiny_arduinointernalgpiopin_id = new libretiny::ArduinoInternalGPIOPin();"
        in main_cpp
    )
    assert "libretiny_arduinointernalgpiopin_id->set_pin(6);" in main_cpp
    assert (
        "deep_sleep_deepsleepcomponent_id->add_wakeup_pin(deep_sleep::WakeupPinItem{\n"
        "    .wakeup_pin = libretiny_arduinointernalgpiopin_id,\n"
        "    .wakeup_pin_mode = deep_sleep::WAKEUP_PIN_MODE_IGNORE,\n"
        "});"
    ) in main_cpp


def test_bk72xx_deep_sleep_single_pin_with_mode(generate_main):
    """
    When BK72xx deep sleep is configured with single pin and mode, it should work.
    """
    main_cpp = generate_main(
        "tests/component_tests/deep_sleep/test_bk72xx_deep_sleep2.yaml"
    )

    assert "bk72xx_internalgpiopin = new libretiny::InternalGPIOPin();" in main_cpp
    assert "bk72xx_internalgpiopin->set_pin(6);" in main_cpp
    assert (
        "deepsleep->add_wakeup_pin(deep_sleep::WakeupPinItem{\n"
        "    .wakeup_pin = bk72xx_internalgpiopin,\n"
        "    .wakeup_pin_mode = deep_sleep::WAKEUP_PIN_MODE_KEEP_AWAKE,\n"
        "});"
    ) in main_cpp


def test_bk72xx_deep_sleep_multi_pin(generate_main):
    """
    When BK72xx deep sleep is configured with multiple pins, it should work.
    """
    main_cpp = generate_main(
        "tests/component_tests/deep_sleep/test_bk72xx_deep_sleep3.yaml"
    )

    # Check for first pin (GPIO6 with INVERT_WAKEUP)
    assert "bk72xx_internalgpiopin = new libretiny::InternalGPIOPin();" in main_cpp
    assert "bk72xx_internalgpiopin->set_pin(6);" in main_cpp
    assert (
        "deepsleep->add_wakeup_pin(deep_sleep::WakeupPinItem{\n"
        "    .wakeup_pin = bk72xx_internalgpiopin,\n"
        "    .wakeup_pin_mode = deep_sleep::WAKEUP_PIN_MODE_INVERT_WAKEUP,\n"
        "});"
    ) in main_cpp

    # Check for second pin (GPIO7 with KEEP_AWAKE)
    assert "bk72xx_internalgpiopin_2 = new libretiny::InternalGPIOPin();" in main_cpp
    assert "bk72xx_internalgpiopin_2->set_pin(7);" in main_cpp
    assert (
        "deepsleep->add_wakeup_pin(deep_sleep::WakeupPinItem{\n"
        "    .wakeup_pin = bk72xx_internalgpiopin_2,\n"
        "    .wakeup_pin_mode = deep_sleep::WAKEUP_PIN_MODE_KEEP_AWAKE,\n"
        "});"
    ) in main_cpp

    # Check for third pin (GPIO8 with default IGNORE mode)
    assert "bk72xx_internalgpiopin_3 = new libretiny::InternalGPIOPin();" in main_cpp
    assert "bk72xx_internalgpiopin_3->set_pin(8);" in main_cpp
    assert (
        "deepsleep->add_wakeup_pin(deep_sleep::WakeupPinItem{\n"
        "    .wakeup_pin = bk72xx_internalgpiopin_3,\n"
        "    .wakeup_pin_mode = deep_sleep::WAKEUP_PIN_MODE_IGNORE,\n"
        "});"
    ) in main_cpp
