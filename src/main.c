#include <math.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WIDTH 2400
#define HEIGHT 1600
#define N_STROKES 420

uint8_t canvas[HEIGHT * WIDTH * 3];

float frand(void) { return rand() / (float)RAND_MAX; }
float frange(float lo, float hi) { return lo + frand() * (hi - lo); }

void blend_px(int x, int y, uint8_t r, uint8_t g, uint8_t b, float a) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
    return;
  }

  uint8_t *p = &canvas[(y * WIDTH + x) * 3];
  p[0] = (uint8_t)(p[0] * (1.0f - a) + r * a);
  p[1] = (uint8_t)(p[1] * (1.0f - a) + g * a);
  p[2] = (uint8_t)(p[2] * (1.0f - a) + b * a);
}
