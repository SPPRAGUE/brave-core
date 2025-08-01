// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_CHROMIUM_SRC_CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UI_CONTROLLER_H_
#define BRAVE_CHROMIUM_SRC_CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UI_CONTROLLER_H_

// Declare a decorator for the ProcessDownloadButtonPress function to handle
// BraveDownloadCommands::kDeleteLocalFile command.
#define ProcessDownloadButtonPress(...)                 \
  ProcessDownloadButtonPress_ChromiumImpl(__VA_ARGS__); \
  void ProcessDownloadButtonPress(__VA_ARGS__)

#include <chrome/browser/download/bubble/download_bubble_ui_controller.h>  // IWYU pragma: export

#undef ProcessDownloadButtonPress

#endif  // BRAVE_CHROMIUM_SRC_CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UI_CONTROLLER_H_
