/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/time/time.h"
#include "brave/browser/ai_chat/ai_chat_service_factory.h"
#include "brave/browser/ai_chat/ai_chat_urls.h"
#include "brave/components/ai_chat/content/browser/ai_chat_tab_helper.h"
#include "brave/components/ai_chat/core/browser/ai_chat_service.h"
#include "brave/components/ai_chat/core/browser/conversation_handler.h"
#include "brave/components/ai_chat/core/common/mojom/ai_chat.mojom.h"
#include "brave/components/constants/webui_url_constants.h"
#include "chrome/android/chrome_jni_headers/BraveLeoUtils_jni.h"
#include "content/public/browser/web_contents.h"

namespace ai_chat {

static void JNI_BraveLeoUtils_OpenLeoQuery(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jstring>& conversation_uuid,
    const base::android::JavaParamRef<jstring>& query) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  AIChatService* ai_chat_service = AIChatServiceFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());
  CHECK(ai_chat_service);
  auto conversation_uuid_str =
      base::android::ConvertJavaStringToUTF8(conversation_uuid);
  // This function is either targeted at a specific conversation
  // or a conversation based on the provided WebContents.
  ConversationHandler* conversation;
  if (conversation_uuid_str.empty()) {
    AIChatTabHelper* chat_tab_helper =
        AIChatTabHelper::FromWebContents(web_contents);
    DCHECK(chat_tab_helper);
    conversation = ai_chat_service->GetOrCreateConversationHandlerForContent(
        chat_tab_helper->content_id(), chat_tab_helper->GetWeakPtr());
  } else {
    conversation = ai_chat_service->GetConversation(conversation_uuid_str);
  }
  if (!conversation) {
    return;
  }
  // Send the query
  conversation->MaybeUnlinkAssociatedContent();
  mojom::ConversationTurnPtr turn = mojom::ConversationTurn::New(
      std::nullopt, mojom::CharacterType::HUMAN, mojom::ActionType::QUERY,
      base::android::ConvertJavaStringToUTF8(query), std::nullopt /* prompt */,
      std::nullopt /* selected_text */, std::nullopt /* events */,
      base::Time::Now(), std::nullopt, std::nullopt /* uploaded images */,
      false, std::nullopt /* model_key */);
  conversation->SubmitHumanConversationEntry(std::move(turn));

  content::OpenURLParams params(
      ConversationUrl(conversation->get_conversation_uuid()),
      content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_FROM_API, false);
  web_contents->OpenURL(params, {});
}

static void JNI_BraveLeoUtils_OpenLeoUrlForTab(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  AIChatService* ai_chat_service = AIChatServiceFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());
  DCHECK(ai_chat_service);
  AIChatTabHelper* chat_tab_helper =
      AIChatTabHelper::FromWebContents(web_contents);
  DCHECK(chat_tab_helper);
  ConversationHandler* conversation =
      ai_chat_service->GetOrCreateConversationHandlerForContent(
          chat_tab_helper->content_id(), chat_tab_helper->GetWeakPtr());

  content::OpenURLParams params(
      ConversationUrl(conversation->get_conversation_uuid()),
      content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_FROM_API, false);
  web_contents->OpenURL(params, {});
}
}  // namespace ai_chat
