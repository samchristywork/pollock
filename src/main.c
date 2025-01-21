#include <math.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
  uint8_t r, g, b;
} Color;

static const Color default_palette[] = {
    {20, 16, 12},    {20, 16, 12},    {20, 16, 12},
    {242, 238, 228}, {210, 158, 18},  {140, 28, 22},
    {25, 55, 100},   {148, 140, 130}, {88, 55, 28},
};
#define N_DEFAULT_COLORS                                                       \
  ((int)(sizeof(default_palette) / sizeof(default_palette[0])))

const Color *palette = default_palette;
int n_colors = N_DEFAULT_COLORS;

int width = 2400;
int height = 1600;
int n_strokes = 420;
float splatter_prob = 1.0f / 25.0f;
float drip_prob = 1.0f / 45.0f;

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

static Color *load_palette(const char *path, int *out_n) {
  FILE *f = fopen(path, "r");
  if (!f) {
    fprintf(stderr, "Cannot open palette file: %s\n", path);
    return NULL;
  }

  Color buf[256];
  int n = 0;
  char line[64];
  int lineno = 0;

  while (fgets(line, sizeof(line), f)) {
    lineno++;
    size_t len = strlen(line);
    int truncated = (len == sizeof(line) - 1 && line[len - 1] != '\n');
    if (truncated) {
      int ch;
      while ((ch = fgetc(f)) != '\n' && ch != EOF)
        ;
    }
    char *p = line;
    while (*p == ' ' || *p == '\t')
      p++;
    if (*p == '\n' || *p == '\0' || *p == '#')
      continue;

    unsigned int r, g, b;
    if (sscanf(p, "%u,%u,%u", &r, &g, &b) != 3 || r > 255 || g > 255 ||
        b > 255) {
      fprintf(stderr, "Invalid color on line %d of %s: %s", lineno, path, line);
      fclose(f);
      return NULL;
    }
    if (n >= 256) {
      fprintf(stderr, "Palette file exceeds 256 colors\n");
      fclose(f);
      return NULL;
    }
    buf[n++] = (Color){(uint8_t)r, (uint8_t)g, (uint8_t)b};
  }

  fclose(f);

  if (n == 0) {
    fprintf(stderr, "No valid colors found in palette file: %s\n", path);
    return NULL;
  }

  Color *colors = malloc(n * sizeof(Color));
  if (!colors) {
    fprintf(stderr, "Failed to allocate palette\n");
    return NULL;
  }
  memcpy(colors, buf, n * sizeof(Color));
  *out_n = n;
  return colors;
}

void paint_stroke(Color *canvas) {
  Color c = palette[rand() % n_colors];
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
  float diag = sqrtf((float)width * width + (float)height * height);
  int steps = (int)(diag / speed * frange(0.8f, 2.2f));

  for (int i = 0; i < steps; i++) {
    float rad = base_rad + sinf(i * 0.04f) * base_rad * 0.4f;
    rad = fmaxf(0.3f, rad);
    float a = frange(0.55f, 0.95f);

    if (rand() % 60 == 0) {
      stamp(canvas, x, y, rad * frange(2.0f, 4.5f), c, a * 0.5f);
    }

    stamp(canvas, x, y, rad, c, a);

    if (frand() < splatter_prob) {
      splatter(canvas, x, y, frange(8, 30), c, 2 + rand() % 6);
    }

    if (frand() < drip_prob) {
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
  int quiet = 0;
  Color *loaded_palette = NULL;
  Color extra_colors[256];
  int n_extra = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0) {
      printf(
          "Usage: %s [options]\n"
          "Generate a Pollock-style drip painting as a PNG.\n\n"
          "Options:\n"
          "  --seed <N>              Random seed (default: time-based)\n"
          "  --output <file>         Output PNG path (default: pollock.png)\n"
          "  --width <N>             Canvas width in pixels (default: 2400)\n"
          "  --height <N>            Canvas height in pixels (default: 1600)\n"
          "  --strokes <N>           Number of paint strokes (default: 420)\n"
          "  --background <R,G,B>    Background colour (default: 237,232,218)\n"
          "  --palette <file>        Load colours from file (one R,G,B per "
          "line)\n"
          "  --color <R,G,B>         Append a colour (repeatable)\n"
          "  --splatter-density <F>  Splatter probability per step 0-1 "
          "(default: 0.04)\n"
          "  --drip-density <F>      Drip probability per step 0-1 (default: "
          "0.022)\n"
          "  --quiet                 Suppress seed output\n"
          "  --help                  Show this help and exit\n",
          argv[0]);
      return EXIT_SUCCESS;
    } else if (strcmp(argv[i], "--seed") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing argument for --seed\n");
        return EXIT_FAILURE;
      }
      char *end;
      long v = strtol(argv[++i], &end, 10);
      if (end == argv[i] || *end != '\0') {
        fprintf(stderr, "Invalid integer for --seed: %s\n", argv[i]);
        return EXIT_FAILURE;
      }
      seed = (unsigned)v;
    } else if (strcmp(argv[i], "--output") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing argument for --output\n");
        return EXIT_FAILURE;
      }
      output = argv[++i];
    } else if (strcmp(argv[i], "--width") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing argument for --width\n");
        return EXIT_FAILURE;
      }
      char *end;
      long v = strtol(argv[++i], &end, 10);
      if (end == argv[i] || *end != '\0' || v <= 0) {
        fprintf(stderr, "Invalid positive integer for --width: %s\n", argv[i]);
        return EXIT_FAILURE;
      }
      width = (int)v;
    } else if (strcmp(argv[i], "--height") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing argument for --height\n");
        return EXIT_FAILURE;
      }
      char *end;
      long v = strtol(argv[++i], &end, 10);
      if (end == argv[i] || *end != '\0' || v <= 0) {
        fprintf(stderr, "Invalid positive integer for --height: %s\n", argv[i]);
        return EXIT_FAILURE;
      }
      height = (int)v;
    } else if (strcmp(argv[i], "--strokes") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing argument for --strokes\n");
        return EXIT_FAILURE;
      }
      char *end;
      long v = strtol(argv[++i], &end, 10);
      if (end == argv[i] || *end != '\0' || v <= 0) {
        fprintf(stderr, "Invalid positive integer for --strokes: %s\n",
                argv[i]);
        return EXIT_FAILURE;
      }
      n_strokes = (int)v;
    } else if (strcmp(argv[i], "--background") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing argument for --background\n");
        return EXIT_FAILURE;
      }
      unsigned int r, g, b;
      if (sscanf(argv[++i], "%u,%u,%u", &r, &g, &b) != 3) {
        fprintf(stderr, "Invalid color for --background: %s (expected R,G,B)\n",
                argv[i]);
        return EXIT_FAILURE;
      }
      if (r > 255 || g > 255 || b > 255) {
        fprintf(stderr, "Color components must be 0-255 for --background\n");
        return EXIT_FAILURE;
      }
      bg = (Color){(uint8_t)r, (uint8_t)g, (uint8_t)b};
    } else if (strcmp(argv[i], "--palette") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing argument for --palette\n");
        return EXIT_FAILURE;
      }
      loaded_palette = load_palette(argv[++i], &n_colors);
      if (!loaded_palette) {
        return EXIT_FAILURE;
      }
      palette = loaded_palette;
    } else if (strcmp(argv[i], "--splatter-density") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing argument for --splatter-density\n");
        return EXIT_FAILURE;
      }
      char *end;
      float v = strtof(argv[++i], &end);
      if (end == argv[i] || *end != '\0' || v < 0.0f || v > 1.0f) {
        fprintf(stderr,
                "Invalid value for --splatter-density: %s (expected 0.0-1.0)\n",
                argv[i]);
        return EXIT_FAILURE;
      }
      splatter_prob = v;
    } else if (strcmp(argv[i], "--drip-density") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing argument for --drip-density\n");
        return EXIT_FAILURE;
      }
      char *end;
      float v = strtof(argv[++i], &end);
      if (end == argv[i] || *end != '\0' || v < 0.0f || v > 1.0f) {
        fprintf(stderr,
                "Invalid value for --drip-density: %s (expected 0.0-1.0)\n",
                argv[i]);
        return EXIT_FAILURE;
      }
      drip_prob = v;
    } else if (strcmp(argv[i], "--color") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing argument for --color\n");
        return EXIT_FAILURE;
      }
      if (n_extra >= 256) {
        fprintf(stderr, "--color limit of 256 entries exceeded\n");
        return EXIT_FAILURE;
      }
      unsigned int r, g, b;
      if (sscanf(argv[++i], "%u,%u,%u", &r, &g, &b) != 3) {
        fprintf(stderr, "Invalid color for --color: %s (expected R,G,B)\n",
                argv[i]);
        return EXIT_FAILURE;
      }
      if (r > 255 || g > 255 || b > 255) {
        fprintf(stderr, "Color components must be 0-255 for --color\n");
        return EXIT_FAILURE;
      }
      extra_colors[n_extra++] = (Color){(uint8_t)r, (uint8_t)g, (uint8_t)b};
    } else if (strcmp(argv[i], "--quiet") == 0) {
      quiet = 1;
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      fprintf(stderr, "Run '%s --help' for usage.\n", argv[0]);
      return EXIT_FAILURE;
    }
  }

  if (n_extra > 0) {
    int total = n_colors + n_extra;
    Color *merged = malloc(total * sizeof(Color));
    if (!merged) {
      fprintf(stderr, "Failed to allocate palette\n");
      free(loaded_palette);
      return EXIT_FAILURE;
    }
    memcpy(merged, palette, n_colors * sizeof(Color));
    memcpy(merged + n_colors, extra_colors, n_extra * sizeof(Color));
    free(loaded_palette);
    loaded_palette = merged;
    palette = merged;
    n_colors = total;
  }

  srand(seed);
  if (!quiet) {
    printf("seed: %u\n", seed);
  }

  Color *canvas = malloc(width * height * sizeof(Color));
  if (!canvas) {
    fprintf(stderr, "Failed to allocate canvas\n");
    return EXIT_FAILURE;
  }

  for (int i = 0; i < height * width; i++) {
    canvas[i] = bg;
  }

  int tty = isatty(fileno(stderr));
  for (int s = 0; s < n_strokes; s++) {
    if (tty) {
      fprintf(stderr, "\rstroke %d/%d", s + 1, n_strokes);
      fflush(stderr);
    } else if ((s + 1) * 10 / n_strokes != s * 10 / n_strokes) {
      fprintf(stderr, "stroke %d/%d (%d%%)\n", s + 1, n_strokes,
              (s + 1) * 100 / n_strokes);
    }
    paint_stroke(canvas);
  }
  if (tty) {
    fprintf(stderr, "\n");
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
  free(loaded_palette);
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
