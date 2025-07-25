/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/webui/brave_vpn/brave_vpn_localized_string_provider.h"

#include "base/no_destructor.h"
#include "components/grit/brave_components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace brave_vpn {

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"braveVpn", IDS_BRAVE_VPN},
      {"braveVpnConnect", IDS_BRAVE_VPN_CONNECT},
      {"braveVpnConnecting", IDS_BRAVE_VPN_CONNECTING},
      {"braveVpnConnected", IDS_BRAVE_VPN_CONNECTED},
      {"braveVpnDisconnecting", IDS_BRAVE_VPN_DISCONNECTING},
      {"braveVpnDisconnected", IDS_BRAVE_VPN_DISCONNECTED},
      {"braveVpnConnectionFailed", IDS_BRAVE_VPN_CONNECTION_FAILED},
      {"braveVpnUnableConnectToServer", IDS_BRAVE_VPN_UNABLE_CONNECT_TO_SERVER},
      {"braveVpnTryAgain", IDS_BRAVE_VPN_TRY_AGAIN},
      {"braveVpnChooseAnotherServer", IDS_BRAVE_VPN_CHOOSE_ANOTHER_SERVER},
      {"braveVpnUnableConnectInfo", IDS_BRAVE_VPN_UNABLE_CONNECT_INFO},
      {"braveVpnBuy", IDS_BRAVE_VPN_BUY},
      {"braveVpnPurchased", IDS_BRAVE_VPN_HAS_PURCHASED},
      {"braveVpnPoweredBy", IDS_BRAVE_VPN_POWERED_BY},
      {"braveVpnSettingsPanelHeader", IDS_BRAVE_VPN_SETTINGS_PANEL_HEADER},
      {"braveVpnSettingsPanelBackButtonAriaLabel",
       IDS_BRAVE_VPN_SETTINGS_PANEL_BACK_BUTTON_ARIA_LABEL},
      {"braveVpnErrorPanelBackButtonAriaLabel",
       IDS_BRAVE_VPN_PANEL_GO_TO_MAIN_BUTTON_ARIA_LABEL},
      {"braveVpnStatus", IDS_BRAVE_VPN_STATUS},
      {"braveVpnExpires", IDS_BRAVE_VPN_EXPIRES},
      {"braveVpnManageSubscription", IDS_BRAVE_VPN_MANAGE_SUBSCRIPTION},
      {"braveVpnReconnectAutomatically", IDS_BRAVE_VPN_RECONNECT_AUTOMATICALLY},
      {"braveVpnSmartProxyRouting", IDS_BRAVE_VPN_SMART_PROXY_ROUTING},
      {"braveVpnSmartProxyRoutingDesc", IDS_BRAVE_VPN_SMART_PROXY_ROUTING_DESC},
      {"braveVpnSmartProxyRoutingDescLearnMore",
       IDS_BRAVE_VPN_SMART_PROXY_ROUTING_DESC_LEARN_MORE},
      {"braveVpnSmartProxyRoutingIconTooltip",
       IDS_BRAVE_VPN_SMART_PROXY_ROUTING_ICON_TOOLTIP},
      {"braveVpnContactSupport", IDS_BRAVE_VPN_CONTACT_SUPPORT},
      {"braveVpnAbout", IDS_BRAVE_VPN_ABOUT},
      {"braveVpnFeature1", IDS_BRAVE_VPN_FEATURE_1},
      {"braveVpnFeature2", IDS_BRAVE_VPN_FEATURE_2},
      {"braveVpnFeature3", IDS_BRAVE_VPN_FEATURE_3},
      {"braveVpnFeature4", IDS_BRAVE_VPN_FEATURE_4},
      {"braveVpnFeature5", IDS_BRAVE_VPN_FEATURE_5},
      {"braveVpnLoading", IDS_BRAVE_VPN_LOADING},
      {"braveVpnPurchaseFailed", IDS_BRAVE_VPN_PURCHASE_FAILED},
      {"braveVpnSelectYourServer", IDS_BRAVE_VPN_SELECT_YOUR_SERVER},
      {"braveVpnServerSelectionSingleCity",
       IDS_BRAVE_VPN_SERVER_SELECTION_SINGLE_CITY},
      {"braveVpnServerSelectionMultipleCities",
       IDS_BRAVE_VPN_SERVER_SELECTION_MULTIPLE_CITIES},
      {"braveVpnServerSelectionSingleServer",
       IDS_BRAVE_VPN_SERVER_SELECTION_SINGLE_SERVER},
      {"braveVpnServerSelectionMultipleServers",
       IDS_BRAVE_VPN_SERVER_SELECTION_MULTIPLE_SERVERS},
      {"braveVpnServerSelectionAutomaticLabel",
       IDS_BRAVE_VPN_SERVER_SELECTION_AUTOMATIC_LABEL},
      {"braveVpnServerSelectionOptimalLabel",
       IDS_BRAVE_VPN_SERVER_SELECTION_OPTIMAL_LABEL},
      {"braveVpnServerSelectionOptimalDesc",
       IDS_BRAVE_VPN_SERVER_SELECTION_OPTIMAL_DESC},
      {"braveVpnSelectPanelBackButtonAriaLabel",
       IDS_BRAVE_VPN_PANEL_GO_TO_MAIN_BUTTON_ARIA_LABEL},
      {"braveVpnSupportTicketFailed", IDS_BRAVE_VPN_SUPPORT_TICKET_FAILED},
      {"braveVpnEditPaymentMethod", IDS_BRAVE_VPN_EDIT_PAYMENT},
      {"braveVpnPaymentFailure", IDS_BRAVE_VPN_PAYMENT_FAILURE},
      {"braveVpnPaymentFailureReason", IDS_BRAVE_VPN_PAYMENT_FAILURE_REASON},
      {"braveVpnSupportPanelBackButtonAriaLabel",
       IDS_BRAVE_VPN_SUPPORT_PANEL_BACK_BUTTON_ARIA_LABEL},
      {"braveVpnSupportEmail", IDS_BRAVE_VPN_SUPPORT_EMAIL},
      {"braveVpnSupportEmailInputPlaceholder",
       IDS_BRAVE_VPN_SUPPORT_EMAIL_PLACEHOLDER},
      {"braveVpnSupportEmailNotValid", IDS_BRAVE_VPN_SUPPORT_EMAIL_NOT_VALID},
      {"braveVpnSupportFieldIsRequired",
       IDS_BRAVE_VPN_SUPPORT_FIELD_IS_REQUIRED},
      {"braveVpnSupportSubject", IDS_BRAVE_VPN_SUPPORT_SUBJECT},
      {"braveVpnSupportSubjectNotSet", IDS_BRAVE_VPN_SUPPORT_SUBJECT_NOTSET},
      {"braveVpnSupportSubjectOtherConnectionProblem",
       IDS_BRAVE_VPN_SUPPORT_SUBJECT_OTHER_CONNECTION_PROBLEM},
      {"braveVpnSupportSubjectNoInternet",
       IDS_BRAVE_VPN_SUPPORT_SUBJECT_NO_INTERNET},
      {"braveVpnSupportSubjectSlowConnection",
       IDS_BRAVE_VPN_SUPPORT_SUBJECT_SLOW_CONNECTION},
      {"braveVpnSupportSubjectWebsiteDoesntWork",
       IDS_BRAVE_VPN_SUPPORT_SUBJECT_WEBSITE_DOESNT_WORK},
      {"braveVpnSupportSubjectOther", IDS_BRAVE_VPN_SUPPORT_SUBJECT_OTHER},
      {"braveVpnSupportBody", IDS_BRAVE_VPN_SUPPORT_BODY},
      {"braveVpnSupportDescriptionPlaceholder",
       IDS_BRAVE_VPN_SUPPORT_BODY_PLACEHOLDER},
      {"braveVpnSupportOptionalHeader", IDS_BRAVE_VPN_SUPPORT_OPTIONAL_HEADER},
      {"braveVpnSupportOptionalNotes", IDS_BRAVE_VPN_SUPPORT_OPTIONAL_NOTES},
      {"braveVpnSupportOptionalNotesPrivacyPolicy",
       IDS_BRAVE_VPN_SUPPORT_OPTIONAL_NOTES_PRIVACY_POLICY},
      {"braveVpnSupportOptionalVpnHostname",
       IDS_BRAVE_VPN_SUPPORT_OPTIONAL_VPN_HOSTNAME},
      {"braveVpnSupportOptionalAppVersion",
       IDS_BRAVE_VPN_SUPPORT_OPTIONAL_APP_VERSION},
      {"braveVpnSupportOptionalOsVersion",
       IDS_BRAVE_VPN_SUPPORT_OPTIONAL_OS_VERSION},
      {"braveVpnSupportNotes", IDS_BRAVE_VPN_SUPPORT_NOTES},
      {"braveVpnSupportSubmit", IDS_BRAVE_VPN_SUPPORT_SUBMIT},
      {"braveVpnConnectNotAllowed", IDS_BRAVE_VPN_CONNECT_NOT_ALLOWED},
      {"braveVpnSupportTimezone", IDS_BRAVE_VPN_SUPPORT_TIMEZONE},
      {"braveVpnSessionExpiredTitle",
       IDS_BRAVE_VPN_MAIN_PANEL_SESSION_EXPIRED_PART_TITLE},
      {"braveVpnSettingsTooltip", IDS_BRAVE_VPN_MAIN_PANEL_VPN_SETTINGS_TITLE},
      {"braveVpnSessionExpiredContent",
       IDS_BRAVE_VPN_MAIN_PANEL_SESSION_EXPIRED_PART_CONTENT},
      {"braveVpnOutOfCredentials",
       IDS_BRAVE_VPN_MAIN_PANEL_OUT_OF_CREDENTIALS_TITLE},
  };

  for (const auto& str : kLocalizedStrings) {
    std::u16string l10n_str = l10n_util::GetStringUTF16(str.id);
    html_source->AddString(str.name, l10n_str);
  }
}

}  // namespace brave_vpn
