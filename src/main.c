#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 200
#define HEIGHT 200
#define SQUARE_SIZE 50

int main() {
  int x, y;

  png_image image;
  memset(&image, 0, (sizeof image));
  image.version = PNG_IMAGE_VERSION;
  image.format = PNG_FORMAT_RGB;
  image.width = WIDTH;
  image.height = HEIGHT;

  png_bytep buffer = malloc(PNG_IMAGE_SIZE(image));

  for (y = 0; y < HEIGHT; y++) {
    for (x = 0; x < WIDTH; x++) {
      png_bytep ptr = &buffer[y * WIDTH * 3 + x * 3];
      ptr[0] = 255;
      ptr[1] = 255;
      ptr[2] = 255;
    }
  }

  for (y = HEIGHT / 2 - SQUARE_SIZE / 2; y < HEIGHT / 2 + SQUARE_SIZE / 2;
       y++) {
    for (x = WIDTH / 2 - SQUARE_SIZE / 2; x < WIDTH / 2 + SQUARE_SIZE / 2;
         x++) {
      png_bytep ptr = &buffer[y * WIDTH * 3 + x * 3];
      ptr[0] = 255;
      ptr[1] = 0;
      ptr[2] = 0;
    }
  }

  if (!png_image_write_to_file(&image, "red_square.png", 0, buffer, 0, NULL)) {
    printf("png_image_write_to_file failed: %s\n", image.message);
    return EXIT_FAILURE;
  }

  free(buffer);
  png_image_free(&image);

  return EXIT_SUCCESS;
}
