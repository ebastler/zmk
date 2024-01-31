/*
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

#define PULL_UPS_ENABLED GPIO_INPUT | GPIO_ACTIVE_HIGH | GPIO_PULL_UP
#define PULL_UPS_DISABLED GPIO_INPUT | GPIO_ACTIVE_HIGH

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_ZMK_EC11_AUTO_OFF_IDLE) || IS_ENABLED(CONFIG_ZMK_EC11_AUTO_OFF_SLEEP)

int zmk_encoder_sleep_event_listener(const zmk_event_t *eh) {
    const struct device *gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    const struct device *gpio1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));

    struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) {
        return -ENOTSUP;
    }

    switch (ev->state) {
    case ZMK_ACTIVITY_ACTIVE:
        LOG_DBG("Entering active mode. Re-enabling encoders.");
        if (gpio_pin_configure(gpio1, 0, PULL_UPS_ENABLED)) {
            return -ENOTSUP;
        }
        if (gpio_pin_configure(gpio0, 22, PULL_UPS_ENABLED)) {
            return -ENOTSUP;
        }
        break;

#if IS_ENABLED(CONFIG_ZMK_EC11_AUTO_OFF_IDLE)
    case ZMK_ACTIVITY_IDLE:
#endif /* IS_ENABLED(CONFIG_ZMK_EC11_AUTO_OFF_IDLE) ß*/

#if IS_ENABLED(CONFIG_ZMK_EC11_AUTO_OFF_IDLE) || IS_ENABLED(CONFIG_ZMK_EC11_AUTO_OFF_SLEEP)
    case ZMK_ACTIVITY_SLEEP:
        LOG_DBG("Disabling encoders.");
        if (!gpio_pin_get(gpio1, 0)) {
            if (gpio_pin_configure(gpio1, 0, PULL_UPS_DISABLED)) {
                return -ENOTSUP;
            }
        }
        if (!gpio_pin_get(gpio0, 22)) {
            if (gpio_pin_configure(gpio0, 22, PULL_UPS_DISABLED)) {
                return -ENOTSUP;
            }
        }
        break;
#endif /* IS_ENABLED(CONFIG_ZMK_EC11_AUTO_OFF_IDLE) ||                                             \
          IS_ENABLED(CONFIG_ZMK_EC11_AUTO_OFF_SLEEP)*/

    default:
        LOG_WRN("Unhandled activity state: %d", ev->state);
        return -EINVAL;
    }
    return 0;
}

ZMK_LISTENER(encoder_sleep, zmk_encoder_sleep_event_listener);
ZMK_SUBSCRIPTION(encoder_sleep, zmk_activity_state_changed);
#endif
