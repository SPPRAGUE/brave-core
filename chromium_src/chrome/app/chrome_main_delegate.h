/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_CHROMIUM_SRC_CHROME_APP_CHROME_MAIN_DELEGATE_H_
#define BRAVE_CHROMIUM_SRC_CHROME_APP_CHROME_MAIN_DELEGATE_H_

#include "brave/common/brave_content_client.h"
#include "chrome/common/chrome_content_client.h"

#define ChromeContentClient BraveContentClient
#include <chrome/app/chrome_main_delegate.h>  // IWYU pragma: export
#undef ChromeContentClient

#endif  // BRAVE_CHROMIUM_SRC_CHROME_APP_CHROME_MAIN_DELEGATE_H_
