#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void board_init(void);
void board_present(const uint16_t *pixels);
void board_present_native(const uint16_t *pixels);
void board_brightness(uint8_t value);
// -1: I2C error, 0: released, 1: contact. Portrait coordinates.
int board_touch(int *x,int *y);
#ifdef __cplusplus
}
#endif
