/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/search_engines/search_engine_provider_util.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/values.h"
#include "brave/browser/search_engines/pref_names.h"
#include "brave/components/l10n/common/locale_util.h"
#include "brave/components/search_engines/brave_prepopulated_engines.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data_resolver_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"
#include "components/search_engines/template_url_service.h"

namespace brave {

namespace {
constexpr auto kTargetCountriesForEnableSearchSuggestionsByDefault =
    base::MakeFixedFlatSet<std::string_view>({"AR", "AT", "BR", "CA", "DE",
                                              "ES", "FR", "GB", "IN", "IT",
                                              "MX", "US"});
}

void SetBraveAsDefaultPrivateSearchProvider(Profile& profile) {
  auto& prefs = *profile.GetPrefs();
  auto* prepopulate_data_resolver =
      TemplateURLPrepopulateData::ResolverFactory::GetForProfile(&profile);
  const auto template_url_data =
      prepopulate_data_resolver->GetPrepopulatedEngine(
          TemplateURLPrepopulateData::PREPOPULATED_ENGINE_ID_BRAVE);
  DCHECK(template_url_data);
  prefs.SetString(prefs::kSyncedDefaultPrivateSearchProviderGUID,
                  template_url_data->sync_guid);
  prefs.SetDict(prefs::kSyncedDefaultPrivateSearchProviderData,
                TemplateURLDataToDictionary(*template_url_data));
}

void UpdateDefaultPrivateSearchProviderData(Profile& profile) {
  auto* service = TemplateURLServiceFactory::GetForProfile(&profile);
  DCHECK(service->loaded());

  auto& prefs = *profile.GetPrefs();
  const std::string private_provider_guid =
      prefs.GetString(prefs::kSyncedDefaultPrivateSearchProviderGUID);

  if (private_provider_guid.empty()) {
    // This can happen while resetting whole settings.
    // In this case, set brave as a default search provider.
    SetBraveAsDefaultPrivateSearchProvider(profile);
    return;
  }

  // Sync kSyncedDefaultPrivateSearchProviderData with newly updated provider's
  // one.
  if (auto* url = service->GetTemplateURLForGUID(private_provider_guid)) {
    prefs.SetDict(prefs::kSyncedDefaultPrivateSearchProviderData,
                  TemplateURLDataToDictionary(url->data()));
    return;
  }

  // When user delete current private search provder from provider list in
  // settings page, |private_provider_guid| will not be existed in the list. Use
  // Brave.
  SetBraveAsDefaultPrivateSearchProvider(profile);
}

void PrepareDefaultPrivateSearchProviderDataIfNeeded(Profile& profile) {
  auto& prefs = *profile.GetPrefs();
  auto* preference =
      prefs.FindPreference(prefs::kSyncedDefaultPrivateSearchProviderGUID);

  if (!preference)
    return;

  auto* service = TemplateURLServiceFactory::GetForProfile(&profile);
  DCHECK(service->loaded());

  const std::string private_provider_guid =
      prefs.GetString(prefs::kSyncedDefaultPrivateSearchProviderGUID);

  // Set Brave as a private window's initial search provider.
  if (private_provider_guid.empty()) {
    SetBraveAsDefaultPrivateSearchProvider(profile);
    return;
  }

  preference =
      prefs.FindPreference(prefs::kSyncedDefaultPrivateSearchProviderData);
  // Cache if url data is not yet existed.
  if (preference->IsDefaultValue()) {
    if (auto* url = service->GetTemplateURLForGUID(private_provider_guid)) {
      prefs.SetDict(prefs::kSyncedDefaultPrivateSearchProviderData,
                    TemplateURLDataToDictionary(url->data()));
    } else {
      // This could happen with update default provider list when brave is not
      // updated for longtime. So it doesn't have any chance to cache url data.
      // Set Brave as default private search provider.
      SetBraveAsDefaultPrivateSearchProvider(profile);
    }
    return;
  }

  // In this case previous default private search provider doesn't exist in
  // current default provider list. This could happen when default provider list
  // is updated. Add previous provider to service.
  if (auto* url = service->GetTemplateURLForGUID(private_provider_guid); !url) {
    auto private_url_data =
        TemplateURLDataFromDictionary(preference->GetValue()->GetDict());
    private_url_data->id = kInvalidTemplateURLID;
    DCHECK_EQ(private_provider_guid, private_url_data->sync_guid);
    service->Add(std::make_unique<TemplateURL>(*private_url_data));
  }
}

void ResetDefaultPrivateSearchProvider(Profile& profile) {
  PrefService& prefs = *profile.GetPrefs();
  prefs.ClearPref(prefs::kSyncedDefaultPrivateSearchProviderGUID);
  prefs.ClearPref(prefs::kSyncedDefaultPrivateSearchProviderData);

  PrepareDefaultPrivateSearchProviderDataIfNeeded(profile);
}

void PrepareSearchSuggestionsConfig(PrefService& local_state, bool first_run) {
  if (!first_run) {
    return;
  }

  const std::string default_country_code =
      brave_l10n::GetDefaultISOCountryCodeString();

  const bool enable_search_suggestions_default_value =
      kTargetCountriesForEnableSearchSuggestionsByDefault.count(
          default_country_code);

  local_state.SetBoolean(kEnableSearchSuggestionsByDefault,
                         enable_search_suggestions_default_value);
}

void UpdateDefaultSearchSuggestionsPrefs(PrefService& local_state,
                                         PrefService& profile_prefs) {
  if (!local_state.GetBoolean(kEnableSearchSuggestionsByDefault)) {
    return;
  }

  profile_prefs.SetDefaultPrefValue(prefs::kSearchSuggestEnabled,
                                    base::Value(true));
}

}  // namespace brave
