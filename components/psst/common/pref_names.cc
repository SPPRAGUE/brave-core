// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/psst/common/pref_names.h"

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "brave/components/psst/common/features.h"
#include "components/prefs/pref_registry_simple.h"
// #include "components/prefs/pref_service.h"
// #include "components/prefs/scoped_user_pref_update.h"

namespace psst {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  if (base::FeatureList::IsEnabled(psst::features::kEnablePsst)) {
    registry->RegisterBooleanPref(prefs::kPsstEnabled, true);
  }
}

}  // namespace psst
