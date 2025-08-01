// Copyright (c) 2023 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "brave/browser/ai_chat/ai_chat_service_factory.h"
#include "brave/browser/ai_chat/ai_chat_urls.h"
#include "brave/components/ai_chat/content/browser/ai_chat_tab_helper.h"
#include "brave/components/ai_chat/core/browser/ai_chat_metrics.h"
#include "brave/components/ai_chat/core/browser/ai_chat_service.h"
#include "brave/components/ai_chat/core/browser/conversation_handler.h"
#include "brave/components/ai_chat/core/common/features.h"
#include "brave/components/ai_chat/core/common/pref_names.h"
#include "brave/components/commander/common/buildflags/buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

#if !BUILDFLAG(IS_ANDROID)
#include "brave/browser/misc_metrics/profile_misc_metrics_service.h"
#include "brave/browser/misc_metrics/profile_misc_metrics_service_factory.h"
#include "brave/browser/ui/brave_browser.h"
#include "brave/browser/ui/sidebar/sidebar_controller.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#endif  // BUILDFLAG(!IS_ANDROID)

#if BUILDFLAG(ENABLE_COMMANDER)
#include "brave/browser/ui/commander/commander_service_factory.h"
#include "brave/components/commander/browser/commander_frontend_delegate.h"
#endif  // BUILDFLAG(ENABLE_COMMANDER)

#include <chrome/browser/autocomplete/chrome_autocomplete_provider_client.cc>

#if BUILDFLAG(ENABLE_COMMANDER)
commander::CommanderFrontendDelegate*
ChromeAutocompleteProviderClient::GetCommanderDelegate() {
  return commander::CommanderServiceFactory::GetForBrowserContext(profile_);
}
#endif  // BUILDFLAG(ENABLE_COMMANDER)

void ChromeAutocompleteProviderClient::OpenLeo(const std::u16string& query) {
#if !BUILDFLAG(IS_ANDROID)
  ai_chat::AIChatService* ai_chat_service =
      ai_chat::AIChatServiceFactory::GetForBrowserContext(profile_);

  if (!ai_chat_service) {
    return;
  }

  // Note that we're getting the last active browser. This is what upstream
  // does when they open the history journey from the omnibox. This seem to be
  // good enough because
  // * The time between the user typing and the journey opening is very small,
  // so active browser is unlikely to be changed
  // * Even if the active browser is changed, it'd be better to open the Leo in
  // the new active browser.
  Browser* browser =
      chrome::FindTabbedBrowser(profile_,
                                /*match_original_profiles=*/true);
  if (!browser) {
    return;
  }

  ai_chat::ConversationHandler* conversation_handler;

  if (ai_chat_service->IsAIChatHistoryEnabled() &&
      ai_chat::features::kOmniboxOpensFullPage.Get()) {
    conversation_handler = ai_chat_service->CreateConversation();
    browser->OpenURL({ai_chat::ConversationUrl(
                          conversation_handler->get_conversation_uuid()),
                      content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PageTransition::PAGE_TRANSITION_GENERATED, false},
                     {});
  } else {
    auto* chat_tab_helper = ai_chat::AIChatTabHelper::FromWebContents(
        browser->tab_strip_model()->GetActiveWebContents());
    DCHECK(chat_tab_helper);
    conversation_handler =
        ai_chat_service->GetOrCreateConversationHandlerForContent(
            chat_tab_helper->content_id(), chat_tab_helper->GetWeakPtr());
    if (!conversation_handler) {
      return;
    }

    // Before trying to activate the panel, unlink page content if needed.
    // This needs to be called before activating the panel to check against the
    // current state.
    conversation_handler->MaybeUnlinkAssociatedContent();

    // Activate the panel.
    auto* sidebar_controller = browser->GetFeatures().sidebar_controller();
    sidebar_controller->ActivatePanelItem(
        sidebar::SidebarItem::BuiltInItemType::kChatUI);
  }

  if (!conversation_handler) {
    return;
  }

  // Send the query to the AIChat's backend.
  ai_chat::mojom::ConversationTurnPtr turn =
      ai_chat::mojom::ConversationTurn::New(
          std::nullopt, ai_chat::mojom::CharacterType::HUMAN,
          ai_chat::mojom::ActionType::QUERY,
          base::UTF16ToUTF8(query) /* text */, std::nullopt /* prompt */,
          std::nullopt /* selected_text */, std::nullopt /* events */,
          base::Time::Now(), std::nullopt /* edits */,
          std::nullopt /* uploaded images */,
          false /* from_brave_search_SERP */, std::nullopt /* model_key */);

  auto* profile_metrics =
      misc_metrics::ProfileMiscMetricsServiceFactory::GetServiceForContext(
          profile_);
  if (profile_metrics) {
    auto* ai_chat_metrics = profile_metrics->GetAIChatMetrics();
    CHECK(ai_chat_metrics);
    ai_chat_metrics->RecordOmniboxOpen();
  }

  conversation_handler->SubmitHumanConversationEntry(std::move(turn));
#endif
}

bool ChromeAutocompleteProviderClient::IsLeoProviderEnabled() {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return profile_->IsRegularProfile() &&
         GetPrefs()->GetBoolean(
             ai_chat::prefs::kBraveChatAutocompleteProviderEnabled);
#endif
}

std::u16string ChromeAutocompleteProviderClient::GetClipboardText() const {
#if !BUILDFLAG(IS_ANDROID)
  return ::GetClipboardText(/*notify_if_restricted*/ false);
#else
  return u"";
#endif
}
