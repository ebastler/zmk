/*
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>

#include <zephyr/logging/log.h>
#include <zephyr/drivers/led.h>

#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/hid_indicators.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/ble_active_profile_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define INITIAL_DELAY 100
#define BLINK_INTERVAL 300
#define CAPS_LOCK_MASK 0x02

static const struct device *led_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_led_indicator));

enum BOARD_STATE {
    BOARD_BOOTING_1 = 1,
    BOARD_BOOTING_2 = 2,
    BOARD_PAIRING_1 = 3,
    BOARD_PAIRING_2 = 4,
    BOARD_READY = 5,
} static board_state;

static int indicator_callback() {
    if (board_state < BOARD_READY) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    zmk_hid_indicators_t indicators = zmk_hid_indicators_get_current_profile();

    if (indicators & CAPS_LOCK_MASK) {
        led_on(led_dev, 0);
    } else {
        led_off(led_dev, 0);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(indicator_callback, indicator_callback);
ZMK_SUBSCRIPTION(indicator_callback, zmk_hid_indicators_changed);

static void blink_callback();

static K_WORK_DELAYABLE_DEFINE(blink_work, blink_callback);

static void blink_callback() {
    if (board_state == BOARD_PAIRING_1) {
        led_on(led_dev, 0);
        board_state = BOARD_PAIRING_2;

    } else {
        led_off(led_dev, 0);
        board_state = BOARD_PAIRING_1;
    }

    k_work_schedule(&blink_work, K_MSEC(BLINK_INTERVAL));
}

static int pairing_callback() {
    if (board_state < BOARD_PAIRING_1) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (zmk_ble_active_profile_is_open()) {
        board_state = BOARD_PAIRING_1;
        led_off(led_dev, 0);
        k_work_schedule(&blink_work, K_MSEC(BLINK_INTERVAL));

    } else {
        k_work_cancel_delayable(&blink_work);
        board_state = BOARD_READY;
        indicator_callback();
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(pairing_callback, pairing_callback);
ZMK_SUBSCRIPTION(pairing_callback, zmk_ble_active_profile_changed);

void booting_callback();

static K_WORK_DELAYABLE_DEFINE(booting_work, booting_callback);

void booting_callback() {
    if (board_state == BOARD_BOOTING_1) {
        led_on(led_dev, 0);
        board_state = BOARD_BOOTING_2;
        k_work_schedule(&booting_work, K_MSEC(BLINK_INTERVAL));

    } else if (board_state == BOARD_BOOTING_2) {
        led_off(led_dev, 0);
        board_state = BOARD_READY;
        k_work_schedule(&booting_work, K_MSEC(BLINK_INTERVAL));

    } else {
        pairing_callback();
    }
}

static int led_indicator_init() {
    if (!device_is_ready(led_dev)) {
        LOG_ERR("Caps Lock device is not ready");
        return -ENODEV;
    }

    board_state = BOARD_BOOTING_1;
    led_off(led_dev, 0);
    k_work_schedule(&booting_work, K_MSEC(INITIAL_DELAY));

    return 0;
}

SYS_INIT(led_indicator_init, APPLICATION, 99);
