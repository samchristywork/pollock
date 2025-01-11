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

void stamp(float cx, float cy, float rad, uint8_t r, uint8_t g, uint8_t b,
           float alpha) {
  int x0 = (int)(cx - rad) - 1, x1 = (int)(cx + rad) + 1;
  int y0 = (int)(cy - rad) - 1, y1 = (int)(cy + rad) + 1;

  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      float dx = x - cx, dy = y - cy;
      float d = sqrtf(dx * dx + dy * dy);

      if (d < rad) {
        float a = alpha * fminf(1.0f, (rad - d) / fmaxf(rad * 0.3f, 1.0f));
        blend_px(x, y, r, g, b, a);
      }
    }
  }
}

void splatter(float cx, float cy, float spread, uint8_t r, uint8_t g, uint8_t b,
              int n) {
  for (int i = 0; i < n; i++) {
    float angle = frand() * 2.0f * (float)M_PI;
    float dist = sqrtf(frand()) * spread;

    stamp(cx + cosf(angle) * dist, cy + sinf(angle) * dist, frange(0.3f, 2.0f),
          r, g, b, frange(0.5f, 0.9f));
  }
}

void drip(float x, float y, float len, uint8_t r, uint8_t g, uint8_t b) {
  float wobble = frange(0.0f, 6.28f);

  for (float dy = 0; dy < len; dy += 0.8f) {
    float rad = frange(0.4f, 1.2f) * (1.0f - dy / len * 0.5f);
    float alpha = frange(0.4f, 0.8f) * (1.0f - dy / len);

    stamp(x + sinf(dy * 0.3f + wobble) * 1.5f, y + dy, rad, r, g, b, alpha);
  }
}
