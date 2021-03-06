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

#include "videoplayer.h"
#include <QQmlInfo>
#include <QTimer>
#include "renderer.h"
#include <QPainter>
#include <QMatrix4x4>
#include <cmath>
#include <gst/pbutils/encoding-profile.h>
#include <gst/pbutils/encoding-target.h>

static GstEncodingProfile *encodingProfile(const char *file, const char *name) {
  GstEncodingTarget *target = gst_encoding_target_load_from_file(file, NULL);
  if (!target) {
    return NULL;
  }

  GstEncodingProfile *profile = gst_encoding_target_get_profile(target, name);

  gst_encoding_target_unref (target);

  return profile;
}

VideoPlayer::VideoPlayer(QQuickItem *parent) :
  QQuickPaintedItem(parent),
  m_renderer(0),
  m_bin(0),
  m_src(0),
  m_sink(0) {

  setRenderTarget(QQuickPaintedItem::FramebufferObject);
  setSmooth(false);
  setAntialiasing(false);
}

VideoPlayer::~VideoPlayer() {
  stop();

  if (m_bin) {
    gst_object_unref(m_bin);
    m_bin = 0;
  }
}

void VideoPlayer::componentComplete() {
  QQuickPaintedItem::componentComplete();
}

void VideoPlayer::classBegin() {
  QQuickPaintedItem::classBegin();

  GstEncodingProfile *video = encodingProfile("video.gep", "video-profile");
  GstEncodingProfile *image = encodingProfile("image.gep", "image-profile");

  m_bin = gst_element_factory_make("camerabin", NULL);
  m_src = gst_element_factory_make("droidcamsrc", NULL);
  GstElement *src = gst_element_factory_make ("pulsesrc", NULL);
  g_object_set (m_bin, "audio-source", src, "camera-source", m_src, "flags",
		0x00000001 | 0x00000002 | 0x00000004 | 0x00000008, "image-profile", image,
		"video-profile", video, NULL);

  GstBus *bus = gst_element_get_bus(m_bin);
  gst_bus_add_watch(bus, bus_call, this);
  gst_object_unref(bus);
}

bool VideoPlayer::start() {
  if (!m_renderer) {
    m_renderer = QtCamViewfinderRenderer::create(this);
    if (!m_renderer) {
      qmlInfo(this) << "Failed to create viewfinder renderer";
      return false;
    }

    QObject::connect(m_renderer, SIGNAL(updateRequested()), this, SLOT(updateRequested()));
  }

  m_renderer->resize(QSizeF(width(), height()));

  if (!m_bin) {
    qmlInfo(this) << "no playbin";
    return false;
  }

  if (!m_sink) {
    m_sink = m_renderer->sinkElement();
    g_object_set (m_bin, "viewfinder-sink", m_sink, NULL);
  } else {
    // This will allow resetting EGLDisplay
    m_renderer->sinkElement();
  }

  if (gst_element_set_state(m_bin, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    qmlInfo(this) << "error setting pipeline to PLAYING";
    return false;
  }

  emit runningChanged();

  return true;
}

bool VideoPlayer::stop() {
  if (gst_element_set_state(m_bin, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE) {
    qmlInfo(this) << "error setting pipeline to NULL";
    return false;
  }

  if (m_renderer) {
    m_renderer->reset();
  }

  emit runningChanged();

  return true;
}

void VideoPlayer::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry) {
  QQuickPaintedItem::geometryChanged(newGeometry, oldGeometry);

  if (m_renderer) {
    m_renderer->resize(newGeometry.size());
  }
}


void VideoPlayer::paint(QPainter *painter) {
  painter->fillRect(contentsBoundingRect(), Qt::black);

  if (!m_renderer) {
    return;
  }

  bool needsNativePainting = m_renderer->needsNativePainting();

  if (needsNativePainting) {
    painter->beginNativePainting();
  }

  m_renderer->paint(QMatrix4x4(painter->combinedTransform()), painter->viewport());

  if (needsNativePainting) {
    painter->endNativePainting();
  }
}

gboolean VideoPlayer::bus_call(GstBus *bus, GstMessage *msg, gpointer data) {
  Q_UNUSED(bus);

  VideoPlayer *that = (VideoPlayer *) data;

  gchar *debug = NULL;
  GError *err = NULL;

  switch (GST_MESSAGE_TYPE(msg)) {
  case GST_MESSAGE_EOS:
    that->stop();
    break;

  case GST_MESSAGE_ERROR:
    gst_message_parse_error (msg, &err, &debug);
    qWarning() << "Error" << err->message;

    emit that->error(err->message, err->code, debug);
    that->stop();

    if (err) {
      g_error_free (err);
    }

    if (debug) {
      g_free (debug);
    }

    break;

  default:
    break;
  }

  return TRUE;
}

void VideoPlayer::updateRequested() {
  update();
}

bool VideoPlayer::running() {
  GstState st;

  return gst_element_get_state(m_bin, &st, NULL, GST_CLOCK_TIME_NONE) != GST_STATE_CHANGE_FAILURE && st == GST_STATE_PLAYING;
}

void VideoPlayer::capture() {
  g_signal_emit_by_name (m_bin, "start-capture", NULL);
}

void VideoPlayer::stopCapture() {
  g_signal_emit_by_name (m_bin, "stop-capture", NULL);
}

bool VideoPlayer::readyForCapture() {
  gboolean ready = FALSE;

  g_object_get (m_src, "ready-for-capture", &ready, NULL);

  return ready == TRUE;
}

int VideoPlayer::mode() {
  if (!m_bin) {
    return 1;
  }

  int m;
  g_object_get (m_bin, "mode", &m, NULL);

  return m;
}

void VideoPlayer::setMode(int mode) {
  if (VideoPlayer::mode() == mode) {
    return;
  }

  g_object_set (m_bin, "mode", mode, NULL);

  emit modeChanged();
}

int VideoPlayer::device() {
  if (!m_src) {
    return 0;
  }

  int d;
  g_object_get (m_src, "camera-device", &d, NULL);

  return d;
}

void VideoPlayer::setDevice(int device) {
  if (VideoPlayer::device() == device) {
    return;
  }

  bool r = running();

  if (r) {
    stop();
  }

  g_object_set (m_src, "camera-device", device, NULL);

  if (r) {
    start();
  }

  emit deviceChanged();
}
