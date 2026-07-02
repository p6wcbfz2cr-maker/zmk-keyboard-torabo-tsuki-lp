// SPDX-License-Identifier: GPL-2.0-or-later
//
// Trackball cursor acceleration, implemented as a ZMK input processor.
//
// Wired ONLY into the base chain of &pointing_listener (normal cursor), in the
// exact same chain position previously held by zip_xy_scaler (i.e. AFTER
// zip_xy_transform and zip_temp_layer). Because zip_temp_layer's own logic does
// not read event->value at all (it only reacts to the fact/timing of an event),
// keeping this processor in that same slot means zip_temp_layer's auto-mouse
// trigger timing is unaffected by acceleration.
//
// Algorithm (fixed-point only, no floats):
//   1. Track ONE shared "last full-sample timestamp", advanced only when an
//      event with event->sync == true is seen. Both REL_X and REL_Y events
//      belonging to the same physical sample read the SAME dt this way, so
//      diagonal movement scales X and Y by the same factor (no direction
//      distortion). (If dt were tracked per-axis instead, a burst of
//      horizontal-only movement followed by a diagonal flick would see a
//      stale/huge dt on Y and a fresh/small dt on X, producing inconsistent
//      per-axis factors and a warped path.)
//   2. speed (counts/sec) = |event->value| * 1000 / clamp(dt_ms, min-dt-ms,
//      max-dt-ms).
//   3. factor (permille, 1000 = 100%) ramps from min-factor*10 to
//      max-factor*10 as speed goes from speed-threshold to speed-max, via an
//      integer power curve (accel-exponent). Below/above the thresholds the
//      factor simply clamps to min/max - this is what makes fast flicks
//      naturally "reset" to a slow factor after any pause or reversal: speed
//      is recomputed from scratch every event, so there is no persisted
//      momentum that could run away.
//   4. event->value * factor, plus/minus the ZMK-provided per-axis remainder
//      (state->remainder, only present because this node sets
//      track-remainders - the same mechanism zip_xy_scaler itself uses),
//      divided by 1000, written back to event->value. Return
//      ZMK_INPUT_PROC_CONTINUE so the (now-scaled) event proceeds down the
//      chain like the scaler did.
//
// Non-REL_X/REL_Y events (e.g. mouse button clicks, which also flow through
// this listener) are passed through completely untouched.

#define DT_DRV_COMPAT zmk_input_processor_accel

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include <drivers/input_processor.h>

LOG_MODULE_REGISTER(trackball_accel, CONFIG_ZMK_LOG_LEVEL);

/* ===== Tunable parameters (internal implementation constants; the
 * user-facing knobs - min/max factor, speed thresholds, exponent, dt clamps -
 * are DT cells/properties, see dts/bindings/input_processors/
 * zmk,input-processor-accel.yaml, and are set at the &zip_xy_accel node in
 * torabo_tsuki_lp.dtsi / at the <&zip_xy_accel ...> call site in
 * config/keymap.keymap - no recompile needed to retune those) ===== */

// Fixed-point base for factor math: 1000 == 100%.
#define ACCEL_FACTOR_SCALE 1000
// Fixed-point base for the normalized [0,1] curve progress.
#define ACCEL_NORM_SCALE 1000

struct accel_config {
    uint32_t speed_threshold; // counts/sec
    uint32_t speed_max;       // counts/sec
    uint8_t accel_exponent;   // 1-4
    uint16_t min_dt_ms;
    uint16_t max_dt_ms;
};

struct accel_data {
    int64_t last_sample_time; // ms, 0 = no sample seen yet
};

// Returns the permille (x1000) scaling factor for the given raw event
// magnitude and elapsed-time-since-last-sample, per cfg's curve.
static int32_t compute_factor_permille(const struct accel_config *cfg, int32_t raw_abs,
                                        int32_t dt_ms, uint32_t min_factor_pct,
                                        uint32_t max_factor_pct) {
    if (dt_ms < cfg->min_dt_ms) {
        dt_ms = cfg->min_dt_ms;
    } else if (dt_ms > cfg->max_dt_ms) {
        // Gap since the last sample is long enough that this is effectively a
        // fresh, idle-start motion: treat it as slow rather than computing an
        // artificially tiny speed from a stale reference.
        dt_ms = cfg->max_dt_ms;
    }

    uint32_t speed = ((uint32_t)raw_abs * 1000U) / (uint32_t)dt_ms; // counts/sec

    uint32_t min_factor_permille = min_factor_pct * 10U;
    uint32_t max_factor_permille = max_factor_pct * 10U;

    if (speed <= cfg->speed_threshold) {
        return min_factor_permille;
    }
    if (speed >= cfg->speed_max || cfg->speed_max <= cfg->speed_threshold) {
        return max_factor_permille;
    }

    // Normalize progress between the thresholds into [0, ACCEL_NORM_SCALE].
    uint32_t range = cfg->speed_max - cfg->speed_threshold;
    uint32_t norm = ((speed - cfg->speed_threshold) * ACCEL_NORM_SCALE) / range;

    // Integer power curve: norm := norm^exponent / ACCEL_NORM_SCALE^(exponent-1)
    uint32_t curved = norm;
    for (int i = 1; i < cfg->accel_exponent; i++) {
        curved = (curved * norm) / ACCEL_NORM_SCALE;
    }

    return min_factor_permille +
           ((max_factor_permille - min_factor_permille) * curved) / ACCEL_NORM_SCALE;
}

static int accel_handle_event(const struct device *dev, struct input_event *event,
                               uint32_t param1, uint32_t param2,
                               struct zmk_input_processor_state *state) {
    // param1 = min-factor (percent), param2 = max-factor (percent), exactly
    // like zip_xy_scaler's (mult, div) cells - see <&zip_xy_accel 65 100>.
    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const struct accel_config *cfg = dev->config;
    struct accel_data *data = dev->data;

    int64_t now = k_uptime_get();
    int32_t dt_ms;
    if (data->last_sample_time == 0) {
        // First event ever seen: no reference point yet, treat as a slow start.
        dt_ms = cfg->max_dt_ms;
    } else {
        dt_ms = (int32_t)(now - data->last_sample_time);
    }

    int32_t raw = event->value;
    int32_t raw_abs = raw < 0 ? -raw : raw;

    int32_t factor_permille = compute_factor_permille(cfg, raw_abs, dt_ms, param1, param2);

    int32_t value_mul = raw * factor_permille;
    if (state && state->remainder) {
        value_mul += *state->remainder;
    }
    int32_t scaled = value_mul / ACCEL_FACTOR_SCALE;
    if (state && state->remainder) {
        *state->remainder = (int16_t)(value_mul - scaled * ACCEL_FACTOR_SCALE);
    }
    event->value = scaled;

    LOG_DBG("accel axis=%d raw=%d dt=%d factor=%d.%d%% -> %d", event->code, raw, dt_ms,
            factor_permille / 10, factor_permille % 10, scaled);

    // Advance the shared sample clock only at the end of a physical sample
    // (sync == true), so X and Y events belonging to the SAME sample compute
    // the SAME dt above (see file header comment).
    if (event->sync) {
        data->last_sample_time = now;
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api accel_driver_api = {
    .handle_event = accel_handle_event,
};

#define ACCEL_INST(n)                                                                             \
    static struct accel_data accel_data_##n = {};                                                 \
    static const struct accel_config accel_config_##n = {                                         \
        .speed_threshold = DT_INST_PROP_OR(n, speed_threshold, 600),                               \
        .speed_max = DT_INST_PROP_OR(n, speed_max, 3000),                                          \
        .accel_exponent = DT_INST_PROP_OR(n, accel_exponent, 2),                                   \
        .min_dt_ms = DT_INST_PROP_OR(n, min_dt_ms, 4),                                             \
        .max_dt_ms = DT_INST_PROP_OR(n, max_dt_ms, 100),                                           \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &accel_data_##n, &accel_config_##n, POST_KERNEL,          \
                           CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &accel_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ACCEL_INST)
