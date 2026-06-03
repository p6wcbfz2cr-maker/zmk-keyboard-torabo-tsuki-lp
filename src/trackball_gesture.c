// SPDX-License-Identifier: GPL-2.0-or-later
//
// Trackball direction gestures, implemented as a ZMK input processor.
//
// This processor is placed FIRST in &pointing_listener's input-processors so it
// sees raw pointer movement before any transform/scaler. While a configured
// "gesture layer" is active (entered by holding the trigger key, e.g.
// `&lt 9 BACKSPACE`), it:
//   1. accumulates raw X/Y and, once a direction crosses the threshold, fires a
//      keymap position (UP/DOWN/LEFT/RIGHT) exactly once per continuous motion;
//   2. returns ZMK_INPUT_PROC_STOP so the pointer does NOT move while gesturing.
// Because it stops the event WITHOUT zeroing the value, gesture detection and
// cursor suppression no longer conflict (the earlier zip_xy_scaler-0 approach
// zeroed the value other consumers read, which broke detection).
//
// The fired positions reuse real, otherwise-unused keymap positions (top row,
// bound to the swipe actions only on the Trackball_Gesture layer), so the
// action is fully configurable in config/keymap.keymap.

#define DT_DRV_COMPAT zmk_input_processor_gesture

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include <drivers/input_processor.h>
#include <zmk/keymap.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>

LOG_MODULE_REGISTER(trackball_gesture, CONFIG_ZMK_LOG_LEVEL);

/* ===== Tunable parameters ===== */
// Accumulated raw movement on the dominant axis before a direction fires.
#define GESTURE_THRESHOLD 80
// The ball must stay still (no movement events) for at least this long before
// another gesture can fire. One continuous motion => one action.
#define GESTURE_IDLE_REARM_MS 300
// Delay between the virtual key press and its release.
#define GESTURE_RELEASE_DELAY_MS 15

/* Keymap positions used as gesture targets. These are REAL, normally-unused
 * positions (top row, &none on every layer); on the Trackball_Gesture layer
 * they are bound to the swipe actions. */
#define GESTURE_POS_UP 0
#define GESTURE_POS_DOWN 1
#define GESTURE_POS_LEFT 2
#define GESTURE_POS_RIGHT 3

/* Layers on which trackball gestures are active (and the cursor is suppressed).
 * Add more entries to enable gestures on additional layers. */
static const zmk_keymap_layer_id_t gesture_layers[] = {9};

static int32_t accum_x;
static int32_t accum_y;
static bool disarmed; // true after a fire, until the ball goes idle again
static uint32_t queued_press_pos;
static uint32_t pending_release_pos;
static bool release_pending;

static void raise_position(uint32_t position, bool pressed) {
    raise_zmk_position_state_changed(
        (struct zmk_position_state_changed){.source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
                                            .position = position,
                                            .state = pressed,
                                            .timestamp = k_uptime_get()});
}

static void gesture_release_work_cb(struct k_work *work) {
    if (release_pending) {
        raise_position(pending_release_pos, false);
        release_pending = false;
    }
}

static K_WORK_DELAYABLE_DEFINE(gesture_release_work, gesture_release_work_cb);

// Raise the press/release from the system workqueue rather than inline in the
// input thread, matching how ZMK raises position events elsewhere.
static void gesture_press_work_cb(struct k_work *work) {
    if (release_pending) {
        raise_position(pending_release_pos, false);
        release_pending = false;
    }

    raise_position(queued_press_pos, true);
    pending_release_pos = queued_press_pos;
    release_pending = true;
    k_work_schedule(&gesture_release_work, K_MSEC(GESTURE_RELEASE_DELAY_MS));

    LOG_DBG("trackball gesture fired position=%u", queued_press_pos);
}

static K_WORK_DEFINE(gesture_press_work, gesture_press_work_cb);

// Re-arm once the ball has been idle for GESTURE_IDLE_REARM_MS. Every movement
// pushes this back, so one continuous motion stays disarmed after its fire.
static void gesture_rearm_work_cb(struct k_work *work) {
    disarmed = false;
    accum_x = 0;
    accum_y = 0;
}

static K_WORK_DELAYABLE_DEFINE(gesture_rearm_work, gesture_rearm_work_cb);

static bool any_gesture_layer_active(void) {
    for (size_t i = 0; i < ARRAY_SIZE(gesture_layers); i++) {
        if (zmk_keymap_layer_active(gesture_layers[i])) {
            return true;
        }
    }
    return false;
}

static void fire_gesture(uint32_t position) {
    queued_press_pos = position;
    k_work_submit(&gesture_press_work);
    accum_x = 0;
    accum_y = 0;
}

static int gesture_handle_event(const struct device *dev, struct input_event *event,
                                uint32_t param1, uint32_t param2,
                                struct zmk_input_processor_state *state) {
    if (!any_gesture_layer_active()) {
        // Not gesturing: keep the slate clean and let the pointer work normally.
        accum_x = 0;
        accum_y = 0;
        disarmed = false;
        k_work_cancel_delayable(&gesture_rearm_work);
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type == INPUT_EV_REL) {
        // Any movement (re)starts the idle timer that re-arms the gesture.
        k_work_reschedule(&gesture_rearm_work, K_MSEC(GESTURE_IDLE_REARM_MS));

        if (!disarmed) {
            switch (event->code) {
            case INPUT_REL_X:
                accum_x += event->value;
                break;
            case INPUT_REL_Y:
                accum_y += event->value;
                break;
            default:
                break;
            }

            int32_t ax = accum_x < 0 ? -accum_x : accum_x;
            int32_t ay = accum_y < 0 ? -accum_y : accum_y;

            if (ax >= GESTURE_THRESHOLD || ay >= GESTURE_THRESHOLD) {
                // One fire per continuous motion; the idle timer re-arms later.
                disarmed = true;
                if (ax >= ay) {
                    // Raw X. If left/right feel swapped, swap these two.
                    fire_gesture(accum_x > 0 ? GESTURE_POS_RIGHT : GESTURE_POS_LEFT);
                } else {
                    // Raw Y. If up/down feel swapped, swap these two.
                    fire_gesture(accum_y > 0 ? GESTURE_POS_DOWN : GESTURE_POS_UP);
                }
            }
        }
    }

    // Suppress the pointer while a gesture layer is active. STOP halts the rest
    // of the processor chain (no cursor movement) without modifying the value.
    return ZMK_INPUT_PROC_STOP;
}

static const struct zmk_input_processor_driver_api gesture_driver_api = {
    .handle_event = gesture_handle_event,
};

#define GESTURE_INST(n)                                                                            \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &gesture_driver_api);

DT_INST_FOREACH_STATUS_OKAY(GESTURE_INST)
