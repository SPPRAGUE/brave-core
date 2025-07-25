/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "base/containers/adapters.h"

#define GetSearchProvidersUsingKeywordResult \
  GetSearchProvidersUsingKeywordResult_ChromiumImpl
#include <components/search_engines/util.cc>
#undef GetSearchProvidersUsingKeywordResult

void GetSearchProvidersUsingKeywordResult(
    const WDKeywordsResult& keyword_result,
    KeywordWebDataService* service,
    PrefService* prefs,
    const TemplateURLPrepopulateData::Resolver& template_url_data_resolver,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    WDKeywordsResult::Metadata& out_updated_keywords_metadata,
    std::set<std::string>* removed_keyword_guids) {
  // Call the original implementation to get template_urls.
  GetSearchProvidersUsingKeywordResult_ChromiumImpl(
      keyword_result, service, prefs, template_url_data_resolver, template_urls,
      default_search_provider, search_terms_data, out_updated_keywords_metadata,
      removed_keyword_guids);
  // Resort template_urls in the order of prepopulated search engines.
  if (template_urls && !template_urls->empty()) {
    std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
        template_url_data_resolver.GetPrepopulatedEngines();
    for (const auto& template_url_data : base::Reversed(prepopulated_urls)) {
      auto it = std::ranges::find_if(
          *template_urls,
          [&template_url_data](std::unique_ptr<TemplateURL>& t_url1) {
            return (t_url1->prepopulate_id() ==
                    template_url_data->prepopulate_id);
          });
      if (it != end(*template_urls))
        std::rotate(begin(*template_urls), it, it + 1);
    }
  }
}
