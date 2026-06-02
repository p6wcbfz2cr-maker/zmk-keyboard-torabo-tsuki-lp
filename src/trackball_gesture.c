// SPDX-License-Identifier: GPL-2.0-or-later
//
// Trackball direction gestures.
//
// While one of the configured "gesture layers" is active (typically entered by
// holding a key bound to `&mo <layer>`), rotating the trackball far enough in a
// direction fires a virtual key position (UP/DOWN/LEFT/RIGHT). The action for
// each direction is whatever that position is bound to on the active layer in
// config/keymap.keymap, so "key + direction -> action" is fully configurable
// there without touching this file.
//
// The virtual positions are appended to size_l_transform in
// boards/shields/torabo_tsuki_lp/torabo_tsuki_lp.dtsi; keep GESTURE_POS_*
// in sync with that order.

#include <zephyr/sys/util_macro.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include <zmk/keymap.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>

LOG_MODULE_REGISTER(trackball_gesture, CONFIG_ZMK_LOG_LEVEL);

/* ===== Tunable parameters ===== */
// Accumulated raw movement (sensor counts) on the dominant axis before a
// direction fires. Larger = the ball must travel further to trigger a gesture.
#define GESTURE_THRESHOLD 80
// Minimum gap between consecutive fires while holding & keeping the ball moving.
// Keep this >= GESTURE_RELEASE_DELAY_MS. Lower it for faster auto-repeat.
#define GESTURE_COOLDOWN_MS 150
// Delay between the virtual key press and its release, so the host registers a
// distinct tap.
#define GESTURE_RELEASE_DELAY_MS 15

/* Keymap positions used as gesture targets. These are REAL, normally-unused
 * positions (the top row, bound to &none on every layer) rather than synthetic
 * ones, because raised events only resolve to bindings for positions that exist
 * in the active physical layout's position map. On the Trackball_Gesture layer
 * these positions are bound to the swipe actions; the action is whatever the
 * keymap maps them to there. */
#define GESTURE_POS_UP 0
#define GESTURE_POS_DOWN 1
#define GESTURE_POS_LEFT 2
#define GESTURE_POS_RIGHT 3

/* Layers on which trackball gestures are active. Bind a key to `&mo 9` (or a
 * hold-tap) to enter the gesture layer, then rotate the ball. Add more entries
 * to enable gestures on additional layers (each layer can map the four
 * positions to different actions). */
static const zmk_keymap_layer_id_t gesture_layers[] = {9};

static int32_t accum_x;
static int32_t accum_y;
static int64_t cooldown_until;
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

static bool any_gesture_layer_active(void) {
    for (size_t i = 0; i < ARRAY_SIZE(gesture_layers); i++) {
        if (zmk_keymap_layer_active(gesture_layers[i])) {
            return true;
        }
    }
    return false;
}

static void fire_gesture(uint32_t position) {
    // Make sure any in-flight virtual press is released before the next one.
    k_work_cancel_delayable(&gesture_release_work);
    if (release_pending) {
        raise_position(pending_release_pos, false);
        release_pending = false;
    }

    raise_position(position, true);
    pending_release_pos = position;
    release_pending = true;
    k_work_schedule(&gesture_release_work, K_MSEC(GESTURE_RELEASE_DELAY_MS));

    accum_x = 0;
    accum_y = 0;
    cooldown_until = k_uptime_get() + GESTURE_COOLDOWN_MS;

    LOG_DBG("trackball gesture fired position=%u", position);
}

static void trackball_gesture_cb(struct input_event *evt) {
    if (!any_gesture_layer_active()) {
        // Drop stale movement so the next hold starts from a clean slate.
        accum_x = 0;
        accum_y = 0;
        return;
    }

    if (evt->type != INPUT_EV_REL) {
        return;
    }

    switch (evt->code) {
    case INPUT_REL_X:
        accum_x += evt->value;
        break;
    case INPUT_REL_Y:
        accum_y += evt->value;
        break;
    default:
        return;
    }

    if (k_uptime_get() < cooldown_until) {
        return;
    }

    int32_t ax = accum_x < 0 ? -accum_x : accum_x;
    int32_t ay = accum_y < 0 ? -accum_y : accum_y;

    if (ax < GESTURE_THRESHOLD && ay < GESTURE_THRESHOLD) {
        return;
    }

    if (ax >= ay) {
        // Raw X. If left/right feel swapped, swap these two constants.
        fire_gesture(accum_x > 0 ? GESTURE_POS_RIGHT : GESTURE_POS_LEFT);
    } else {
        // Raw Y. If up/down feel swapped, swap these two constants.
        fire_gesture(accum_y > 0 ? GESTURE_POS_DOWN : GESTURE_POS_UP);
    }
}

// NULL device => receive input events from every device, like src/board.c does.
INPUT_CALLBACK_DEFINE(NULL, trackball_gesture_cb);

#endif /* CONFIG_ZMK_SPLIT_ROLE_CENTRAL */
