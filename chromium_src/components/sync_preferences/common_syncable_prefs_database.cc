/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "components/sync_preferences/common_syncable_prefs_database.h"

#include <optional>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "brave/components/containers/buildflags/buildflags.h"
#include "components/search_engines/search_engines_pref_names.h"

#if BUILDFLAG(ENABLE_CONTAINERS)
#include "brave/components/containers/core/browser/pref_names.h"
#endif

namespace sync_preferences {
namespace {

namespace brave_syncable_prefs_ids {
enum {
  kSyncedDefaultPrivateSearchProviderGUID = 1000,
  kSyncedDefaultPrivateSearchProviderData = 1001,
#if BUILDFLAG(ENABLE_CONTAINERS)
  kContainersList = 1002,
#endif
};
}  // namespace brave_syncable_prefs_ids

constexpr auto kBraveCommonSyncablePrefsAllowlist = base::MakeFixedFlatMap<
    std::string_view,
    SyncablePrefMetadata>({
    {
        prefs::kSyncedDefaultPrivateSearchProviderGUID,
        {
            brave_syncable_prefs_ids::kSyncedDefaultPrivateSearchProviderGUID,
            syncer::PREFERENCES,
            sync_preferences::PrefSensitivity::kNone,
            MergeBehavior::kNone,
        },
    },
    {
        prefs::kSyncedDefaultPrivateSearchProviderData,
        {
            brave_syncable_prefs_ids::kSyncedDefaultPrivateSearchProviderData,
            syncer::PREFERENCES,
            sync_preferences::PrefSensitivity::kNone,
            MergeBehavior::kNone,
        },
    },
#if BUILDFLAG(ENABLE_CONTAINERS)
    {
        containers::prefs::kContainersList,
        {
            brave_syncable_prefs_ids::kContainersList,
            syncer::PREFERENCES,
            sync_preferences::PrefSensitivity::kNone,
            MergeBehavior::kNone,
        },
    },
#endif
    // See //components/sync_preferences/README.md about adding new entries
    // here.
    //
    // vvvvv IMPORTANT! vvvvv
    // Note to the reviewer: IT IS YOUR RESPONSIBILITY to ensure that new
    // syncable prefs follow privacy guidelines! See the readme file linked
    // above for guidance and escalation path in case anything is unclear.
    // ^^^^^ IMPORTANT! ^^^^^
});

}  // namespace
}  // namespace sync_preferences

#define GetSyncablePrefMetadata GetSyncablePrefMetadata_ChromiumOriginalImpl
#include <components/sync_preferences/common_syncable_prefs_database.cc>
#undef GetSyncablePrefMetadata

namespace sync_preferences {

std::optional<SyncablePrefMetadata>
CommonSyncablePrefsDatabase::GetSyncablePrefMetadata(
    std::string_view pref_name) const {
  const auto it = kBraveCommonSyncablePrefsAllowlist.find(pref_name);
  if (it != kBraveCommonSyncablePrefsAllowlist.end()) {
    return it->second;
  }
  return GetSyncablePrefMetadata_ChromiumOriginalImpl(pref_name);
}

std::optional<SyncablePrefMetadata>
CommonSyncablePrefsDatabase::GetSyncablePrefMetadata_ChromiumImpl(
    std::string_view pref_name) const {
  return GetSyncablePrefMetadata(pref_name);
}

}  // namespace sync_preferences
