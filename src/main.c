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

typedef struct {
  uint8_t r, g, b;
} Color;

const Color palette[] = {
    {20, 16, 12},    {20, 16, 12},    {20, 16, 12},
    {242, 238, 228}, {210, 158, 18},  {140, 28, 22},
    {25, 55, 100},   {148, 140, 130}, {88, 55, 28},
};
#define N_COLORS ((int)(sizeof(palette) / sizeof(palette[0])))

Color canvas[HEIGHT * WIDTH];

float frand() { return rand() / (float)RAND_MAX; }
float frange(float lo, float hi) { return lo + frand() * (hi - lo); }

void blend_px(int x, int y, Color c, float a) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
    return;
  }

  Color *px = &canvas[y * WIDTH + x];
  px->r = (uint8_t)(px->r * (1.0f - a) + c.r * a);
  px->g = (uint8_t)(px->g * (1.0f - a) + c.g * a);
  px->b = (uint8_t)(px->b * (1.0f - a) + c.b * a);
}

void stamp(float cx, float cy, float rad, Color c, float a) {
  int x0 = (int)(cx - rad) - 1, x1 = (int)(cx + rad) + 1;
  int y0 = (int)(cy - rad) - 1, y1 = (int)(cy + rad) + 1;

  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      float dx = x - cx, dy = y - cy;
      float d = sqrtf(dx * dx + dy * dy);

      if (d < rad) {
        float alpha = a * fminf(1.0f, (rad - d) / fmaxf(rad * 0.3f, 1.0f));
        blend_px(x, y, c, alpha);
      }
    }
  }
}

void splatter(float cx, float cy, float spread, Color c, int n) {
  for (int i = 0; i < n; i++) {
    float angle = frand() * 2.0f * (float)M_PI;
    float dist = sqrtf(frand()) * spread;

    stamp(cx + cosf(angle) * dist, cy + sinf(angle) * dist, frange(0.3f, 2.0f),
          c, frange(0.5f, 0.9f));
  }
}

void drip(float x, float y, float len, Color c) {
  float wobble = frange(0.0f, 2.0f * (float)M_PI);

  for (float dy = 0; dy < len; dy += 0.8f) {
    float rad = frange(0.4f, 1.2f) * (1.0f - dy / len * 0.5f);
    float a = frange(0.4f, 0.8f) * (1.0f - dy / len);
    stamp(x + sinf(dy * 0.3f + wobble) * 1.5f, y + dy, rad, c, a);
  }
}

void paint_stroke() {
  Color c = palette[rand() % N_COLORS];
  float x, y, angle;

  // 65% traversal strokes crossing the canvas, 35% freeform arcs
  if (rand() % 20 < 13) {
    int edge = rand() % 4;

    // Wide angle spread for variety
    float spread = frange(0.3f, 1.1f);
    if (edge == 0) {
      x = frange(-15, 5);
      y = frand() * HEIGHT;
      angle = frange(-spread, spread);
    } else if (edge == 1) {
      x = frange(WIDTH - 5, WIDTH + 15);
      y = frand() * HEIGHT;
      angle = (float)M_PI + frange(-spread, spread);
    } else if (edge == 2) {
      x = frand() * WIDTH;
      y = frange(-15, 5);
      angle = (float)M_PI / 2.0f + frange(-spread, spread);
    } else {
      x = frand() * WIDTH;
      y = frange(HEIGHT - 5, HEIGHT + 15);
      angle = -(float)M_PI / 2.0f + frange(-spread, spread);
    }
  } else {
    x = frand() * WIDTH;
    y = frand() * HEIGHT;
    angle = frand() * 2.0f * (float)M_PI;
  }

  float speed = frange(4.0f, 12.0f);
  float angular_vel = 0.0f;

  // Mix of thin drips, medium lines, and occasional thick blobs
  float base_rad;
  int tier = rand() % 10;
  if (tier < 5) {
    // Thin drips
    base_rad = frange(0.5f, 1.8f);
  } else if (tier < 8) {
    // Medium lines
    base_rad = frange(1.8f, 4.5f);
  } else {
    // Thick blobs
    base_rad = frange(4.5f, 9.0f);
  }

  // sine oscillators for organic waviness
  float f1 = frange(0.002f, 0.008f), p1 = frand() * 2.0f * (float)M_PI,
        a1 = frange(0.006f, 0.025f);
  float f2 = frange(0.008f, 0.030f), p2 = frand() * 2.0f * (float)M_PI,
        a2 = frange(0.002f, 0.010f);

  float target_angle = angle;
  // Varied: loose allows wide arcs, tight keeps straight
  float spring_k = frange(0.002f, 0.030f);

  // Steps sized to cross the canvas roughly 1–2 times
  float diag = sqrtf(WIDTH * WIDTH + HEIGHT * HEIGHT);
  int steps = (int)(diag / speed * frange(0.8f, 2.2f));

  for (int i = 0; i < steps; i++) {
    float rad = base_rad + sinf(i * 0.04f) * base_rad * 0.4f;
    rad = fmaxf(0.3f, rad);
    float a = frange(0.55f, 0.95f);

    if (rand() % 60 == 0) {
      stamp(x, y, rad * frange(2.0f, 4.5f), c, a * 0.5f);
    }

    stamp(x, y, rad, c, a);

    if (rand() % 25 == 0) {
      splatter(x, y, frange(8, 30), c, 2 + rand() % 6);
    }

    if (rand() % 45 == 0) {
      drip(x, y, frange(15, 60), c);
    }

    // Spring pulls back toward target_angle, oscillators add organic waviness
    float err = angle - target_angle;
    while (err > (float)M_PI) {
      err -= 2.0f * (float)M_PI;
    }

    while (err < -(float)M_PI) {
      err += 2.0f * (float)M_PI;
    }

    // Target slowly drifts
    target_angle += frange(-0.0015f, 0.0015f);

    angular_vel += -err * spring_k + a1 * sinf(i * f1 + p1) +
                   a2 * sinf(i * f2 + p2) + frange(-0.003f, 0.003f);
    angular_vel *= 0.82f;
    if (angular_vel > 0.12f) {
      angular_vel = 0.12f;
    }

    if (angular_vel < -0.12f) {
      angular_vel = -0.12f;
    }
    angle += angular_vel;

    x += cosf(angle) * speed;
    y += sinf(angle) * speed;

    // Wrap so strokes can re-enter from the opposite edge
    if (x < -100) {
      x += WIDTH + 200;
    }

    if (x > WIDTH + 100) {
      x -= WIDTH + 200;
    }

    if (y < -100) {
      y += HEIGHT + 200;
    }

    if (y > HEIGHT + 100) {
      y -= HEIGHT + 200;
    }
  }
}

int main(int argc, char *argv[]) {
  unsigned seed = (unsigned)time(NULL);
  const char *output = "pollock.png";

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = (unsigned)atoi(argv[++i]);
    } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
      output = argv[++i];
    }
  }

  srand(seed);
  printf("seed: %u\n", seed);

  // Linen/canvas background
  for (int i = 0; i < HEIGHT * WIDTH; i++) {
    canvas[i].r = 237;
    canvas[i].g = 232;
    canvas[i].b = 218;
  }

  for (int s = 0; s < N_STROKES; s++) {
    paint_stroke();
  }

  png_image img;
  memset(&img, 0, sizeof img);
  img.version = PNG_IMAGE_VERSION;
  img.format = PNG_FORMAT_RGB;
  img.width = WIDTH;
  img.height = HEIGHT;

  if (!png_image_write_to_file(&img, output, 0, canvas, 0, NULL)) {
    fprintf(stderr, "Failed to write PNG: %s\n", img.message);
    return EXIT_FAILURE;
  }

  png_image_free(&img);
}
