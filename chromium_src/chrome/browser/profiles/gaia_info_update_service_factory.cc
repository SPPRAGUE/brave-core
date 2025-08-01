/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "chrome/browser/profiles/gaia_info_update_service_factory.h"

// Include to prevent redefining BuildServiceInstanceForBrowserContext
#include "chrome/browser/signin/identity_manager_factory.h"

#define BuildServiceInstanceForBrowserContext \
  BuildServiceInstanceForBrowserContext_ChromiumImpl
#include <chrome/browser/profiles/gaia_info_update_service_factory.cc>
#undef BuildServiceInstanceForBrowserContext

std::unique_ptr<KeyedService>
GAIAInfoUpdateServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return nullptr;
}
