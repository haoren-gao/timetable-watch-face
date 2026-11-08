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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>
#include "timetable_face.h"
#include "movement.h"
#include "rtc32.h"
#include "watch.h"
#include "watch_rtc.h"
#include "watch_slcd.h"
#include "watch_tcc.h"
#include "watch_utility.h"

/*
    Debug with this macro defined. Press light button to switch
    between:

    Mode Explanation
    0    no time change
    1    several seconds to midnight, yesterday
    2    several seconds before the 1st period starts, today
    3    several seconds before the 1st period finishes, today
    4    several seconds before the last period starts, today
    5    several seconds before the last period finishes, today
    6    several seconds to midnight, today

    A number will also be displayed on the right top,
    to indicate the current debug mode.
*/
//#define DEV_ON

#ifdef DEV_ON
#define REACTION_TIME 1
#define DEV_CODE(x) x
#else
#define DEV_CODE(x)
#endif

#define day_time(hour, min) (((hour) << 7) | ((min) << 1))
#define day_time_second(hour, min, sec) (((hour) << 7) | ((min) << 1) | ((sec) > 0))
#define minute(day_time) (((day_time) >> 1) & 0b111111)
#define hour(day_time) (((day_time) >> 7) & 0b11111)
#define try(func_call) do {\
    timetable_error_state_t e;\
    if ((e = func_call)){\
        return e;\
    }\
} while(false);
#define try_with_state(func_call, state, return_value) do {\
    timetable_error_state_t e;\
    if ((e = func_call)){\
        state->error_state = e;\
        return return_value;\
    }\
} while(false);

/*--------------------Config below this line--------------------*/

#define DAILY_PERIODS_COUNT 8

const char* NO_PERIOD_SYMBOL = "---"; //Only 3 char long
const char* NO_COUNTDOWN_SYMBOL = "------"; //Only 6 char long

const timetable_period_name_t TIMETABLE_BASE[7][DAILY_PERIODS_COUNT] = {
    //Set your timetable here
    //The comments are an example
    //[0] = {"CLG", "MTH", "PHY", "CPS", "CML", "PSY", "CHN", "EML"},
    //[1] = {"CML", "BIO", "GPE", "MTH", "MSC", "EML", "P E", "SRP"},
    //[2] = {"MTH", "EML", "CHN", "CHN", "PHY", "HTR", "P E", "CML"},
    //[3] = {"CHN", "CMT", "ART", "EAB", "DRM", "MTH", "CML", "MOR"},
    //[4] = {"MTH", "CHN", "EML", "EML", "CML", "SWI", "CHE", "ASB"}
};

const timetable_period_range_t PERIOD_RANGES[DAILY_PERIODS_COUNT] = {
    //Set when your periods start and finish. Remember to set all of them in time order!
    //Comments are an example
    // {
    //     .start = day_time(8, 20),
    //     .end = day_time(9, 0)
    // },
    // {
    //     .start = day_time(9, 15),
    //     .end = day_time(9, 55)
    // },
    // {
    //     .start = day_time(10, 40),
    //     .end = day_time(11, 20)
    // },
    // {
    //     .start = day_time(11, 35),
    //     .end = day_time(12, 15)
    // },
    // {
    //     .start = day_time(13, 15),
    //     .end = day_time(13, 55)
    // },
    // {
    //     .start = day_time(14, 10),
    //     .end = day_time(14, 50)
    // },
    // {
    //     .start = day_time(15, 5),
    //     .end = day_time(15, 45)
    // },
    // {
    //     .start = day_time(16, 0),
    //     .end = day_time(16, 40)
    // },
};

static void timetable_face_config_modify_period(
    timetable_period_name_t *period_name, 
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t weekday,
    uint8_t period,
    int32_t utc_offset
){
    //Prevent Warnings
    (void)period_name;(void)year;(void)month;
    (void)day;(void)weekday;(void)period;
    //If your timetable changes sometimes, write the process code here.
    //Timestamp 1792944000 is Oct 26 2026 00:00:00 GMT+0800
    uint8_t week_passed = 
        (watch_utility_convert_to_unix_time(year, month, day, 0, 0, 0, utc_offset) - 1792944000)
        / (7 * 24 * 60 * 60);
    bool is_week_b = week_passed & 1;

    if (is_week_b && weekday == 1 && period == 1){
        snprintf(
            *period_name, 
            sizeof(timetable_period_name_t), 
            "%s", 
            "CPS"
        );
    }

    if(is_week_b && weekday == 2 && period == 8){
        snprintf(
            *period_name, 
            sizeof(timetable_period_name_t), 
            "%s",
            "TUT"
        );
    }
}

/*--------------------Config above this line--------------------*/

static inline timetable_error_state_t get_period_name_safe(
    timetable_period_name_t *result, 
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t weekday,
    uint8_t period,
    int32_t utc_offset
){
    if (weekday > 7 || weekday < 1 || period > DAILY_PERIODS_COUNT || period < 1) return PERIOD_NAME_ACCESS_OUT_OF_BOUNDS;
    if (result == NULL) return NULL_PTR;

    snprintf(
        *result, 
        sizeof(*result),
        "%s",
        TIMETABLE_BASE[weekday - 1][period - 1][0] == '\0' ?
            NO_PERIOD_SYMBOL :
            TIMETABLE_BASE[weekday - 1][period - 1]
    );

    timetable_face_config_modify_period(result, year, month, day, weekday, period, utc_offset);

    return NONE;
}

static inline timetable_error_state_t get_period_start_safe(
    timetable_day_time_t *result,
    uint8_t period
){
    if(result == NULL) return NULL_PTR;
    if(period > DAILY_PERIODS_COUNT || period < 1) return PERIOD_START_ACCESS_OUT_OF_BOUNDS;
    *result = PERIOD_RANGES[period - 1].start;
    return NONE;
}

static inline timetable_error_state_t get_period_end_safe(
    timetable_day_time_t *result,
    uint8_t period
){
    if(result == NULL) return NULL_PTR;
    if(period > DAILY_PERIODS_COUNT || period < 1) return PERIOD_START_ACCESS_OUT_OF_BOUNDS;
    *result = PERIOD_RANGES[period - 1].end;
    return NONE;
}

static inline bool has_period(
    uint8_t weekday,
    uint8_t period
) {
    if(weekday < 1 || weekday > 7 || period < 1 || period > DAILY_PERIODS_COUNT) return false;
    return TIMETABLE_BASE[weekday - 1][period - 1][0] != '\0';
}

static timetable_error_state_t countdown_line_cache_update_if_counting_down(
    timetable_state_t *state, 
    uint32_t timestamp
){
    if (state == NULL) return NULL_PTR;
    if (state->no_countdown) return NONE;

    uint32_t countdown_target = state->next_full_cache_update_unix_timestamp -1;

    uint32_t seconds_to_count = countdown_target > timestamp?
        countdown_target - timestamp :
        0;
        
    if (state->in_low_energy) {
        snprintf(
            state->countdown_line_cache, 
            sizeof(state->countdown_line_cache), 
            movement_clock_mode_24h() == MOVEMENT_CLOCK_MODE_024H ?
                "%02" PRIu32 "%02" PRIu32 "%s" :
                "%2" PRIu32 "%02" PRIu32 "%s",
            seconds_to_count / 3600,
            (seconds_to_count % 3600) / 60,
            "\0\0"
        );
    } else {
        snprintf(
            state->countdown_line_cache, 
            sizeof(state->countdown_line_cache), 
            movement_clock_mode_24h() == MOVEMENT_CLOCK_MODE_024H ?
                "%02" PRIu32 "%02" PRIu32 "%02" PRIu32 : 
                "%2" PRIu32 "%02" PRIu32 "%02" PRIu32,
            seconds_to_count / 3600,
            (seconds_to_count % 3600) / 60,
            seconds_to_count % 60
        );
    }

    return NONE;
}

static inline timetable_error_state_t countdown_line_cache_shortening_update(
    timetable_state_t *state
) {
    if(state == NULL) return NULL_PTR;
    if(watch_get_lcd_type() != WATCH_LCD_TYPE_CUSTOM && state->in_low_energy){
        state->countdown_line_cache[4] = '\0';
        state->countdown_line_cache[5] = '\0';
    }

    return NONE;
}

static timetable_error_state_t full_cache_update(
    timetable_state_t *state, 
    rtc_date_time_t moment, 
    int32_t utc_offset
){
    if(state == NULL) return NULL_PTR;

    uint8_t weekday = watch_utility_get_iso8601_weekday_number(
        moment.unit.year + RTC_REFERENCE_YEAR, 
        moment.unit.month, 
        moment.unit.day
    );

    const timetable_day_time_t current_day_time =
        day_time_second(moment.unit.hour, moment.unit.minute, moment.unit.second);

    const uint32_t day_timestamp =
        watch_utility_convert_to_unix_time(
            moment.unit.year + RTC_REFERENCE_YEAR,
            moment.unit.month,
            moment.unit.day,
            0,
            0,
            0,
            utc_offset
        );

    timetable_day_time_t first_period_start;
    timetable_day_time_t last_period_start;
    timetable_day_time_t last_period_end;

    try(get_period_start_safe(&first_period_start, 1))
    try(get_period_start_safe(&last_period_start, DAILY_PERIODS_COUNT))
    try(get_period_end_safe(&last_period_end, DAILY_PERIODS_COUNT))

    if (current_day_time == day_time(0, 0)){
        state->next_full_cache_update_unix_timestamp =
            day_timestamp + 1;
        state->no_countdown = true;
        snprintf(
            state->next_period_line_cache,
            sizeof(state->next_period_line_cache), 
            "%s",
            NO_PERIOD_SYMBOL
        );
    } else if (current_day_time <= first_period_start){
        state->next_full_cache_update_unix_timestamp = day_timestamp +
            hour(first_period_start) * 3600 +
            minute(first_period_start) * 60 +
            1;

        state->no_countdown = !has_period(weekday, 1);

        try(get_period_name_safe(
            &state->next_period_line_cache, 
            moment.unit.year + RTC_REFERENCE_YEAR,
            moment.unit.month, 
            moment.unit.day,
            weekday, 
            1,
            utc_offset
        ));
    } else if (current_day_time <= last_period_start){
        uint8_t current_technical_period = 1;
        timetable_day_time_t next_period_start;

        try(get_period_start_safe(&next_period_start, current_technical_period + 1))
        while (next_period_start < current_day_time) {
            current_technical_period++;
            try(get_period_start_safe(&next_period_start, current_technical_period + 1))
        }

        timetable_day_time_t current_period_end;
        try(get_period_end_safe(&current_period_end, current_technical_period))

        timetable_day_time_t next_full_update_day_time = 
            current_day_time <= current_period_end ?
                current_period_end : 
                next_period_start;

        state->next_full_cache_update_unix_timestamp = day_timestamp +
            hour(next_full_update_day_time) * 3600 + 
            minute(next_full_update_day_time) * 60 +
            1;

        state->no_countdown = current_day_time <= current_period_end ?
            !has_period(weekday, current_technical_period) :
            !has_period(weekday, current_technical_period + 1);

        try(get_period_name_safe(
            &state->next_period_line_cache, 
            moment.unit.year + RTC_REFERENCE_YEAR,
            moment.unit.month, 
            moment.unit.day,
            weekday, 
            current_technical_period + 1,
            utc_offset
        ));
    } else if (current_day_time <= last_period_end){
        state->next_full_cache_update_unix_timestamp = day_timestamp +
            hour(last_period_end) * 3600 +
            minute(last_period_end) * 60 +
            1;

        snprintf(
            state->next_period_line_cache, 
            sizeof(state->next_period_line_cache), 
            "%s", 
            NO_PERIOD_SYMBOL
        );

        state->no_countdown = !has_period(weekday, DAILY_PERIODS_COUNT);
    } else {
        state->next_full_cache_update_unix_timestamp = day_timestamp + 24 * 60 * 60 + 1;

        state->no_countdown = true;

        snprintf(
            state->next_period_line_cache, 
            sizeof(state->next_period_line_cache), 
            "%s", 
            NO_PERIOD_SYMBOL
        );
    }

    if(state->no_countdown){
        snprintf(
            state->countdown_line_cache,
            sizeof(state->countdown_line_cache),
            "%s",
            NO_COUNTDOWN_SYMBOL
        );
        try(countdown_line_cache_shortening_update(state))
    }

    return countdown_line_cache_update_if_counting_down(
        state, 
        watch_utility_date_time_to_unix_time(moment, utc_offset)
    );
}

static timetable_error_state_t display_update(
    timetable_state_t *state,
    bool full_update,
    timetable_update_type_t update_type
){
    if(state == NULL) return NULL_PTR;
    switch (update_type) {
        case ENTERING_SLEEP:
            watch_start_sleep_animation(500);
            try(countdown_line_cache_shortening_update(state))
            watch_display_text(WATCH_POSITION_SECONDS, "  ");
            __attribute__((fallthrough));
        case SLEEPING:
            if(state->no_countdown){
                watch_clear_colon();
                watch_stop_blink();
            } else {
                if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM){
                    watch_clear_colon();
                    watch_start_indicator_blink_if_possible(WATCH_INDICATOR_COLON, 500);
                } else {
                    watch_set_colon();
                }
            }
            break;
        case WAKING_UP:
            watch_stop_sleep_animation();
            watch_stop_blink();
            __attribute__((fallthrough));
        case ACTIVATE:
            __attribute__((fallthrough));
        case TICK:
            if(!state->no_countdown) watch_set_colon(); else watch_clear_colon();
            break;
        default:
            break;
    }

    if((watch_get_lcd_type() != WATCH_LCD_TYPE_CUSTOM && state->in_low_energy))
        watch_display_text(WATCH_POSITION_SECONDS, "  ");

    watch_display_text(WATCH_POSITION_BOTTOM, state->countdown_line_cache);

    if(full_update){
        watch_display_text_with_fallback(
            WATCH_POSITION_TOP_LEFT, 
            state->next_period_line_cache, 
            state->next_period_line_cache
        );
    }
    return NONE;
}

void timetable_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void)watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(timetable_state_t));
        if (*context_ptr == NULL){
            return;
        }
        memset(*context_ptr, 0, sizeof(timetable_state_t));
        // Do any one-time tasks in here; the inside of this conditional happens only at boot.
    }

    timetable_state_t *state = (timetable_state_t *)(*context_ptr);

    if (DAILY_PERIODS_COUNT < 1 || DAILY_PERIODS_COUNT > 127){
        state->error_state = PERIODS_COUNT_OUT_OF_BOUNDS;
        return;
    }

    if(strlen(NO_PERIOD_SYMBOL)!=3){
        state->error_state = NO_PERIOD_SYMBOL_LENGTH_ILLEGAL;
        return;
    }

    if(strlen(NO_COUNTDOWN_SYMBOL)!=6){
        state->error_state = NO_COUNTDOWN_SYMBOL_LENGTH_ILLEGAL;
        return;
    }

    if (PERIOD_RANGES[DAILY_PERIODS_COUNT - 1].end > day_time_second(23, 59, 59)){
        state->error_state = PERIODS_COUNT_OUT_OF_BOUNDS;
        return;
    }

    timetable_day_time_t latest = day_time(0, 0);
    for (uint8_t i = 0;i < DAILY_PERIODS_COUNT; i++){

        timetable_day_time_t start = PERIOD_RANGES[i].start;
        if (start <= latest){
            state->error_state = PERIODS_NOT_IN_ORDER;
            return;
        }
        latest = start;

        timetable_day_time_t end = PERIOD_RANGES[i].end;
        if (end <= latest){
            state->error_state = PERIODS_NOT_IN_ORDER;
            return;
        }
        latest = end;
    }
    // Do any pin or peripheral setup here; this will be called whenever the watch wakes from deep sleep.
}

void timetable_face_activate(void *context) {
    timetable_state_t *state = (timetable_state_t *)context;

    if (context == NULL || state->error_state) return;

    rtc_date_time_t moment = movement_get_local_date_time();

    bool waking_up = state->in_low_energy;

    state->in_low_energy = false;
    
    try_with_state(full_cache_update(
        state, 
        moment, 
        movement_get_current_timezone_offset()
    ), state,)

    try_with_state(
        display_update(
            state, 
            true, 
            waking_up ? WAKING_UP : ACTIVATE
        ), 
        state,
    )
}

bool timetable_face_loop(movement_event_t event, void *context) {

    timetable_state_t *state = (timetable_state_t *)context;

    if (context == NULL || state->error_state){
        watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "ERR", "ER");
        
        char error_info[3] = {0};

        snprintf(
            error_info, 
            sizeof(error_info), 
            "%02" PRIu8,
            (uint8_t)(context == NULL ? MEMORY_ALLOCATION_FAILED : state->error_state)
        );

        watch_display_text(WATCH_POSITION_MINUTES, error_info);  

        return movement_default_loop_handler(event);
    }
    timetable_update_type_t update_type = TICK;

#ifdef DEV_ON
    bool long_press = false;
#endif

    switch (event.event_type) {
        case EVENT_LOW_ENERGY_UPDATE:
            update_type = state->in_low_energy ? SLEEPING : ENTERING_SLEEP;
            state->in_low_energy = true;
            __attribute__((fallthrough));
        case EVENT_TICK: {
            uint32_t actual_timestamp = movement_get_utc_timestamp();
            uint32_t technical_timestamp = actual_timestamp + (state->in_low_energy && (actual_timestamp % 60 == 0));
            int32_t utc_offset = movement_get_current_timezone_offset();
            rtc_date_time_t moment = watch_utility_date_time_from_unix_time(technical_timestamp, utc_offset);

            if (technical_timestamp >= state->next_full_cache_update_unix_timestamp){
                try_with_state(full_cache_update(
                    state, moment, 
                    utc_offset
                ), state, true)

                try_with_state(display_update(state, true, update_type), state, true);
            } else {

                try_with_state(countdown_line_cache_update_if_counting_down(
                    state, 
                    technical_timestamp
                ), state, true)

                try_with_state(display_update(state, state->in_low_energy, update_type), state, true);
            }
            break;
        }
        case EVENT_ALARM_LONG_PRESS: {
            timetable_face_resign(state);
            timetable_face_activate(state);
            movement_play_signal();
            break;
        }
        case EVENT_TIMEOUT: {
            movement_move_to_face(0);
            break;
        }
#ifdef DEV_ON
        case EVENT_LIGHT_LONG_PRESS:
            long_press = true;
            __attribute__((fallthrough));
        case EVENT_LIGHT_BUTTON_UP: {
            static uint8_t time_jump_marker = 0;
            time_jump_marker++;
            if(time_jump_marker > 6) time_jump_marker = 0;

            const uint32_t timestamp = movement_get_utc_timestamp();
            const int32_t utc_offset = movement_get_current_timezone_offset();
            rtc_date_time_t moment = movement_get_local_date_time();

            switch (time_jump_marker) {
                case 1:
                    moment = watch_utility_date_time_from_unix_time(timestamp - 24 * 60 * 60, utc_offset);
                    moment.unit.hour = 23;
                    moment.unit.minute = 59;
                    moment.unit.second = 59 - REACTION_TIME;
                    break;
                case 2: {
                    uint16_t day_time_min =
                        hour(PERIOD_RANGES[0].start) * 60 + minute(PERIOD_RANGES[0].start);

                    day_time_min--;

                    moment.unit.hour = day_time_min / 60;
                    moment.unit.minute = day_time_min % 60;
                    moment.unit.second = 59 - REACTION_TIME;
                    break;
                }
                case 3: {
                    uint16_t day_time_min =
                        hour(PERIOD_RANGES[0].end) * 60 + minute(PERIOD_RANGES[0].end);

                    day_time_min--;

                    moment.unit.hour = day_time_min / 60;
                    moment.unit.minute = day_time_min % 60;
                    moment.unit.second = 59 - REACTION_TIME;
                    break;
                }
                case 4: {
                    uint16_t day_time_min =
                        hour(PERIOD_RANGES[DAILY_PERIODS_COUNT - 1].start) * 60 + minute(PERIOD_RANGES[DAILY_PERIODS_COUNT - 1].start);

                    day_time_min--;

                    moment.unit.hour = day_time_min / 60;
                    moment.unit.minute = day_time_min % 60;
                    moment.unit.second = 59 - REACTION_TIME;
                    break;
                }
                case 5: {
                    uint16_t day_time_min =
                        hour(PERIOD_RANGES[DAILY_PERIODS_COUNT - 1].end) * 60 + minute(PERIOD_RANGES[DAILY_PERIODS_COUNT - 1].end);

                    day_time_min--;

                    moment.unit.hour = day_time_min / 60;
                    moment.unit.minute = day_time_min % 60;
                    moment.unit.second = 59 - REACTION_TIME;
                    break;
                }
                case 6: {
                    moment.unit.hour = 23;
                    moment.unit.minute = 59;
                    moment.unit.second = 59 - REACTION_TIME;
                    break;
                }
            }

            moment.unit.year = 6;
            moment.unit.month = 11;
            moment.unit.day = 5;
            movement_set_local_date_time(moment);
            char str[] = {time_jump_marker + '0', '\0'};
            watch_display_text(WATCH_POSITION_TOP_RIGHT, str);
            timetable_face_resign(state);
            memset(state, 0, sizeof(timetable_state_t));
            timetable_face_activate(state);
            if(long_press) movement_request_sleep();

            break;
        }
        case EVENT_LIGHT_BUTTON_DOWN:
        case EVENT_LIGHT_LONG_UP:
            break;
#endif
    }
    return movement_default_loop_handler(event);
} 

void timetable_face_resign(void* context) {
    (void)context;
}
