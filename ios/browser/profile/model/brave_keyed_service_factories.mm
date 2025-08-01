/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/ios/browser/profile/model/brave_keyed_service_factories.h"

#include "brave/components/brave_account/features.h"
#include "brave/ios/browser/ai_chat/ai_chat_service_factory.h"
#include "brave/ios/browser/ai_chat/model_service_factory.h"
#include "brave/ios/browser/api/webcompat_reporter/webcompat_reporter_service_factory.h"
#include "brave/ios/browser/brave_account/brave_account_service_factory_ios.h"
#include "brave/ios/browser/brave_ads/ads_service_factory_ios.h"
#include "brave/ios/browser/brave_wallet/asset_ratio_service_factory.h"
#include "brave/ios/browser/brave_wallet/brave_wallet_ipfs_service_factory.h"
#include "brave/ios/browser/brave_wallet/brave_wallet_service_factory.h"
#include "brave/ios/browser/brave_wallet/meld_integration_service_factory.h"
#include "brave/ios/browser/brave_wallet/swap_service_factory.h"
#include "brave/ios/browser/debounce/debounce_service_factory+private.h"
#include "brave/ios/browser/favicon/brave_ios_favicon_loader_factory.h"
#include "brave/ios/browser/misc_metrics/profile_misc_metrics_service_factory.h"
#include "brave/ios/browser/skus/skus_service_factory.h"
#include "brave/ios/browser/url_sanitizer/url_sanitizer_service_factory+private.h"

namespace brave {

void EnsureProfileKeyedServiceFactoriesBuilt() {
  misc_metrics::ProfileMiscMetricsServiceFactory::GetInstance();
  ai_chat::ModelServiceFactory::GetInstance();
  ai_chat::AIChatServiceFactory::GetInstance();
  if (brave_account::features::IsBraveAccountEnabled()) {
    brave_account::BraveAccountServiceFactoryIOS::GetInstance();
  }
  brave_ads::AdsServiceFactoryIOS::GetInstance();
  brave_favicon::BraveIOSFaviconLoaderFactory::GetInstance();
  brave_wallet::AssetRatioServiceFactory::GetInstance();
  brave_wallet::BraveWalletIpfsServiceFactory::GetInstance();
  brave_wallet::BraveWalletServiceFactory::GetInstance();
  brave_wallet::MeldIntegrationServiceFactory::GetInstance();
  brave_wallet::SwapServiceFactory::GetInstance();
  skus::SkusServiceFactory::GetInstance();
  brave::URLSanitizerServiceFactory::GetInstance();
  debounce::DebounceServiceFactory::GetInstance();
  webcompat_reporter::WebcompatReporterServiceFactory::GetInstance();
}

}  // namespace brave
