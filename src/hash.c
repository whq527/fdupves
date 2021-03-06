/*
 * This file is part of the fdupves package
 * Copyright (C) <2008> Alf
 *
 * Contact: Alf <naihe2010@126.com>
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
 *
 */
/* @CFILE ihash.c
 *
 *  Author: Alf <naihe2010@126.com>
 */

#include "hash.h"
#include "video.h"
#include "image.h"
#include "ini.h"
#include "cache.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>

const char *hash_phrase[] =
  {
    "hash",
    "phash",
  };

static hash_t pixbuf_hash (GdkPixbuf *);

#define FDUPVES_HASH_LEN 8

hash_t
file_hash (const char *file)
{
  GdkPixbuf *buf;
  hash_t h;
  GError *err;

  if (g_cache)
    {
      if (cache_get (g_cache, file, 0, FDUPVES_HASH_HASH, &h))
	{
	  return h;
	}
    }

  buf = fdupves_gdkpixbuf_load_file_at_size (file,
					     FDUPVES_HASH_LEN,
					     FDUPVES_HASH_LEN,
					     &err);
  if (err)
    {
      g_warning ("Load file: %s to pixbuf failed: %s", file, err->message);
      g_error_free (err);
      return 0;
    }

  h = pixbuf_hash (buf);
  g_object_unref (buf);

  if (g_cache)
    {
      if (h)
	{
	  cache_set (g_cache, file, 0, FDUPVES_HASH_HASH, h);
	}
    }

  return h;
}

hash_t
buffer_hash (const char *buffer, int size)
{
  GdkPixbuf *buf;
  GError *err;
  hash_t h;

  err = NULL;
  buf = gdk_pixbuf_new_from_data ((const guchar *) buffer,
				  GDK_COLORSPACE_RGB,
				  FALSE,
				  8,
				  FDUPVES_HASH_LEN,
				  FDUPVES_HASH_LEN,
				  FDUPVES_HASH_LEN * 3,
				  NULL,
				  &err);
  if (err)
    {
      g_warning ("Load inline data to pixbuf failed: %s", err->message);
      g_error_free (err);
      return 0;
    }

  h = pixbuf_hash (buf);
  g_object_unref (buf);

  return h;
}

static hash_t
pixbuf_hash (GdkPixbuf *pixbuf)
{
  int width, height, rowstride, n_channels;
  guchar *pixels, *p;
  int *grays, sum, avg, x, y, off;
  hash_t hash;

  n_channels = gdk_pixbuf_get_n_channels (pixbuf);

  g_assert (gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB);
  g_assert (gdk_pixbuf_get_bits_per_sample (pixbuf) == 8);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  grays = g_new0 (int, width * height);
  off = 0;
  for (y = 0; y < height; ++ y)
    {
      for (x = 0; x < width; ++ x)
	{
	  p = pixels + y * rowstride + x * n_channels;
	  grays[off] = (p[0] * 30 + p[1] * 59 + p[2] * 11) / 100;
	  ++ off;
	}
    }

  sum = 0;
  for (x = 0; x < off; ++ x)
    {
      sum += grays[x];
    }
  avg = sum / off;

  hash = 0;
  for (x = 0; x < off; ++ x)
    {
      if (grays[x] >= avg)
	{
	  hash |= ((hash_t) 1 << x);
	}
    }

  g_free (grays);

  return hash;
}

int
hash_cmp (hash_t a, hash_t b)
{
  hash_t c;
  int cmp;

  if (!a || !b)
    {
      return FDUPVES_HASH_LEN * FDUPVES_HASH_LEN; /* max invalid distance */
    }

  c = a ^ b;
  switch (g_ini->compare_area)
    {
    case 1:
      c = c & 0xFFFFFF00ULL;
      break;

    case 2:
      c = c & 0x00FFFFFFULL;
      break;

    case 3:
      c = c & 0xFCFCFCFCULL;
      break;

    case 4:
      c = c & 0x3F3F3F3FULL;
      break;

    default:
      break;
    }
  for (cmp = 0; c; c = c >> 1)
    {
      if (c & 1)
	{
	  ++ cmp;
	}
    }

  return cmp;
}

hash_t
video_time_hash (const char *file, int time)
{
  hash_t h;
  gchar *buffer;
  gsize len;
#ifdef _DEBUG
  gchar *basename, outfile[4096];
#endif

  if (g_cache)
    {
      if (cache_get (g_cache, file, time, FDUPVES_HASH_HASH, &h))
	{
	  return h;
	}
    }

  len = FDUPVES_HASH_LEN * FDUPVES_HASH_LEN * 3;
  buffer = g_malloc (len);
  g_return_val_if_fail (buffer, 0);

  video_time_screenshot (file, time,
			 FDUPVES_HASH_LEN, FDUPVES_HASH_LEN,
			 buffer, len);
#ifdef _DEBUG
  basename = g_path_get_basename (file);
  g_snprintf (outfile, sizeof outfile, "%s/%s-%d.png",
	      g_get_tmp_dir (),
	      basename, time);
  g_free (basename);
  video_time_screenshot_file (file, time,
			      FDUPVES_HASH_LEN * 100,
			      FDUPVES_HASH_LEN * 100,
			      outfile);
#endif

  h = buffer_hash (buffer, len);
  g_free (buffer);

  if (g_cache)
    {
      if (h)
	{
	  cache_set (g_cache, file, time, FDUPVES_HASH_HASH, h);
	}
    }

  return h;
}
