// Copyright (c) 2024 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/constants/webui_url_constants.h"

// Include password_manager constants before override so we don't override the
// definition.
#include "components/password_manager/content/common/web_ui_constants.h"

// Add some extra items to WebUI hosts considered valid for PWAs
#define kChromeUIPasswordManagerHost \
  kChromeUIPasswordManagerHost &&    \
      !kInstallablePWAWebUIHosts.contains(url.host_piece())

#include <components/webapps/browser/banners/app_banner_manager.cc>
#undef kChromeUIPasswordManagerHost
