#pragma once
// Copy to display_settings.h and edit locally. Night mode is off by default.
#define DECK_NIGHT_MODE_ENABLED 0
#define DECK_DAY_BRIGHTNESS_PERCENT 85
#define DECK_NIGHT_BRIGHTNESS_PERCENT 20
#define DECK_TOUCH_BRIGHTNESS_PERCENT 100
// Minutes after local midnight. These defaults mean 22:30 to 07:00.
#define DECK_NIGHT_START_MINUTE (22*60+30)
#define DECK_NIGHT_END_MINUTE (7*60)
#define DECK_NIGHT_TOUCH_TIMEOUT_MS 30000
// 1 = at night the first touch on the dim screen only wakes it (no swipe/tap).
#define DECK_NIGHT_WAKE_ONLY_TOUCH 1
// 1 = slowly shift the image up to 2 px to reduce AMOLED burn-in. Off by default.
#define DECK_PIXEL_SHIFT 0
