/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "chrome/browser/ui/tabs/public/tab_features.h"

#define CreateTabFeatures CreateTabFeatures_Unused
#define ReplaceTabFeaturesForTesting ReplaceTabFeaturesForTesting_Unused

#include <chrome/browser/ui/tabs/tab_features.cc>

#undef CreateTabFeatures
#undef ReplaceTabFeaturesForTesting
