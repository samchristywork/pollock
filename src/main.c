#include <math.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  uint8_t r, g, b;
} Color;

const Color palette[] = {
    {20, 16, 12},    {20, 16, 12},    {20, 16, 12},
    {242, 238, 228}, {210, 158, 18},  {140, 28, 22},
    {25, 55, 100},   {148, 140, 130}, {88, 55, 28},
};
#define N_COLORS ((int)(sizeof(palette) / sizeof(palette[0])))

int width = 2400;
int height = 1600;
int n_strokes = 420;

float frand() { return rand() / (float)RAND_MAX; }
float frange(float lo, float hi) { return lo + frand() * (hi - lo); }

void blend_px(Color *canvas, int x, int y, Color c, float a) {
  if (x < 0 || x >= width || y < 0 || y >= height) {
    return;
  }

  Color *px = &canvas[y * width + x];
  px->r = (uint8_t)(px->r * (1.0f - a) + c.r * a);
  px->g = (uint8_t)(px->g * (1.0f - a) + c.g * a);
  px->b = (uint8_t)(px->b * (1.0f - a) + c.b * a);
}

void stamp(Color *canvas, float cx, float cy, float rad, Color c, float a) {
  int x0 = (int)(cx - rad) - 1, x1 = (int)(cx + rad) + 1;
  int y0 = (int)(cy - rad) - 1, y1 = (int)(cy + rad) + 1;

  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      float dx = x - cx, dy = y - cy;
      float d = sqrtf(dx * dx + dy * dy);

      if (d < rad) {
        float alpha = a * fminf(1.0f, (rad - d) / fmaxf(rad * 0.3f, 1.0f));
        blend_px(canvas, x, y, c, alpha);
      }
    }
  }
}

void splatter(Color *canvas, float cx, float cy, float spread, Color c, int n) {
  for (int i = 0; i < n; i++) {
    float angle = frand() * 2.0f * (float)M_PI;
    float dist = sqrtf(frand()) * spread;

    stamp(canvas, cx + cosf(angle) * dist, cy + sinf(angle) * dist,
          frange(0.3f, 2.0f), c, frange(0.5f, 0.9f));
  }
}

void drip(Color *canvas, float x, float y, float len, Color c) {
  float wobble = frange(0.0f, 2.0f * (float)M_PI);

  for (float dy = 0; dy < len; dy += 0.8f) {
    float rad = frange(0.4f, 1.2f) * (1.0f - dy / len * 0.5f);
    float a = frange(0.4f, 0.8f) * (1.0f - dy / len);
    stamp(canvas, x + sinf(dy * 0.3f + wobble) * 1.5f, y + dy, rad, c, a);
  }
}

// 65% traversal strokes entering from an edge, 35% freeform arcs
void pick_origin(float *x, float *y, float *angle) {
  if (rand() % 20 < 13) {
    float spread = frange(0.3f, 1.1f);
    switch (rand() % 4) {
    case 0:
      *x = frange(-15, 5);
      *y = frand() * height;
      *angle = frange(-spread, spread);
      break;
    case 1:
      *x = frange(width - 5, width + 15);
      *y = frand() * height;
      *angle = (float)M_PI + frange(-spread, spread);
      break;
    case 2:
      *x = frand() * width;
      *y = frange(-15, 5);
      *angle = (float)M_PI / 2.0f + frange(-spread, spread);
      break;
    default:
      *x = frand() * width;
      *y = frange(height - 5, height + 15);
      *angle = -(float)M_PI / 2.0f + frange(-spread, spread);
      break;
    }
  } else {
    *x = frand() * width;
    *y = frand() * height;
    *angle = frand() * 2.0f * (float)M_PI;
  }
}

// 50% thin, 30% medium, 20% thick
float pick_base_rad() {
  int tier = rand() % 10;
  if (tier < 5)
    return frange(0.5f, 1.8f);
  if (tier < 8)
    return frange(1.8f, 4.5f);
  return frange(4.5f, 9.0f);
}

void paint_stroke(Color *canvas) {
  Color c = palette[rand() % N_COLORS];
  float x, y, angle;
  pick_origin(&x, &y, &angle);

  float speed = frange(4.0f, 12.0f);
  float angular_vel = 0.0f;
  float base_rad = pick_base_rad();

  // sine oscillators for organic waviness
  float f1 = frange(0.002f, 0.008f), p1 = frand() * 2.0f * (float)M_PI,
        a1 = frange(0.006f, 0.025f);
  float f2 = frange(0.008f, 0.030f), p2 = frand() * 2.0f * (float)M_PI,
        a2 = frange(0.002f, 0.010f);

  float target_angle = angle;
  // Varied: loose allows wide arcs, tight keeps straight
  float spring_k = frange(0.002f, 0.030f);

  // Steps sized to cross the canvas roughly 1–2 times
  float diag = sqrtf((float)(width * width + height * height));
  int steps = (int)(diag / speed * frange(0.8f, 2.2f));

  for (int i = 0; i < steps; i++) {
    float rad = base_rad + sinf(i * 0.04f) * base_rad * 0.4f;
    rad = fmaxf(0.3f, rad);
    float a = frange(0.55f, 0.95f);

    if (rand() % 60 == 0) {
      stamp(canvas, x, y, rad * frange(2.0f, 4.5f), c, a * 0.5f);
    }

    stamp(canvas, x, y, rad, c, a);

    if (rand() % 25 == 0) {
      splatter(canvas, x, y, frange(8, 30), c, 2 + rand() % 6);
    }

    if (rand() % 45 == 0) {
      drip(canvas, x, y, frange(15, 60), c);
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
    angular_vel = fmaxf(-0.12f, fminf(0.12f, angular_vel));
    angle += angular_vel;

    x += cosf(angle) * speed;
    y += sinf(angle) * speed;

    // Wrap so strokes can re-enter from the opposite edge
    if (x < -100) {
      x += width + 200;
    }

    if (x > width + 100) {
      x -= width + 200;
    }

    if (y < -100) {
      y += height + 200;
    }

    if (y > height + 100) {
      y -= height + 200;
    }
  }
}

int main(int argc, char *argv[]) {
  unsigned seed = (unsigned)time(NULL);
  const char *output = "pollock.png";
  Color bg = {237, 232, 218};

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = (unsigned)atoi(argv[++i]);
    } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
      output = argv[++i];
    } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
      width = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
      height = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--strokes") == 0 && i + 1 < argc) {
      n_strokes = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--background") == 0 && i + 1 < argc) {
      unsigned int r, g, b;
      if (sscanf(argv[++i], "%u,%u,%u", &r, &g, &b) == 3) {
        bg = (Color){r & 0xff, g & 0xff, b & 0xff};
      }
    }
  }

  srand(seed);
  printf("seed: %u\n", seed);

  Color *canvas = malloc(width * height * sizeof(Color));
  if (!canvas) {
    fprintf(stderr, "Failed to allocate canvas\n");
    return EXIT_FAILURE;
  }

  for (int i = 0; i < height * width; i++) {
    canvas[i] = bg;
  }

  for (int s = 0; s < n_strokes; s++) {
    paint_stroke(canvas);
  }

  png_image img;
  memset(&img, 0, sizeof img);
  img.version = PNG_IMAGE_VERSION;
  img.format = PNG_FORMAT_RGB;
  img.width = width;
  img.height = height;

  int ok = png_image_write_to_file(&img, output, 0, canvas, 0, NULL);
  if (!ok) {
    fprintf(stderr, "Failed to write PNG: %s\n", img.message);
  }

  png_image_free(&img);
  free(canvas);
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
