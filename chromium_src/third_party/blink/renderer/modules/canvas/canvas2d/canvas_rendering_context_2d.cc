/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"

#include "brave/third_party/blink/renderer/core/farbling/brave_session_cache.h"

#define getImageDataInternal getImageDataInternal_Unused
#include <third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.cc>
#undef getImageDataInternal

namespace blink {

ImageData* CanvasRenderingContext2D::getImageDataInternal(
    ScriptState* script_state,
    int sx,
    int sy,
    int sw,
    int sh,
    ImageDataSettings* image_data_settings,
    ExceptionState& exception_state) {
  return BaseRenderingContext2D::getImageDataInternal(
      script_state, sx, sy, sw, sh, image_data_settings, exception_state);
}

}  // namespace blink
