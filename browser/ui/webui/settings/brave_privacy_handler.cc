/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/webui/settings/brave_privacy_handler.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "brave/components/ai_chat/core/browser/utils.h"
#include "brave/components/ai_chat/core/common/features.h"
#include "brave/components/brave_shields/core/common/features.h"
#include "brave/components/constants/pref_names.h"
#include "brave/components/de_amp/common/features.h"
#include "brave/components/debounce/core/common/features.h"
#include "brave/components/google_sign_in_permission/google_sign_in_permission_util.h"
#include "brave/components/p3a/pref_names.h"
#include "brave/components/request_otr/common/buildflags/buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/gcm_driver/gcm_buildflags.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"

#if BUILDFLAG(ENABLE_REQUEST_OTR)
#include "brave/components/request_otr/common/features.h"
#endif

#if !BUILDFLAG(USE_GCM_FROM_PLATFORM)
#include "brave/browser/gcm_driver/brave_gcm_channel_status.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "brave/components/windows_recall/windows_recall.h"
#endif

BravePrivacyHandler::BravePrivacyHandler() {
  local_state_change_registrar_.Init(g_browser_process->local_state());
  local_state_change_registrar_.Add(
      kStatsReportingEnabled,
      base::BindRepeating(&BravePrivacyHandler::OnStatsUsagePingEnabledChanged,
                          base::Unretained(this)));
  local_state_change_registrar_.Add(
      kStatsReportingDisabledByPolicy,
      base::BindRepeating(&BravePrivacyHandler::OnStatsUsagePingEnabledChanged,
                          base::Unretained(this)));
  local_state_change_registrar_.Add(
      p3a::kP3AEnabled,
      base::BindRepeating(&BravePrivacyHandler::OnP3AEnabledChanged,
                          base::Unretained(this)));
  local_state_change_registrar_.Add(
      p3a::kP3ADisabledByPolicy,
      base::BindRepeating(&BravePrivacyHandler::OnP3AEnabledChanged,
                          base::Unretained(this)));
#if BUILDFLAG(IS_WIN)
  if (windows_recall::IsWindowsRecallAvailable()) {
    local_state_change_registrar_.Add(
        windows_recall::prefs::kWindowsRecallDisabled,
        base::BindRepeating(
            &BravePrivacyHandler::OnWindowsRecallDisabledChanged,
            base::Unretained(this)));
  }
#endif
}

BravePrivacyHandler::~BravePrivacyHandler() {
  local_state_change_registrar_.RemoveAll();
}

void BravePrivacyHandler::RegisterMessages() {
  profile_ = Profile::FromWebUI(web_ui());

  web_ui()->RegisterMessageCallback(
      "setP3AEnabled", base::BindRepeating(&BravePrivacyHandler::SetP3AEnabled,
                                           base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getP3AEnabled", base::BindRepeating(&BravePrivacyHandler::GetP3AEnabled,
                                           base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setStatsUsagePingEnabled",
      base::BindRepeating(&BravePrivacyHandler::SetStatsUsagePingEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getStatsUsagePingEnabled",
      base::BindRepeating(&BravePrivacyHandler::GetStatsUsagePingEnabled,
                          base::Unretained(this)));
#if BUILDFLAG(IS_WIN)
  if (windows_recall::IsWindowsRecallAvailable()) {
    web_ui()->RegisterMessageCallback(
        "isWindowsRecallDisabled",
        base::BindRepeating(&BravePrivacyHandler::GetLocalStateBooleanEnabled,
                            base::Unretained(this),
                            windows_recall::prefs::kWindowsRecallDisabled));
    web_ui()->RegisterMessageCallback(
        "setWindowsRecallDisabled",
        base::BindRepeating(&BravePrivacyHandler::SetLocalStateBooleanEnabled,
                            base::Unretained(this),
                            windows_recall::prefs::kWindowsRecallDisabled));
  }
#endif
}

// static
void BravePrivacyHandler::AddLoadTimeData(content::WebUIDataSource* data_source,
                                          Profile* profile) {
#if BUILDFLAG(USE_GCM_FROM_PLATFORM)
  data_source->AddBoolean("pushMessagingEnabledAtStartup", true);
#else
  gcm::BraveGCMChannelStatus* gcm_channel_status =
      gcm::BraveGCMChannelStatus::GetForProfile(profile);

  DCHECK(gcm_channel_status);
  data_source->AddBoolean("pushMessagingEnabledAtStartup",
                          gcm_channel_status->IsGCMEnabled());
#endif
  data_source->AddBoolean(
      "isDeAmpFeatureEnabled",
      base::FeatureList::IsEnabled(de_amp::features::kBraveDeAMP));
  data_source->AddBoolean(
      "isDebounceFeatureEnabled",
      base::FeatureList::IsEnabled(debounce::features::kBraveDebounce));
#if BUILDFLAG(ENABLE_REQUEST_OTR)
  data_source->AddBoolean(
      "isRequestOTRFeatureEnabled",
      base::FeatureList::IsEnabled(request_otr::features::kBraveRequestOTRTab));
#endif
  data_source->AddBoolean(
      "isGoogleSignInFeatureEnabled",
      google_sign_in_permission::IsGoogleSignInFeatureEnabled());
  data_source->AddBoolean(
      "isLocalhostAccessFeatureEnabled",
      base::FeatureList::IsEnabled(
          brave_shields::features::kBraveLocalhostAccessPermission));
  data_source->AddBoolean(
      "isOpenAIChatFromBraveSearchEnabled",
      ai_chat::IsAIChatEnabled(profile->GetPrefs()) &&
          ai_chat::features::IsOpenAIChatFromBraveSearchEnabled());
  auto* local_state = g_browser_process->local_state();
  data_source->AddBoolean("isP3ADisabledByPolicy",
                          local_state->GetBoolean(p3a::kP3ADisabledByPolicy));
  data_source->AddBoolean(
      "isStatsReportingDisabledByPolicy",
      local_state->GetBoolean(kStatsReportingDisabledByPolicy));

#if BUILDFLAG(IS_WIN)
  {
    data_source->AddBoolean("isWindowsRecallAvailable",
                            windows_recall::IsWindowsRecallAvailable());
    data_source->AddBoolean("windowsRecallDisabledAtStartup",
                            windows_recall::IsWindowsRecallDisabled(
                                g_browser_process->local_state()));
  }
#else
  data_source->AddBoolean("isWindowsRecallAvailable", false);
#endif
}

void BravePrivacyHandler::SetLocalStateBooleanEnabled(
    const std::string& path,
    const base::Value::List& args) {
  CHECK_EQ(args.size(), 1U);
  if (!args[0].is_bool()) {
    return;
  }

  bool enabled = args[0].GetBool();
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(path, enabled);
}

void BravePrivacyHandler::GetLocalStateBooleanEnabled(
    const std::string& path,
    const base::Value::List& args) {
  CHECK_EQ(args.size(), 1U);

  PrefService* local_state = g_browser_process->local_state();
  bool enabled = local_state->GetBoolean(path);

  AllowJavascript();
  ResolveJavascriptCallback(args[0], base::Value(enabled));
}

void BravePrivacyHandler::SetStatsUsagePingEnabled(
    const base::Value::List& args) {
  SetLocalStateBooleanEnabled(kStatsReportingEnabled, args);
}

void BravePrivacyHandler::GetStatsUsagePingEnabled(
    const base::Value::List& args) {
  GetLocalStateBooleanEnabled(kStatsReportingEnabled, args);
}

void BravePrivacyHandler::OnStatsUsagePingEnabledChanged() {
  if (IsJavascriptAllowed()) {
    PrefService* local_state = g_browser_process->local_state();
    bool user_enabled = local_state->GetBoolean(kStatsReportingEnabled);
    bool policy_disabled =
        local_state->GetBoolean(kStatsReportingDisabledByPolicy);

    FireWebUIListener("stats-usage-ping-enabled-changed", user_enabled,
                      policy_disabled);
  }
}

void BravePrivacyHandler::SetP3AEnabled(const base::Value::List& args) {
  SetLocalStateBooleanEnabled(p3a::kP3AEnabled, args);
}

void BravePrivacyHandler::GetP3AEnabled(const base::Value::List& args) {
  GetLocalStateBooleanEnabled(p3a::kP3AEnabled, args);
}

void BravePrivacyHandler::OnP3AEnabledChanged() {
  if (IsJavascriptAllowed()) {
    PrefService* local_state = g_browser_process->local_state();
    bool user_enabled = local_state->GetBoolean(p3a::kP3AEnabled);
    bool policy_disabled = local_state->GetBoolean(p3a::kP3ADisabledByPolicy);

    FireWebUIListener("p3a-enabled-changed", user_enabled, policy_disabled);
  }
}

#if BUILDFLAG(IS_WIN)
void BravePrivacyHandler::OnWindowsRecallDisabledChanged() {
  CHECK(windows_recall::IsWindowsRecallAvailable());
  if (!IsJavascriptAllowed()) {
    return;
  }
  FireWebUIListener("windows-recall-disabled-changed",
                    base::Value(g_browser_process->local_state()->GetBoolean(
                        windows_recall::prefs::kWindowsRecallDisabled)));
}
#endif
