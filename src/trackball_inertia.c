// SPDX-License-Identifier: GPL-2.0-or-later
//
// Trackball scroll inertia, implemented as a ZMK input processor and virtual
// input device.
//
// The processor is placed after the XY-to-scroll mapper and scroll-snap, so it
// observes the exact REL_HWHEEL/REL_WHEEL values that would be sent to the host.
// Synthetic scroll events are emitted from the processor device itself and are
// consumed by a dedicated listener, avoiding a second pass through transform,
// scaling, mapping, or snapping.
//
// param1/param2 normalize the already-scaled scroll velocity back to a common
// pre-scaler speed domain. For example, a preceding 2/100 scaler uses 100/2,
// while a 40/100 scaler uses 100/40. This allows one set of thresholds and one
// shared inertia state to work across the Mac and Windows scroll profiles.
//
// macOS does not honor the HID wheel resolution multiplier for third-party
// mice, so every scroll count this processor emits is read back as one whole
// "click" (a fixed number of lines), regardless of CONFIG_ZMK_POINTING_SMOOTH_SCROLLING.
// A click train therefore only reads as continuous while consecutive clicks
// stay well under the ~100ms human perception threshold for a gap. Rather
// than let friction decay the velocity all the way down through a sparse,
// widely-spaced tail, max-click-interval-ms stops inertia outright once the
// unnormalized (actual post-scaler) click rate would fall below that floor.

#define DT_DRV_COMPAT zmk_input_processor_scroll_inertia

#include <errno.h>
#include <limits.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include <drivers/input_processor.h>

LOG_MODULE_REGISTER(trackball_scroll_inertia, CONFIG_ZMK_LOG_LEVEL);

#define INERTIA_FACTOR_SCALE 1000LL
#define VELOCITY_FP_SCALE 1000LL
#define MS_PER_SECOND 1000LL
#define MOVEMENT_DENOMINATOR (VELOCITY_FP_SCALE * MS_PER_SECOND)

struct inertia_config {
    uint32_t start_speed;
    uint32_t stop_speed;
    uint32_t max_speed;
    uint16_t friction_permille;
    uint16_t tick_ms;
    uint16_t start_delay_ms;
    uint16_t min_dt_ms;
    uint16_t sample_timeout_ms;
    uint16_t reverse_cancel_threshold;
    uint16_t max_click_interval_ms;
    uint32_t min_velocity_fp;
};

struct inertia_data {
    struct k_work_delayable tick_work;
    struct k_spinlock lock;
    const struct device *dev;

    int64_t last_sample_time;
    int32_t sample_hwheel;
    int32_t sample_wheel;
    int64_t velocity_hwheel_fp;
    int64_t velocity_wheel_fp;
    int64_t remainder_hwheel;
    int64_t remainder_wheel;
    uint32_t normalization_mult;
    uint32_t normalization_div;

    bool active;
    atomic_t generation;
    atomic_t scheduled_generation;
};

static uint64_t abs_i64(int64_t value) {
    if (value == INT64_MIN) {
        return INT64_MAX;
    }

    return value < 0 ? (uint64_t)-value : (uint64_t)value;
}

// An inexpensive vector-magnitude approximation that avoids floating point.
static uint64_t approx_speed_fp(int64_t x, int64_t y) {
    uint64_t ax = abs_i64(x);
    uint64_t ay = abs_i64(y);
    uint64_t hi = MAX(ax, ay);
    uint64_t lo = MIN(ax, ay);

    return hi + (lo / 2U);
}

static uint64_t normalized_speed_fp(int64_t x, int64_t y, uint32_t mult, uint32_t div) {
    if (div == 0U) {
        return UINT64_MAX;
    }

    return (approx_speed_fp(x, y) * mult) / div;
}

static bool axis_opposes(int32_t movement, int64_t velocity_fp, uint16_t threshold) {
    uint32_t magnitude = movement == INT32_MIN ? INT32_MAX
                                               : (movement < 0 ? (uint32_t)-movement
                                                               : (uint32_t)movement);
    if (magnitude < threshold) {
        return false;
    }

    return (movement < 0 && velocity_fp > 0) || (movement > 0 && velocity_fp < 0);
}

static void stop_locked(struct inertia_data *data) {
    data->active = false;
    data->velocity_hwheel_fp = 0;
    data->velocity_wheel_fp = 0;
    data->remainder_hwheel = 0;
    data->remainder_wheel = 0;
}

static void cap_velocity(const struct inertia_config *cfg, int64_t *x_fp, int64_t *y_fp,
                         uint32_t normalization_mult, uint32_t normalization_div) {
    uint64_t speed_fp = normalized_speed_fp(*x_fp, *y_fp, normalization_mult, normalization_div);
    uint64_t max_speed_fp = (uint64_t)cfg->max_speed * VELOCITY_FP_SCALE;

    if (speed_fp <= max_speed_fp || speed_fp == 0U) {
        return;
    }

    *x_fp = (int64_t)((*x_fp * (int64_t)max_speed_fp) / (int64_t)speed_fp);
    *y_fp = (int64_t)((*y_fp * (int64_t)max_speed_fp) / (int64_t)speed_fp);
}

static int16_t movement_for_tick(int64_t velocity_fp, uint16_t tick_ms, int64_t *remainder) {
    int64_t numerator = (velocity_fp * tick_ms) + *remainder;
    int64_t movement = numerator / MOVEMENT_DENOMINATOR;

    *remainder = numerator - (movement * MOVEMENT_DENOMINATOR);

    return (int16_t)CLAMP(movement, INT16_MIN, INT16_MAX);
}

static void inertia_tick_work(struct k_work *work) {
    struct k_work_delayable *delayable = k_work_delayable_from_work(work);
    struct inertia_data *data = CONTAINER_OF(delayable, struct inertia_data, tick_work);
    const struct inertia_config *cfg = data->dev->config;
    atomic_val_t generation = atomic_get(&data->generation);

    int16_t move_hwheel = 0;
    int16_t move_wheel = 0;
    bool continue_running = false;

    k_spinlock_key_t key = k_spin_lock(&data->lock);

    if (!data->active || atomic_get(&data->scheduled_generation) != generation) {
        k_spin_unlock(&data->lock, key);
        return;
    }

    data->velocity_hwheel_fp =
        (data->velocity_hwheel_fp * cfg->friction_permille) / INERTIA_FACTOR_SCALE;
    data->velocity_wheel_fp =
        (data->velocity_wheel_fp * cfg->friction_permille) / INERTIA_FACTOR_SCALE;

    uint64_t speed_fp = normalized_speed_fp(data->velocity_hwheel_fp, data->velocity_wheel_fp,
                                            data->normalization_mult, data->normalization_div);
    uint64_t stop_speed_fp = (uint64_t)cfg->stop_speed * VELOCITY_FP_SCALE;
    // Unnormalized (actual post-scaler) speed, used against the click-rate
    // floor below. See the file header comment on max-click-interval-ms.
    uint64_t actual_speed_fp = approx_speed_fp(data->velocity_hwheel_fp, data->velocity_wheel_fp);

    if (speed_fp < stop_speed_fp || actual_speed_fp < cfg->min_velocity_fp) {
        stop_locked(data);
        k_spin_unlock(&data->lock, key);
        return;
    } else {
        move_hwheel = movement_for_tick(data->velocity_hwheel_fp, cfg->tick_ms,
                                        &data->remainder_hwheel);
        move_wheel =
            movement_for_tick(data->velocity_wheel_fp, cfg->tick_ms, &data->remainder_wheel);
        continue_running = true;
        atomic_set(&data->scheduled_generation, generation);
    }

    k_spin_unlock(&data->lock, key);

    // A physical input may have arrived while this tick was being calculated.
    if (atomic_get(&data->generation) != generation) {
        return;
    }

    bool have_hwheel = move_hwheel != 0;
    bool have_wheel = move_wheel != 0;
    int err;

    if (have_hwheel) {
        err = input_report_rel(data->dev, INPUT_REL_HWHEEL, move_hwheel, !have_wheel, K_NO_WAIT);
        if (err < 0) {
            LOG_WRN("Failed to emit inertial horizontal scroll: %d", err);
        }
    }
    if (have_wheel) {
        err = input_report_rel(data->dev, INPUT_REL_WHEEL, move_wheel, true, K_NO_WAIT);
        if (err < 0) {
            LOG_WRN("Failed to emit inertial vertical scroll: %d", err);
        }
    }

    if (continue_running && atomic_get(&data->generation) == generation) {
        k_work_schedule(&data->tick_work, K_MSEC(cfg->tick_ms));
    }
}

static int inertia_handle_event(const struct device *dev, struct input_event *event,
                                uint32_t normalization_mult, uint32_t normalization_div,
                                struct zmk_input_processor_state *state) {
    ARG_UNUSED(state);

    const struct inertia_config *cfg = dev->config;
    struct inertia_data *data = dev->data;

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_HWHEEL && event->code != INPUT_REL_WHEEL)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (normalization_mult == 0U || normalization_div == 0U) {
        LOG_ERR("Scroll inertia normalization must be non-zero");
        return -EINVAL;
    }

    // Zero-valued reports are common after fractional scaling. They must not
    // cancel an already-armed flick, but a non-zero real scroll event must
    // invalidate any pending synthetic tick immediately.
    if (event->value != 0) {
        atomic_inc(&data->generation);
        k_work_cancel_delayable(&data->tick_work);
    }
    atomic_val_t generation = atomic_get(&data->generation);

    int64_t now = k_uptime_get();
    bool schedule = false;
    bool reversed = false;

    k_spinlock_key_t key = k_spin_lock(&data->lock);

    if (event->code == INPUT_REL_HWHEEL) {
        reversed = axis_opposes(event->value, data->velocity_hwheel_fp,
                                cfg->reverse_cancel_threshold);
        data->sample_hwheel += event->value;
    } else {
        reversed =
            axis_opposes(event->value, data->velocity_wheel_fp, cfg->reverse_cancel_threshold);
        data->sample_wheel += event->value;
    }

    if (reversed) {
        stop_locked(data);
    }

    if (event->sync) {
        bool have_previous_sample = data->last_sample_time != 0;
        int32_t dt_ms =
            have_previous_sample ? (int32_t)(now - data->last_sample_time) : 0;
        int32_t sample_hwheel = data->sample_hwheel;
        int32_t sample_wheel = data->sample_wheel;

        data->sample_hwheel = 0;
        data->sample_wheel = 0;

        // A fully zero sample only flushes the scaler's fractional remainder;
        // it is not real motion and must not alter timing or armed inertia.
        if (sample_hwheel == 0 && sample_wheel == 0) {
            k_spin_unlock(&data->lock, key);
            return ZMK_INPUT_PROC_CONTINUE;
        }

        data->last_sample_time = now;

        if (!have_previous_sample || dt_ms > cfg->sample_timeout_ms) {
            stop_locked(data);
        } else {
            if (dt_ms < cfg->min_dt_ms) {
                dt_ms = cfg->min_dt_ms;
            }

            int64_t velocity_hwheel_fp =
                ((int64_t)sample_hwheel * MS_PER_SECOND * VELOCITY_FP_SCALE) / dt_ms;
            int64_t velocity_wheel_fp =
                ((int64_t)sample_wheel * MS_PER_SECOND * VELOCITY_FP_SCALE) / dt_ms;
            uint64_t speed_fp = normalized_speed_fp(velocity_hwheel_fp, velocity_wheel_fp,
                                                    normalization_mult, normalization_div);
            uint64_t start_speed_fp = (uint64_t)cfg->start_speed * VELOCITY_FP_SCALE;
            uint64_t stop_speed_fp = (uint64_t)cfg->stop_speed * VELOCITY_FP_SCALE;
            // Actual (unnormalized) speed must also clear the click-rate
            // floor, or the tick handler would stop it on its very first
            // run. See the file header comment on max-click-interval-ms.
            uint64_t actual_speed_fp = approx_speed_fp(velocity_hwheel_fp, velocity_wheel_fp);
            bool clears_click_floor = actual_speed_fp >= cfg->min_velocity_fp;
            bool crosses_start_threshold = speed_fp >= start_speed_fp && clears_click_floor;
            bool remains_armed = data->active && speed_fp >= stop_speed_fp && clears_click_floor;

            if (crosses_start_threshold || remains_armed) {
                cap_velocity(cfg, &velocity_hwheel_fp, &velocity_wheel_fp, normalization_mult,
                             normalization_div);
                data->velocity_hwheel_fp = velocity_hwheel_fp;
                data->velocity_wheel_fp = velocity_wheel_fp;
                data->remainder_hwheel = 0;
                data->remainder_wheel = 0;
                data->normalization_mult = normalization_mult;
                data->normalization_div = normalization_div;
                data->active = true;
                atomic_set(&data->scheduled_generation, generation);
                schedule = true;

                LOG_DBG("Armed scroll inertia velocity=(%lld,%lld) normalized=%llu",
                        (long long)velocity_hwheel_fp, (long long)velocity_wheel_fp,
                        (unsigned long long)speed_fp);
            } else {
                // Slow scrolling never arms from idle. Once armed, the lower
                // stop threshold provides hysteresis through the flick's tail.
                stop_locked(data);
            }
        }
    }

    k_spin_unlock(&data->lock, key);

    if (schedule && atomic_get(&data->generation) == generation) {
        k_work_reschedule(&data->tick_work, K_MSEC(cfg->start_delay_ms));
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static int inertia_init(const struct device *dev) {
    const struct inertia_config *cfg = dev->config;
    struct inertia_data *data = dev->data;

    if (cfg->start_speed == 0U || cfg->tick_ms == 0U || cfg->start_delay_ms == 0U ||
        cfg->min_dt_ms == 0U || cfg->sample_timeout_ms < cfg->min_dt_ms ||
        cfg->friction_permille >= INERTIA_FACTOR_SCALE ||
        cfg->stop_speed >= cfg->start_speed || cfg->max_speed < cfg->start_speed ||
        cfg->max_click_interval_ms == 0U || cfg->max_click_interval_ms < cfg->tick_ms) {
        LOG_ERR("Invalid trackball scroll inertia configuration");
        return -EINVAL;
    }

    data->dev = dev;
    k_work_init_delayable(&data->tick_work, inertia_tick_work);
    atomic_set(&data->generation, 0);
    atomic_set(&data->scheduled_generation, 0);

    return 0;
}

static const struct zmk_input_processor_driver_api inertia_driver_api = {
    .handle_event = inertia_handle_event,
};

#define INERTIA_INST(n)                                                                            \
    static struct inertia_data inertia_data_##n = {};                                             \
    static const struct inertia_config inertia_config_##n = {                                     \
        .start_speed = DT_INST_PROP(n, start_speed),                                              \
        .stop_speed = DT_INST_PROP(n, stop_speed),                                                \
        .max_speed = DT_INST_PROP(n, max_speed),                                                  \
        .friction_permille = DT_INST_PROP(n, friction_permille),                                  \
        .tick_ms = DT_INST_PROP(n, tick_ms),                                                      \
        .start_delay_ms = DT_INST_PROP(n, start_delay_ms),                                        \
        .min_dt_ms = DT_INST_PROP(n, min_dt_ms),                                                  \
        .sample_timeout_ms = DT_INST_PROP(n, sample_timeout_ms),                                  \
        .reverse_cancel_threshold = DT_INST_PROP(n, reverse_cancel_threshold),                    \
        .max_click_interval_ms = DT_INST_PROP(n, max_click_interval_ms),                          \
        .min_velocity_fp = DIV_ROUND_UP(VELOCITY_FP_SCALE * MS_PER_SECOND,                        \
                                        DT_INST_PROP(n, max_click_interval_ms)),                  \
    };                                                                                            \
    DEVICE_DT_INST_DEFINE(n, inertia_init, NULL, &inertia_data_##n, &inertia_config_##n,          \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &inertia_driver_api);

DT_INST_FOREACH_STATUS_OKAY(INERTIA_INST)
