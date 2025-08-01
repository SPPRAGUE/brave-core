/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "base/feature_override.h"

#include <chrome/browser/browser_features.cc>

namespace features {

OVERRIDE_FEATURE_DEFAULT_STATES({{
    {kCertificateTransparencyAskBeforeEnabling,
     base::FEATURE_ENABLED_BY_DEFAULT},

    {kBookmarkTriggerForPrerender2, base::FEATURE_DISABLED_BY_DEFAULT},
    {kDestroyProfileOnBrowserClose, base::FEATURE_DISABLED_BY_DEFAULT},
    // Google has asked embedders not to enforce these pins:
    // https://groups.google.com/a/chromium.org/g/embedder-dev/c/XsNTwEiN1lI/m/TMXh-ZvOAAAJ
    {kNewTabPageTriggerForPrerender2, base::FEATURE_DISABLED_BY_DEFAULT},
#if !BUILDFLAG(IS_ANDROID)
    {kReportPakFileIntegrity, base::FEATURE_DISABLED_BY_DEFAULT},
#endif  // BUILDFLAG(IS_ANDROID)
}});

}  // namespace features
