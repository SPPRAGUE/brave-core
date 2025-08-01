/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_CHROMIUM_SRC_UI_NATIVE_THEME_NATIVE_THEME_MAC_H_
#define BRAVE_CHROMIUM_SRC_UI_NATIVE_THEME_NATIVE_THEME_MAC_H_

#include "ui/native_theme/native_theme_base.h"

#define GetSystemButtonPressedColor                                            \
  GetSystemButtonPressedColor_ChromiumImpl(SkColor base_color) const override; \
  virtual SkColor GetSystemButtonPressedColor

#include <ui/native_theme/native_theme_mac.h>  // IWYU pragma: export
#undef GetSystemButtonPressedColor

#endif  // BRAVE_CHROMIUM_SRC_UI_NATIVE_THEME_NATIVE_THEME_MAC_H_
