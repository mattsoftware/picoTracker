#include "mode0.h"
#include "hardware/spi.h"
#include "ili9341.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

/* Character graphics mode */

// Fonts (public domain): https://github.com/dhepper/font8x8
// TODO: optimize by having the characters on a bitstream array already
// like in Adapters/SDL/GUI/SDLGUIWindowImp.cpp
static const uint16_t font_data[95][10] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0020 (space)
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00, 0x00, 0x00}, // U+0021 (!)
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0022 (")
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00, 0x00, 0x00}, // U+0023 (#)
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00, 0x00, 0x00}, // U+0024 ($)
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00, 0x00, 0x00}, // U+0025 (%)
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00, 0x00, 0x00}, // U+0026 (&)
    {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0027 (')
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00, 0x00, 0x00}, // U+0028 (()
    {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00, 0x00, 0x00}, // U+0029 ())
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00}, // U+002A (*)
    {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0x00}, // U+002B (+)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06, 0x00, 0x00}, // U+002C (,)
    {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+002D (-)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x00}, // U+002E (.)
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00, 0x00, 0x00}, // U+002F (/)
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00, 0x00, 0x00}, // U+0030 (0)
    {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00, 0x00, 0x00}, // U+0031 (1)
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00, 0x00, 0x00}, // U+0032 (2)
    {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00, 0x00, 0x00}, // U+0033 (3)
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00, 0x00, 0x00}, // U+0034 (4)
    {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00, 0x00, 0x00}, // U+0035 (5)
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00, 0x00, 0x00}, // U+0036 (6)
    {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00, 0x00, 0x00}, // U+0037 (7)
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00, 0x00, 0x00}, // U+0038 (8)
    {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00, 0x00, 0x00}, // U+0039 (9)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x00}, // U+003A (:)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06, 0x00, 0x00}, // U+003B (;)
    {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00, 0x00, 0x00}, // U+003C (<)
    {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00}, // U+003D (=)
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00, 0x00, 0x00}, // U+003E (>)
    {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00, 0x00, 0x00}, // U+003F (?)
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00, 0x00, 0x00}, // U+0040 (@)
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00, 0x00, 0x00}, // U+0041 (A)
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00, 0x00, 0x00}, // U+0042 (B)
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00, 0x00, 0x00}, // U+0043 (C)
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00, 0x00, 0x00}, // U+0044 (D)
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00, 0x00, 0x00}, // U+0045 (E)
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00, 0x00, 0x00}, // U+0046 (F)
    {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00, 0x00, 0x00}, // U+0047 (G)
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00, 0x00, 0x00}, // U+0048 (H)
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00, 0x00, 0x00}, // U+0049 (I)
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00, 0x00, 0x00}, // U+004A (J)
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00, 0x00, 0x00}, // U+004B (K)
    {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00, 0x00, 0x00}, // U+004C (L)
    {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00, 0x00, 0x00}, // U+004D (M)
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00, 0x00, 0x00}, // U+004E (N)
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00, 0x00, 0x00}, // U+004F (O)
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00, 0x00, 0x00}, // U+0050 (P)
    {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00, 0x00, 0x00}, // U+0051 (Q)
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00, 0x00, 0x00}, // U+0052 (R)
    {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00, 0x00, 0x00}, // U+0053 (S)
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00, 0x00, 0x00}, // U+0054 (T)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00, 0x00, 0x00}, // U+0055 (U)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00, 0x00, 0x00}, // U+0056 (V)
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00, 0x00, 0x00}, // U+0057 (W)
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00}, // U+0058 (X)
    {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00, 0x00, 0x00}, // U+0059 (Y)
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00, 0x00, 0x00}, // U+005A (Z)
    {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00, 0x00, 0x00}, // U+005B ([)
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00, 0x00, 0x00}, // U+005C (\)
    {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00, 0x00, 0x00}, // U+005D (])
    {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+005E (^)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00}, // U+005F (_)
    {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0060 (`)
    {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00, 0x00, 0x00}, // U+0061 (a)
    {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00, 0x00, 0x00}, // U+0062 (b)
    {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00, 0x00, 0x00}, // U+0063 (c)
    {0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00, 0x00, 0x00}, // U+0064 (d)
    {0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00, 0x00, 0x00}, // U+0065 (e)
    {0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00, 0x00, 0x00}, // U+0066 (f)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F, 0x00, 0x00}, // U+0067 (g)
    {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00, 0x00, 0x00}, // U+0068 (h)
    {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00, 0x00, 0x00}, // U+0069 (i)
    {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00, 0x00}, // U+006A (j)
    {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00, 0x00, 0x00}, // U+006B (k)
    {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00, 0x00, 0x00}, // U+006C (l)
    {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00, 0x00, 0x00}, // U+006D (m)
    {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00, 0x00, 0x00}, // U+006E (n)
    {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00, 0x00, 0x00}, // U+006F (o)
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F, 0x00, 0x00}, // U+0070 (p)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78, 0x00, 0x00}, // U+0071 (q)
    {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00, 0x00, 0x00}, // U+0072 (r)
    {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00, 0x00, 0x00}, // U+0073 (s)
    {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00, 0x00, 0x00}, // U+0074 (t)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00, 0x00, 0x00}, // U+0075 (u)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00, 0x00, 0x00}, // U+0076 (v)
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00, 0x00, 0x00}, // U+0077 (w)
    {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00}, // U+0078 (x)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F, 0x00, 0x00}, // U+0079 (y)
    {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00, 0x00, 0x00}, // U+007A (z)
    {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00, 0x00, 0x00}, // U+007B ({)
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00}, // U+007C (|)
    {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00, 0x00, 0x00}, // U+007D (})
    {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+007E (~)
};

#define TEXT_WIDTH 32
#define TEXT_HEIGHT 24
#define CHAR_HEIGHT 10
#define CHAR_WIDTH 10
#define BUFFER_CHARS 12

#define SWAP_BYTES(color) ((uint16_t)(color >> 8) | (uint16_t)(color << 8))

static mode0_color_t screen_bg_color = MODE0_BG;
static mode0_color_t screen_fg_color = MODE0_NORMAL;
static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t screen[TEXT_HEIGHT * TEXT_WIDTH] = {0};
static uint8_t colors[TEXT_HEIGHT * TEXT_WIDTH] = {0};
static uint16_t buffer[CHAR_HEIGHT * CHAR_WIDTH * BUFFER_CHARS] = {0};

// Using a bit array in order to save memory, there is a slight performance
// hit in doing so vs a bool array
static uint8_t changed[TEXT_HEIGHT * TEXT_WIDTH / 8] = {0};
#define SetBit(A, k) (A[(k) / 8] |= (1 << ((k) % 8)))
#define ClearBit(A, k) (A[(k) / 8] &= ~(1 << ((k) % 8)))
#define TestBit(A, k) (A[(k) / 8] & (1 << ((k) % 8)))

// Default palette, can be redefined
static uint16_t palette[16] = {
    SWAP_BYTES(0x0000), SWAP_BYTES(0x49E5), SWAP_BYTES(0xB926),
    SWAP_BYTES(0xE371), SWAP_BYTES(0x9CF3), SWAP_BYTES(0xA324),
    SWAP_BYTES(0xEC46), SWAP_BYTES(0xF70D), SWAP_BYTES(0xffff),
    SWAP_BYTES(0x1926), SWAP_BYTES(0x2A49), SWAP_BYTES(0x4443),
    SWAP_BYTES(0xA664), SWAP_BYTES(0x02B0), SWAP_BYTES(0x351E),
    SWAP_BYTES(0xB6FD)};

void mode0_clear(mode0_color_t color) {
  int size = TEXT_WIDTH * TEXT_HEIGHT;
  memset(screen, 0, size);
  memset(colors, color, size);
  mode0_set_cursor(0, 0);
  mode0_draw_screen();
}

void mode0_set_foreground(mode0_color_t color) { screen_fg_color = color; }

void mode0_set_background(mode0_color_t color) { screen_bg_color = color; }

void mode0_set_cursor(uint8_t x, uint8_t y) {
  cursor_x = x;
  cursor_y = y;
}

uint8_t mode0_get_cursor_x() { return cursor_x; }

uint8_t mode0_get_cursor_y() { return cursor_y; }

void mode0_putc(char c, bool invert) {
  int idx = cursor_y * TEXT_WIDTH + cursor_x;
  if (c >= 32 && c <= 127) {
    screen[idx] = c - 32;
    SetBit(changed, idx);
    if (invert) {
      colors[idx] = ((screen_bg_color & 0xf) << 4) | (screen_fg_color & 0xf);
    } else {
      colors[idx] = ((screen_fg_color & 0xf) << 4) | (screen_bg_color & 0xf);
    }
  }
}

void mode0_print(const char *str, bool invert) {
  char c;
  while ((c = *str++)) {
    mode0_putc(c, invert);
  }
}

void mode0_write(const char *str, int len, bool invert) {
  for (int i = 0; i < len; i++) {
    mode0_putc(*str++, invert);
  }
}


void mode0_draw_region(uint8_t x, uint8_t y, uint8_t width, uint8_t height) {

  int remainder = height;
  while (remainder) {
    int sub_height = (remainder > BUFFER_CHARS) ? BUFFER_CHARS:remainder;
    int sub_y = y + height - remainder;
    remainder -= sub_height;
    mode0_draw_sub_region(x, sub_y, width, sub_height);
  }
}

inline void mode0_draw_sub_region(uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
  assert(height <= BUFFER_CHARS);

  uint16_t screen_x = x * CHAR_WIDTH;
  uint16_t screen_y = (TEXT_HEIGHT - height - y) * CHAR_HEIGHT;
  uint16_t screen_width = width * CHAR_HEIGHT;
  uint16_t screen_height = height * CHAR_HEIGHT;

  // column address set
  ili9341_set_command(ILI9341_CASET);
  ili9341_command_param16(screen_y);
  ili9341_command_param16(screen_y + screen_height - 1);

  // page address set
  ili9341_set_command(ILI9341_PASET);
  ili9341_command_param16(screen_x);
  ili9341_command_param16(screen_x + screen_width - 1);

  // start writing
  ili9341_set_command(ILI9341_RAMWR);

  ili9341_start_writing();

  for (int page = x; page < x + width; page++) {
    // create one column of screen information
    uint16_t *buffer_idx = buffer;

    for (int bit = CHAR_WIDTH - 1; bit >= 0; bit--) {
      uint8_t mask = 1 << (CHAR_WIDTH - 1 - bit);
      for (int col = y + height - 1; col >= y; col--) {
        int16_t idx = col * TEXT_WIDTH + page;
        uint8_t character = screen[idx];
        uint16_t fg_color = palette[colors[idx] >> 4];
        uint16_t bg_color = palette[colors[idx] & 0xf];

        const uint16_t *pixel_data = font_data[character];

        // draw the character into the buffer
        for (int j = CHAR_HEIGHT - 1; j >= 0; j--) {
          *buffer_idx++ = (pixel_data[j] & mask) ? fg_color : bg_color;
        }
      }
    }
    ili9341_write_data_continuous(buffer, CHAR_WIDTH * screen_height * sizeof(int16_t));
  }
  ili9341_stop_writing();
}

void mode0_draw_changed() {
  for (int idx = 0; idx < TEXT_HEIGHT * TEXT_WIDTH; idx++) {
    if (TestBit(changed, idx)) {
      ClearBit(changed, idx);
      // check adjacent in order to find bigger rectangle
      uint16_t y = idx / TEXT_WIDTH;
      uint16_t x = idx - (TEXT_WIDTH * y);

      int height = 1;
      // first pass tests the height
      for (int probe_y = y + 1; probe_y < TEXT_HEIGHT; probe_y++) {
        int probe_idx = probe_y * TEXT_WIDTH + x;
        if (TestBit(changed, probe_idx)) {
          ClearBit(changed, probe_idx);
          height++;
          continue;
        }
        break;
      }

      int16_t width = 1;
      // having the height, we can test every subsequent column
      for (int probe_x = x + 1; probe_x < TEXT_WIDTH; probe_x++) {
        for (int probe_y = y; probe_y < y + height; probe_y++) {
          // if we don't get to max height, then abort
          int probe_idx = probe_y * TEXT_WIDTH + probe_x;
          if (!TestBit(changed, probe_idx)) {
            // undo last column
            for (int undo_y = y; undo_y < probe_y; undo_y++) {
              SetBit(changed, undo_y * TEXT_WIDTH + probe_x);
            }
            goto end;
          }
          ClearBit(changed, probe_idx);
        }
        width++;
      }
    end:
      mode0_draw_region(x, y, width, height);
    }
  }
}

void mode0_draw_changed_simple() {
  // This method is better (faster) for fewer characters chnaged
  for (int idx = 0; idx < TEXT_HEIGHT * TEXT_WIDTH; idx++) {
    if (TestBit(changed, idx)) {
      ClearBit(changed, idx);
      uint16_t y = idx / TEXT_WIDTH;
      uint16_t x = idx - (TEXT_WIDTH * y);
      mode0_draw_region(x, y, 1, 1);
    }
  }
}

void mode0_draw_screen() {
  // draw the whole screen
  mode0_draw_region(0, 0, TEXT_WIDTH, TEXT_HEIGHT);
}

void mode0_set_palette_color(int idx, uint16_t rgb565_color) {
  palette[idx] = SWAP_BYTES(rgb565_color);
}

void mode0_init() { ili9341_init(); }