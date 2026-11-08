/*
 * MIT License
 *
 * Copyright (c) 2026 HaoRen Gao<haoren.gao@outlook.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include "movement.h"
#include <stdint.h>

/*
 * Timetable Face
 * 
 * This watch face is designed to answer three questions:
 * - What's the next period?
 * - How long is it until the current period finishes?
 * - How long is it until the next period starts?
 *
 * Therefore, the display is divided into two main area:
 * - Next Period: At the top, showing what's the next period
 * - Countdown: At the bottom, showing the countdown to the current period's end or to the next period's start
 */

 /*
  * Data 0 0 0 ... 1
  * Bit  1 2 3 ... 16
  * 
  * Bit 16       Second (0 or 1, 0 means precisely 0, 1 means bigger than 0)
  * Bit 10 - 15  Minute (0 - 59)
  * Bit 5 - 9    Hour   (0 - 23)
  */
typedef uint16_t timetable_day_time_t;

typedef char timetable_period_name_t[4];

typedef char timetable_countdown_t[7];

typedef struct {
    timetable_day_time_t start;
    timetable_day_time_t end;
} timetable_period_range_t;

typedef enum {
    NONE = 0,
    NO_COUNTDOWN_SYMBOL_LENGTH_ILLEGAL,
    NO_PERIOD_SYMBOL_LENGTH_ILLEGAL,
    PERIODS_COUNT_OUT_OF_BOUNDS,
    PERIODS_NOT_IN_ORDER,
    NULL_PTR,
    MEMORY_ALLOCATION_FAILED,
    PERIOD_NAME_ACCESS_OUT_OF_BOUNDS,
    PERIOD_START_ACCESS_OUT_OF_BOUNDS,
} timetable_error_state_t;

typedef enum {
    TICK = 0,
    ACTIVATE,
    ENTERING_SLEEP,
    SLEEPING,
    WAKING_UP
} timetable_update_type_t;

typedef struct {
    // Anything you need to keep track of, put it here!
    timetable_error_state_t error_state;
    uint32_t next_full_cache_update_unix_timestamp;
    timetable_period_name_t next_period_line_cache;
    timetable_countdown_t countdown_line_cache;
    bool no_countdown;
    bool in_low_energy;
} timetable_state_t;
 
void timetable_face_setup(uint8_t watch_face_index, void ** context_ptr);
void timetable_face_activate(void *context);
bool timetable_face_loop(movement_event_t event, void *context);
void timetable_face_resign(void* context);

#define timetable_face ((const watch_face_t){ \
    timetable_face_setup, \
    timetable_face_activate, \
    timetable_face_loop, \
    timetable_face_resign, \
    NULL, \
})
