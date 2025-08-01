/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <string>

#include "components/autofill/core/browser/metrics/autofill_metrics.h"

class PrefService;
namespace syncer {
class SyncService;
}  // namespace syncer

namespace autofill {
class LogManager;
class PaymentsDataManager;
bool IsCreditCardUploadEnabled(
    const syncer::SyncService* sync_service,
    const PrefService& pref_service,
    const std::string& user_country,
    AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
    LogManager* log_manager) {
  return false;
}
}  // namespace autofill

#define IsCreditCardUploadEnabled IsCreditCardUploadEnabled_ChromiumImpl
#include <components/autofill/core/browser/studies/autofill_experiments.cc>
#undef IsCreditCardUploadEnabled
