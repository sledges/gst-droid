/*
 * gst-droid
 *
 * Copyright (C) 2014 Mohammed Sameer <msameer@foolab.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstdroidcamsrcphotography.h"
#include "gstdroidcamsrc.h"
#include <stdlib.h>
#ifndef GST_USE_UNSTABLE_API
#define GST_USE_UNSTABLE_API
#endif /* GST_USE_UNSTABLE_API */
#include <gst/interfaces/photography.h>

GST_DEBUG_CATEGORY_EXTERN (gst_droidcamsrc_debug);
#define GST_CAT_DEFAULT gst_droidcamsrc_debug

struct Entry
{
  int prop;
  const gchar *photo_prop;
};

struct Entry Entries[] = {
  {PROP_WB_MODE, GST_PHOTOGRAPHY_PROP_WB_MODE},
  {PROP_COLOR_TONE, GST_PHOTOGRAPHY_PROP_COLOR_TONE},
  {PROP_SCENE_MODE, GST_PHOTOGRAPHY_PROP_SCENE_MODE},
  {PROP_FLASH_MODE, GST_PHOTOGRAPHY_PROP_FLASH_MODE},
  {PROP_FLICKER_MODE, GST_PHOTOGRAPHY_PROP_FLICKER_MODE},
  {PROP_FOCUS_MODE, GST_PHOTOGRAPHY_PROP_FOCUS_MODE},
  {PROP_EXPOSURE_MODE, GST_PHOTOGRAPHY_PROP_EXPOSURE_MODE},
  {PROP_NOISE_REDUCTION, GST_PHOTOGRAPHY_PROP_NOISE_REDUCTION},
  {PROP_ZOOM, GST_PHOTOGRAPHY_PROP_ZOOM},
  {PROP_EV_COMP, GST_PHOTOGRAPHY_PROP_EV_COMP},
  {PROP_ANALOG_GAIN, GST_PHOTOGRAPHY_PROP_ANALOG_GAIN},
  {PROP_LENS_FOCUS, GST_PHOTOGRAPHY_PROP_LENS_FOCUS},
  {PROP_APERTURE, GST_PHOTOGRAPHY_PROP_APERTURE},
  {PROP_ISO_SPEED, GST_PHOTOGRAPHY_PROP_ISO_SPEED},
  {PROP_COLOR_TEMPERATURE, GST_PHOTOGRAPHY_PROP_COLOR_TEMPERATURE},
  {PROP_MIN_EXPOSURE_TIME, GST_PHOTOGRAPHY_PROP_MIN_EXPOSURE_TIME},
  {PROP_MAX_EXPOSURE_TIME, GST_PHOTOGRAPHY_PROP_MAX_EXPOSURE_TIME},
  {PROP_EXPOSURE_TIME, GST_PHOTOGRAPHY_PROP_EXPOSURE_TIME},
  {PROP_CAPABILITIES, GST_PHOTOGRAPHY_PROP_CAPABILITIES},
  {PROP_IMAGE_CAPTURE_SUPPORTED_CAPS,
      GST_PHOTOGRAPHY_PROP_IMAGE_CAPTURE_SUPPORTED_CAPS},
  {PROP_IMAGE_PREVIEW_SUPPORTED_CAPS,
      GST_PHOTOGRAPHY_PROP_IMAGE_PREVIEW_SUPPORTED_CAPS},
  {PROP_WHITE_POINT, GST_PHOTOGRAPHY_PROP_WHITE_POINT},
};

struct _GstDroidCamSrcPhotography
{
  GstPhotographySettings settings;
};

GHashTable *gst_droidcamsrc_photography_load (GKeyFile * file,
    const gchar * property);

#define PHOTO_IFACE_FUNC(name, tset, tget)					\
  static gboolean gst_droidcamsrc_get_##name (GstDroidCamSrc * src, tget val); \
  static gboolean gst_droidcamsrc_set_##name (GstDroidCamSrc * src, tset val); \
  static gboolean gst_droidcamsrc_photography_get_##name (GstPhotography *photo, tget val) \
  { \
    return gst_droidcamsrc_get_##name (GST_DROIDCAMSRC (photo), val);	\
  }                                                                                             \
  static gboolean gst_droidcamsrc_photography_set_##name (GstPhotography *photo, tset val) \
  { \
    return gst_droidcamsrc_set_##name (GST_DROIDCAMSRC (photo), val);	\
  }

PHOTO_IFACE_FUNC (ev_compensation, float, float *);
PHOTO_IFACE_FUNC (iso_speed, guint, guint *);
PHOTO_IFACE_FUNC (aperture, guint, guint *);
PHOTO_IFACE_FUNC (exposure, guint32, guint32 *);
PHOTO_IFACE_FUNC (white_balance_mode, GstPhotographyWhiteBalanceMode,
    GstPhotographyWhiteBalanceMode *);
PHOTO_IFACE_FUNC (color_tone_mode, GstPhotographyColorToneMode,
    GstPhotographyColorToneMode *);
PHOTO_IFACE_FUNC (scene_mode, GstPhotographySceneMode,
    GstPhotographySceneMode *);
PHOTO_IFACE_FUNC (flash_mode, GstPhotographyFlashMode,
    GstPhotographyFlashMode *);
PHOTO_IFACE_FUNC (zoom, gfloat, gfloat *);
PHOTO_IFACE_FUNC (flicker_mode, GstPhotographyFlickerReductionMode,
    GstPhotographyFlickerReductionMode *);
PHOTO_IFACE_FUNC (focus_mode, GstPhotographyFocusMode,
    GstPhotographyFocusMode *);
PHOTO_IFACE_FUNC (noise_reduction, GstPhotographyNoiseReduction,
    GstPhotographyNoiseReduction *);
PHOTO_IFACE_FUNC (config, GstPhotographySettings *, GstPhotographySettings *);
static GstPhotographyCaps gst_droidcamsrc_get_capabilities (GstDroidCamSrc *
    src);
static GstPhotographyCaps
gst_droidcamsrc_photography_get_capabilities (GstPhotography * photo)
{
  return gst_droidcamsrc_get_capabilities (GST_DROIDCAMSRC (photo));
}

static gboolean gst_droidcamsrc_prepare_for_capture (GstDroidCamSrc * src,
    GstPhotographyCapturePrepared func,
    GstCaps * capture_caps, gpointer user_data);

static gboolean
gst_droidcamsrc_photography_prepare_for_capture (GstPhotography * photo,
    GstPhotographyCapturePrepared func,
    GstCaps * capture_caps, gpointer user_data)
{
  return gst_droidcamsrc_prepare_for_capture (GST_DROIDCAMSRC (photo), func,
      capture_caps, user_data);
}

static void gst_droidcamsrc_set_autofocus (GstDroidCamSrc * src, gboolean on);
static void
gst_droidcamsrc_photography_set_autofocus (GstPhotography * photo, gboolean on)
{
  gst_droidcamsrc_set_autofocus (GST_DROIDCAMSRC (photo), on);
}

void
gst_droidcamsrc_photography_register (gpointer g_iface, gpointer iface_data)
{
  GstPhotographyInterface *iface = (GstPhotographyInterface *) g_iface;
  iface->get_ev_compensation = gst_droidcamsrc_photography_get_ev_compensation;
  iface->get_iso_speed = gst_droidcamsrc_photography_get_iso_speed;
  iface->get_aperture = gst_droidcamsrc_photography_get_aperture;
  iface->get_exposure = gst_droidcamsrc_photography_get_exposure;
  iface->get_white_balance_mode =
      gst_droidcamsrc_photography_get_white_balance_mode;
  iface->get_color_tone_mode = gst_droidcamsrc_photography_get_color_tone_mode;
  iface->get_scene_mode = gst_droidcamsrc_photography_get_scene_mode;
  iface->get_flash_mode = gst_droidcamsrc_photography_get_flash_mode;
  iface->get_zoom = gst_droidcamsrc_photography_get_zoom;
  iface->get_flicker_mode = gst_droidcamsrc_photography_get_flicker_mode;
  iface->get_focus_mode = gst_droidcamsrc_photography_get_focus_mode;
  iface->set_ev_compensation = gst_droidcamsrc_photography_set_ev_compensation;
  iface->set_iso_speed = gst_droidcamsrc_photography_set_iso_speed;
  iface->set_aperture = gst_droidcamsrc_photography_set_aperture;
  iface->set_exposure = gst_droidcamsrc_photography_set_exposure;
  iface->set_white_balance_mode =
      gst_droidcamsrc_photography_set_white_balance_mode;
  iface->set_color_tone_mode = gst_droidcamsrc_photography_set_color_tone_mode;
  iface->set_scene_mode = gst_droidcamsrc_photography_set_scene_mode;
  iface->set_flash_mode = gst_droidcamsrc_photography_set_flash_mode;
  iface->set_zoom = gst_droidcamsrc_photography_set_zoom;
  iface->set_flicker_mode = gst_droidcamsrc_photography_set_flicker_mode;
  iface->set_focus_mode = gst_droidcamsrc_photography_set_focus_mode;
  iface->get_capabilities = gst_droidcamsrc_photography_get_capabilities;
  iface->prepare_for_capture = gst_droidcamsrc_photography_prepare_for_capture;
  iface->set_autofocus = gst_droidcamsrc_photography_set_autofocus;
  iface->set_config = gst_droidcamsrc_photography_set_config;
  iface->get_config = gst_droidcamsrc_photography_get_config;
  iface->get_noise_reduction = gst_droidcamsrc_photography_get_noise_reduction;
  iface->set_noise_reduction = gst_droidcamsrc_photography_set_noise_reduction;
}

void
gst_droidcamsrc_photography_add_overrides (GObjectClass * klass)
{
  int x;
  int len = G_N_ELEMENTS (Entries);

  for (x = 0; x < len; x++) {
    g_object_class_override_property (klass, Entries[x].prop,
        Entries[x].photo_prop);
  }
}

gboolean
gst_droidcamsrc_photography_get_property (GstDroidCamSrc * src, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    case PROP_WB_MODE:
    {
      GstPhotographyWhiteBalanceMode mode;
      if (gst_droidcamsrc_get_white_balance_mode (src, &mode)) {
        g_value_set_enum (value, mode);
      }
    }
      return TRUE;

    case PROP_COLOR_TONE:
    {
      GstPhotographyColorToneMode tone;
      if (gst_droidcamsrc_get_color_tone_mode (src, &tone)) {
        g_value_set_enum (value, tone);
      }
    }
      return TRUE;

    case PROP_SCENE_MODE:
    {
      GstPhotographySceneMode mode;
      if (gst_droidcamsrc_get_scene_mode (src, &mode)) {
        g_value_set_enum (value, mode);
      }
    }
      return TRUE;

    case PROP_FLASH_MODE:
    {
      GstPhotographyFlashMode mode;
      if (gst_droidcamsrc_get_flash_mode (src, &mode)) {
        g_value_set_enum (value, mode);
      }
    }
      return TRUE;

    case PROP_FLICKER_MODE:
    {
      GstPhotographyFlickerReductionMode mode;
      if (gst_droidcamsrc_get_flicker_mode (src, &mode)) {
        g_value_set_enum (value, mode);
      }
    }
      return TRUE;

    case PROP_FOCUS_MODE:
    {
      GstPhotographyFocusMode mode;
      if (gst_droidcamsrc_get_focus_mode (src, &mode)) {
        g_value_set_enum (value, mode);
      }
    }
      return TRUE;

    case PROP_NOISE_REDUCTION:
    {
      GstPhotographyNoiseReduction mode;
      if (gst_droidcamsrc_get_noise_reduction (src, &mode)) {
        g_value_set_enum (value, mode);
      }
    }
      return TRUE;

    case PROP_EXPOSURE_MODE:
    {
      // TODO:
      g_value_set_enum (value, src->photo->settings.exposure_mode);
    }
      return TRUE;

    case PROP_ZOOM:
    {
      gfloat zoom;
      if (gst_droidcamsrc_get_zoom (src, &zoom)) {
        g_value_set_float (value, zoom);
      }
    }
      return TRUE;

    case PROP_EV_COMP:
    {
      gfloat ev;
      if (gst_droidcamsrc_get_ev_compensation (src, &ev)) {
        g_value_set_float (value, ev);
      }
    }
      return TRUE;

    case PROP_ANALOG_GAIN:
    {
      // TODO:
      g_value_set_float (value, src->photo->settings.analog_gain);
    }
      return TRUE;

    case PROP_LENS_FOCUS:
    {
      // TODO:
      g_value_set_float (value, src->photo->settings.lens_focus);
    }
      return TRUE;

    case PROP_APERTURE:
    {
      guint aperture;
      if (gst_droidcamsrc_get_aperture (src, &aperture)) {
        g_value_set_uint (value, aperture);
      }
    }
      return TRUE;

    case PROP_ISO_SPEED:
    {
      guint iso;
      if (gst_droidcamsrc_get_iso_speed (src, &iso)) {
        g_value_set_uint (value, iso);
      }
    }
      return TRUE;

    case PROP_COLOR_TEMPERATURE:
    {
      // TODO:
      g_value_set_uint (value, src->photo->settings.color_temperature);
    }
      return TRUE;

    case PROP_MIN_EXPOSURE_TIME:
    {
      // TODO:
      g_value_set_uint (value, src->photo->settings.min_exposure_time);
    }
      return TRUE;

    case PROP_MAX_EXPOSURE_TIME:
    {
      // TODO:
      g_value_set_uint (value, src->photo->settings.max_exposure_time);
    }
      return TRUE;

    case PROP_EXPOSURE_TIME:
    {
      guint32 exposure;
      if (gst_droidcamsrc_get_exposure (src, &exposure)) {
        g_value_set_uint (value, exposure);
      }
    }
      return TRUE;

    case PROP_CAPABILITIES:
    {
      gulong capabilities = gst_droidcamsrc_get_capabilities (src);
      g_value_set_ulong (value, capabilities);
    }
      return TRUE;

    case PROP_IMAGE_CAPTURE_SUPPORTED_CAPS:
    {
      // TODO:
    }
      return TRUE;

    case PROP_IMAGE_PREVIEW_SUPPORTED_CAPS:
    {
      // TODO:
    }
      return TRUE;

    case PROP_WHITE_POINT:
    {
      // TODO:
    }
      return TRUE;
  }

  return FALSE;
}

gboolean
gst_droidcamsrc_photography_set_property (GstDroidCamSrc * src, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  // TODO:

  switch (prop_id) {
    case PROP_WB_MODE:
    case PROP_COLOR_TONE:
    case PROP_SCENE_MODE:
    case PROP_FLASH_MODE:
    case PROP_FLICKER_MODE:
    case PROP_FOCUS_MODE:
    case PROP_EV_COMP:
    case PROP_ISO_SPEED:
    case PROP_APERTURE:
    case PROP_EXPOSURE_TIME:
    case PROP_ZOOM:
    case PROP_COLOR_TEMPERATURE:
    case PROP_WHITE_POINT:
    case PROP_ANALOG_GAIN:
    case PROP_LENS_FOCUS:
    case PROP_MIN_EXPOSURE_TIME:
    case PROP_MAX_EXPOSURE_TIME:
    case PROP_NOISE_REDUCTION:
    case PROP_EXPOSURE_MODE:
      return TRUE;
  }

  return FALSE;
}

void
gst_droidcamsrc_photography_init (GstDroidCamSrc * src)
{
  GKeyFile *file = g_key_file_new ();
  gchar *file_path =
      g_build_path ("/", SYSCONFDIR, "gst-droid/gstdroidcamsrc.conf", NULL);
  GError *err = NULL;

  src->photo = g_slice_new0 (GstDroidCamSrcPhotography);

  if (!g_key_file_load_from_file (file, file_path, G_KEY_FILE_NONE, &err)) {
    GST_WARNING ("failed to load configuration file %s: %s", file_path,
        err->message);
  }

  if (err) {
    g_error_free (err);
    err = NULL;
  }

  src->photo->settings.wb_mode = GST_PHOTOGRAPHY_WB_MODE_AUTO;
  src->photo->settings.tone_mode = GST_PHOTOGRAPHY_COLOR_TONE_MODE_NORMAL;
  src->photo->settings.scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_AUTO;
  src->photo->settings.flash_mode = GST_PHOTOGRAPHY_FLASH_MODE_AUTO;
  src->photo->settings.exposure_time = 0;
  src->photo->settings.aperture = 0;
  src->photo->settings.ev_compensation = 0.0;
  src->photo->settings.iso_speed = 0;
  src->photo->settings.zoom = 1.0;
  src->photo->settings.flicker_mode = GST_PHOTOGRAPHY_FLICKER_REDUCTION_OFF;
  src->photo->settings.focus_mode =
      GST_PHOTOGRAPHY_FOCUS_MODE_CONTINUOUS_NORMAL;

  /* TODO: the ones below are the ones I am not sure how to set a default value for */
  src->photo->settings.noise_reduction = 0;
  src->photo->settings.exposure_mode = GST_PHOTOGRAPHY_EXPOSURE_MODE_AUTO;      /* TODO: seems not to be a property */
  src->photo->settings.color_temperature = 0;
  memset (&src->photo->settings.white_point, 0x0,
      sizeof (src->photo->settings.white_point));
  src->photo->settings.analog_gain = 0.0;
  src->photo->settings.lens_focus = 0.0;
  src->photo->settings.min_exposure_time = 0;
  src->photo->settings.max_exposure_time = 0;

  /* load settings */
  //  src->photo->flash = gst_droidcamsrc_photography_load (file, "flash-mode");

  /* free our stuff */
  g_free (file_path);
  g_key_file_unref (file);
}

void
gst_droidcamsrc_photography_destroy (GstDroidCamSrc * src)
{
  //  g_hash_table_unref (src->photo->flash);
  //  src->photo->flash = NULL;

  g_slice_free (GstDroidCamSrcPhotography, src->photo);
  src->photo = NULL;
}

GHashTable *
gst_droidcamsrc_photography_load (GKeyFile * file, const gchar * property)
{
  gchar **keys;
  GError *err;
  int x;
  gsize len = 0;
  GHashTable *table =
      g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

  keys = g_key_file_get_keys (file, property, &len, &err);

  if (err) {
    GST_WARNING ("failed to load %s: %s", property, err->message);
    g_error_free (err);
    err = NULL;
  }

  for (x = 0; x < len; x++) {
    gchar *value = g_key_file_get_value (file, property, keys[x], &err);

    if (err) {
      GST_WARNING ("failed to load %s (%s): %s", property, keys[x],
          err->message);
      g_error_free (err);
      err = NULL;
    }

    if (value) {
      int key = atoi (keys[x]);
      g_hash_table_insert (table, GINT_TO_POINTER (key), value);
    }
  }

  return table;
}

static gboolean
gst_droidcamsrc_get_ev_compensation (GstDroidCamSrc * src, gfloat * ev_comp)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_get_iso_speed (GstDroidCamSrc * photo, guint * iso_speed)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_get_aperture (GstDroidCamSrc * src, guint * aperture)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_get_exposure (GstDroidCamSrc * src, guint32 * exposure)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_get_white_balance_mode (GstDroidCamSrc *
    src, GstPhotographyWhiteBalanceMode * wb_mode)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_get_color_tone_mode (GstDroidCamSrc *
    src, GstPhotographyColorToneMode * tone_mode)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_get_scene_mode (GstDroidCamSrc
    * src, GstPhotographySceneMode * scene_mode)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_get_flash_mode (GstDroidCamSrc
    * src, GstPhotographyFlashMode * flash_mode)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_get_zoom (GstDroidCamSrc * src, gfloat * zoom)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_get_flicker_mode (GstDroidCamSrc * photo,
    GstPhotographyFlickerReductionMode * flicker_mode)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_get_focus_mode (GstDroidCamSrc
    * src, GstPhotographyFocusMode * focus_mode)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_set_ev_compensation (GstDroidCamSrc * src, gfloat ev_comp)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_set_iso_speed (GstDroidCamSrc * src, guint iso_speed)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_set_aperture (GstDroidCamSrc * src, guint aperture)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_set_exposure (GstDroidCamSrc * src, guint32 exposure)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_set_white_balance_mode (GstDroidCamSrc *
    src, GstPhotographyWhiteBalanceMode wb_mode)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_set_color_tone_mode (GstDroidCamSrc *
    src, GstPhotographyColorToneMode tone_mode)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_set_scene_mode (GstDroidCamSrc
    * src, GstPhotographySceneMode scene_mode)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_set_flash_mode (GstDroidCamSrc
    * src, GstPhotographyFlashMode flash_mode)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_set_zoom (GstDroidCamSrc * src, gfloat zoom)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_set_flicker_mode (GstDroidCamSrc * photo,
    GstPhotographyFlickerReductionMode flicker_mode)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_set_focus_mode (GstDroidCamSrc
    * src, GstPhotographyFocusMode focus_mode)
{
  // TODO:
  return FALSE;
}

static GstPhotographyCaps
gst_droidcamsrc_get_capabilities (GstDroidCamSrc * src)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_prepare_for_capture (GstDroidCamSrc *
    src, GstPhotographyCapturePrepared func, GstCaps * capture_caps,
    gpointer user_data)
{
  // TODO:
  return FALSE;
}

static void
gst_droidcamsrc_set_autofocus (GstDroidCamSrc * src, gboolean on)
{
  // TODO:

}

static gboolean
gst_droidcamsrc_set_config (GstDroidCamSrc *
    src, GstPhotographySettings * config)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_get_config (GstDroidCamSrc *
    src, GstPhotographySettings * config)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_get_noise_reduction (GstDroidCamSrc *
    src, GstPhotographyNoiseReduction * noise_reduction)
{
  // TODO:
  return FALSE;
}

static gboolean
gst_droidcamsrc_set_noise_reduction (GstDroidCamSrc *
    src, GstPhotographyNoiseReduction noise_reduction)
{
  // TODO:
  return FALSE;
}