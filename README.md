# Timetable Watch Face for Sensor Watch

A customizable school timetable watch face for [Sensor Watch](https://www.sensorwatch.net/), built with [Second Movement](https://github.com/joeycastillo/second-movement).

This watch face shows your upcoming class and a countdown based on your daily timetable. You can customize the timetable to match your own school schedule.

## Features

- **Daily timetable:** Set different class schedules for each day of the week.
- **Custom period times:** Set the start and end time of every period.
- **Schedule changes:** Support special schedules, alternating weeks, and date-based changes.
- **Countdown:** Display a live countdown to the next timetable change.
- **Automatic updates:** Update the displayed information when the schedule changes.
- **Sleep support:** Handle low-energy mode and wake-up events.
- **Configuration checks:** Detect invalid timetable settings and display error codes.
- **LCD support:** Include display handling for Custom LCD and other supported LCD types.

## Compatibility

- Firmware: Second Movement
- Target hardware: Sensor Watch Pro with Custom LCD
- Real hardware testing: Tested on a physical watch.

Compatibility with all hardware and firmware versions has not been confirmed.

## Installation

This is a watch face source code project, not a standalone firmware file.

To use it:

1. Clone the [Second Movement repository](https://github.com/joeycastillo/second-movement).
2. Copy `timetable_face.c` and `timetable_face.h` into the appropriate watch-face source directory. (/second-movement/watch-faces/complication suggested)
3. Add the source file to the firmware build configuration.
4. Register the watch face and its callbacks in the face registry.
5. Enable the watch face in the firmware configuration.
6. Build the firmware and flash it to your watch.

The exact integration steps may depend on your Second Movement version.

## Configuration

All timetable settings are located in the configuration section of `timetable_face.c`:

```c
/*--------------------Config below this line--------------------*/
```

You can customize the following settings.

### 1. Number of periods

```c
#define DAILY_PERIODS_COUNT 8
```

Set the number of periods in one day.

The supported range is 1 to 127.

### 2. Empty period symbols

```c
const char* NO_PERIOD_SYMBOL = "---";
const char* NO_COUNTDOWN_SYMBOL = "------";
```

These symbols are displayed when there is no class or no countdown.

Keep their lengths exactly as shown:

- `NO_PERIOD_SYMBOL`: 3 characters.
- `NO_COUNTDOWN_SYMBOL`: 6 characters.

### 3. Weekly timetable

```c
const timetable_period_name_t TIMETABLE_BASE[7][DAILY_PERIODS_COUNT] = {
    [0] = {"...", "...", "..."},
    [1] = {"...", "...", "..."},
    // ...
};
```

Set the class name for each period.

The seven rows represent Monday to Sunday, following the ISO weekday numbering system.

An empty string (`""`) means that there is no class in that period.

### 4. Period start and end times

```c
const timetable_period_range_t PERIOD_RANGES[DAILY_PERIODS_COUNT] = {
    {
        .start = day_time(8, 0),
        .end = day_time(8, 45)
    },
    // ...
};
```

Set the start and end time of every period.

**Important:**

- All periods must be arranged in chronological order.
- Periods must not overlap.
- All period times must be within the same day.
- The number of time ranges must match `DAILY_PERIODS_COUNT`.

### 5. Special timetable changes

```c
static void timetable_face_config_modify_period(...)
```

Use this function to modify class names based on the date, weekday, or period number.

It can be used for:

- Alternating A/B weeks.
- Special school days.
- Temporary timetable changes.
- Other date-based rules.

The function also receives the current UTC offset.

## Error Handling

The watch face checks several configuration errors during setup, including:

- Invalid period count.
- Incorrect placeholder lengths.
- Periods outside the valid time range.
- Periods that are not in chronological order.

If an error is detected, the watch displays `ERR` and an error code.

## Development Status

This project is being prepared for its first public release.

The current source contains development-only test controls behind `DEV_ON`. These controls can change the watch's local date and time for testing.

Disable or remove these controls before building a normal release firmware.

## License

This project is released under the MIT License. See [LICENSE](LICENSE) for details.
