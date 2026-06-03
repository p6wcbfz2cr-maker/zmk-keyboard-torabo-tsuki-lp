// SPDX-License-Identifier: GPL-2.0-or-later
//
// Trackball direction gestures, implemented as a ZMK input processor.
//
// This processor (`zip_gesture`) is wired ONLY into the gesture-layer override
// of &pointing_listener (layers = <9>) as that layer's sole input processor.
// Because of that, the mere fact that it runs means a gesture layer is active —
// so it needs no layer query (ZMK keymap/layer symbols do not link from an
// external module like this repo; only the event/raise APIs do).
//
// While running it:
//   1. accumulates raw X/Y (it is first/only in the chain, so values are raw)
//      and fires a keymap position (UP/DOWN/LEFT/RIGHT) once per continuous
//      motion (re-armed after the ball goes idle);
//   2. returns ZMK_INPUT_PROC_STOP so the pointer does NOT move while gesturing
//      (STOP halts the chain without modifying the value).
//
// The fired positions are real, otherwise-unused keymap positions (top row),
// bound to the swipe actions only on the Trackball_Gesture layer, so the action
// is fully configurable in config/keymap.keymap.
//
// To enable gestures on more layers, add more <&zip_gesture> overrides to
// &pointing_listener for those layers (this processor itself is stateless about
// which layer it is on).

#define DT_DRV_COMPAT zmk_input_processor_gesture

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include <drivers/input_processor.h>
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
// When the trigger key is released the processor stops receiving events, so the
// idle timer fires during that gap and re-arms for the next hold.
static void gesture_rearm_work_cb(struct k_work *work) {
    disarmed = false;
    accum_x = 0;
    accum_y = 0;
}

static K_WORK_DELAYABLE_DEFINE(gesture_rearm_work, gesture_rearm_work_cb);

static void fire_gesture(uint32_t position) {
    queued_press_pos = position;
    k_work_submit(&gesture_press_work);
    accum_x = 0;
    accum_y = 0;
}

static int gesture_handle_event(const struct device *dev, struct input_event *event,
                                uint32_t param1, uint32_t param2,
                                struct zmk_input_processor_state *state) {
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

    // Suppress the pointer while gesturing. STOP halts the rest of the chain
    // (no cursor movement) without modifying the value.
    return ZMK_INPUT_PROC_STOP;
}

static const struct zmk_input_processor_driver_api gesture_driver_api = {
    .handle_event = gesture_handle_event,
};

#define GESTURE_INST(n)                                                                            \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &gesture_driver_api);

DT_INST_FOREACH_STATUS_OKAY(GESTURE_INST)
