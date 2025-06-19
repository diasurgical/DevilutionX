/*
 * png2icns_tiger - based on png2icns from the pngutils Debian package but
 * modified for Tiger support. See the section that says MODIFICATION.
 *
 * Copyright (C) 2008 Julien BLACHE <jb@jblache.org>
 * Copyright (C) 2012 Mathew Eis <mathew@eisbox.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>

#include <icns.h>
#include <png.h>
#include <setjmp.h>

#define FALSE 0
#define TRUE 1

#if PNG_LIBPNG_VER >= 10209
#define PNG2ICNS_EXPAND_GRAY 1
#endif

static int read_png(FILE *fp, png_bytepp buffer, int32_t *bpp, int32_t *width,
                    int32_t *height) {
  png_structp png_ptr;
  png_infop info;
  png_uint_32 w;
  png_uint_32 h;
  png_bytep *rows;

  int bit_depth;
  int32_t color_type;

  int row;
  int rowsize;

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png_ptr == NULL)
    return FALSE;

  info = png_create_info_struct(png_ptr);
  if (info == NULL) {
    png_destroy_read_struct(&png_ptr, NULL, NULL);
    return FALSE;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_read_struct(&png_ptr, &info, NULL);
    return FALSE;
  }

  png_init_io(png_ptr, fp);

  png_read_info(png_ptr, info);
  png_get_IHDR(png_ptr, info, &w, &h, &bit_depth, &color_type, NULL, NULL,
               NULL);

  switch (color_type) {
  case PNG_COLOR_TYPE_GRAY:
#ifdef PNG2ICNS_EXPAND_GRAY
    png_set_expand_gray_1_2_4_to_8(png_ptr);
#else
    png_set_gray_1_2_4_to_8(png_ptr);
#endif

    if (bit_depth == 16) {
      png_set_strip_16(png_ptr);
      bit_depth = 8;
    }

    png_set_gray_to_rgb(png_ptr);
    png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
    break;

  case PNG_COLOR_TYPE_GRAY_ALPHA:
#ifdef PNG2ICNS_EXPAND_GRAY
    png_set_expand_gray_1_2_4_to_8(png_ptr);
#else
    png_set_gray_1_2_4_to_8(png_ptr);
#endif

    if (bit_depth == 16) {
      png_set_strip_16(png_ptr);
      bit_depth = 8;
    }

    png_set_gray_to_rgb(png_ptr);
    break;

  case PNG_COLOR_TYPE_PALETTE:
    png_set_palette_to_rgb(png_ptr);

    if (png_get_valid(png_ptr, info, PNG_INFO_tRNS))
      png_set_tRNS_to_alpha(png_ptr);
    else
      png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
    break;

  case PNG_COLOR_TYPE_RGB:
    if (bit_depth == 16) {
      png_set_strip_16(png_ptr);
      bit_depth = 8;
    }

    png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
    break;

  case PNG_COLOR_TYPE_RGB_ALPHA:
    if (bit_depth == 16) {
      png_set_strip_16(png_ptr);
      bit_depth = 8;
    }

    break;
  }

  *width = w;
  *height = h;
  *bpp = bit_depth * 4;

  png_set_interlace_handling(png_ptr);

  png_read_update_info(png_ptr, info);

  rowsize = png_get_rowbytes(png_ptr, info);
  rows = malloc(sizeof(png_bytep) * h);
  *buffer = malloc(rowsize * h + 8);

  rows[0] = *buffer;
  for (row = 1; row < h; row++) {
    rows[row] = rows[row - 1] + rowsize;
  }

  png_read_image(png_ptr, rows);
  png_destroy_read_struct(&png_ptr, &info, NULL);

  free(rows);

  return TRUE;
}

static int add_png_to_family(icns_family_t **iconFamily, char *pngname) {
  FILE *pngfile;

  int icnsErr = ICNS_STATUS_OK;
  icns_image_t icnsImage;
  icns_image_t icnsMask;
  icns_type_t iconType;
  icns_type_t maskType;
  icns_icon_info_t iconInfo;

  icns_element_t *iconElement = NULL;
  icns_element_t *maskElement = NULL;
  char iconStr[5] = {0, 0, 0, 0, 0};
  char maskStr[5] = {0, 0, 0, 0, 0};
  int iconDataOffset = 0;
  int maskDataOffset = 0;

  png_bytep buffer;
  int width, height, bpp;

  pngfile = fopen(pngname, "rb");
  if (pngfile == NULL) {
    fprintf(stderr, "Could not open '%s' for reading: %s\n", pngname,
            strerror(errno));
    return FALSE;
  }

  if (!read_png(pngfile, &buffer, &bpp, &width, &height)) {
    fprintf(stderr, "Failed to read PNG file\n");
    fclose(pngfile);

    return FALSE;
  }

  fclose(pngfile);

  icnsImage.imageWidth = width;
  icnsImage.imageHeight = height;
  icnsImage.imageChannels = 4;
  icnsImage.imagePixelDepth = 8;
  icnsImage.imageDataSize = width * height * 4;
  icnsImage.imageData = buffer;

  iconInfo.isImage = 1;
  iconInfo.iconWidth = icnsImage.imageWidth;
  iconInfo.iconHeight = icnsImage.imageHeight;
  iconInfo.iconBitDepth = bpp;
  iconInfo.iconChannels = (bpp == 32 ? 4 : 1);
  iconInfo.iconPixelDepth = bpp / iconInfo.iconChannels;

  iconType = icns_get_type_from_image_info(iconInfo);
  maskType = icns_get_mask_type_for_icon_type(iconType);

  /* MODIFICATION */
  if (iconType == ICNS_128x128_32BIT_ARGB_DATA) {
    /* libicns returns "ic07" for 128x128 icons but that doesn't work on Tiger
     */
    iconType = ICNS_128X128_32BIT_DATA;
    maskType = ICNS_128X128_8BIT_MASK;
  }
  /* END OF MODIFICATION */

  icns_type_str(iconType, iconStr);
  icns_type_str(maskType, maskStr);

  /* Only convert the icons that match sizes icns supports */
  if (iconType == ICNS_NULL_TYPE) {
    fprintf(stderr, "Bad dimensions: PNG file '%s' is %dx%d\n", pngname, width,
            height);
    free(buffer);

    return FALSE;
  }

  if (bpp != 32) {
    fprintf(stderr, "Bit depth %d unsupported in '%s'\n", bpp, pngname);
    free(buffer);

    return FALSE;
  }

  icns_set_print_errors(0);
  if (icns_get_element_from_family(*iconFamily, iconType, &iconElement) ==
      ICNS_STATUS_OK) {
    icns_set_print_errors(1);

    fprintf(stderr, "Duplicate icon element of type '%s' detected (%s)\n",
            iconStr, pngname);
    free(buffer);

    return FALSE;
  }

  icns_set_print_errors(1);

  if ((iconType != ICNS_1024x1024_32BIT_ARGB_DATA) &&
      (iconType != ICNS_512x512_32BIT_ARGB_DATA) &&
      (iconType != ICNS_256x256_32BIT_ARGB_DATA) &&
      (iconType != ICNS_128x128_32BIT_ARGB_DATA)) {
    printf("Using icns type '%s', mask '%s' for '%s'\n", iconStr, maskStr,
           pngname);
  } else {
    printf("Using icns type '%s' (ARGB) for '%s'\n", iconStr, pngname);
  }

  icnsErr = icns_new_element_from_image(&icnsImage, iconType, &iconElement);

  if (iconElement != NULL) {
    if (icnsErr == ICNS_STATUS_OK) {
      icns_set_element_in_family(iconFamily, iconElement);
    }
    free(iconElement);
  }

  if ((iconType != ICNS_1024x1024_32BIT_ARGB_DATA) &&
      (iconType != ICNS_512x512_32BIT_ARGB_DATA) &&
      (iconType != ICNS_256x256_32BIT_ARGB_DATA) &&
      (iconType != ICNS_128x128_32BIT_ARGB_DATA)) {
    icns_init_image_for_type(maskType, &icnsMask);

    iconDataOffset = 0;
    maskDataOffset = 0;

    while ((iconDataOffset < icnsImage.imageDataSize) &&
           (maskDataOffset < icnsMask.imageDataSize)) {
      icnsMask.imageData[maskDataOffset] =
          icnsImage.imageData[iconDataOffset + 3];
      iconDataOffset += 4; /* move to the next alpha byte */
      maskDataOffset += 1; /* move to the next byte */
    }

    icnsErr = icns_new_element_from_mask(&icnsMask, maskType, &maskElement);

    if (maskElement != NULL) {
      if (icnsErr == ICNS_STATUS_OK) {
        icns_set_element_in_family(iconFamily, maskElement);
      }
      free(maskElement);
    }

    icns_free_image(&icnsMask);
  }

  free(buffer);

  return TRUE;
}

int main(int argc, char **argv) {
  FILE *icnsfile;

  icns_family_t *iconFamily;

  int i;

  if (argc < 3) {
    printf("Usage: png2icns file.icns file1.png file2.png ... filen.png\n");
    exit(1);
  }

  icnsfile = fopen(argv[1], "wb+");
  if (icnsfile == NULL) {
    fprintf(stderr, "Could not open '%s' for writing: %s\n", argv[1],
            strerror(errno));
    exit(1);
  }

  icns_set_print_errors(1);
  icns_create_family(&iconFamily);

  for (i = 2; i < argc; i++) {
    if (!add_png_to_family(&iconFamily, argv[i])) {
      fclose(icnsfile);
      unlink(argv[1]);

      exit(1);
    }
  }

  if (icns_write_family_to_file(icnsfile, iconFamily) != ICNS_STATUS_OK) {
    fprintf(stderr, "Failed to write icns file\n");
    fclose(icnsfile);

    exit(1);
  }

  fclose(icnsfile);

  printf("Saved icns file to %s\n", argv[1]);

  if (iconFamily != NULL)
    free(iconFamily);

  return 0;
}
