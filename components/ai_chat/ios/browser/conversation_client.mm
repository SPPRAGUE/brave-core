// Copyright (c) 2024 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/ai_chat/ios/browser/conversation_client.h"

#include "base/strings/sys_string_conversions.h"
#include "brave/base/mac/conversions.h"
#include "brave/components/ai_chat/core/browser/ai_chat_service.h"
#include "brave/components/ai_chat/core/browser/conversation_handler.h"
#include "brave/components/ai_chat/core/common/mojom/ai_chat.mojom-shared.h"
#include "brave/components/ai_chat/core/common/mojom/ai_chat.mojom.h"
#include "brave/components/ai_chat/core/common/mojom/ios/ai_chat.mojom.objc+private.h"
#include "brave/components/ai_chat/ios/browser/ai_chat_delegate.h"

namespace ai_chat {

ConversationClient::ConversationClient(AIChatService* ai_chat_service,
                                       id<AIChatDelegate> bridge)
    : bridge_(bridge) {
  ai_chat_service->BindObserver(
      service_receiver_.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConversationClient::OnStateChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

ConversationClient::~ConversationClient() = default;

void ConversationClient::ChangeConversation(ConversationHandler* conversation) {
  receiver_.reset();
  conversation->Bind(receiver_.BindNewPipeAndPassRemote());
}

// MARK: - mojom::ConversationUI

void ConversationClient::OnConversationHistoryUpdate(
    mojom::ConversationTurnPtr entry) {
  [bridge_ onHistoryUpdate];
}

void ConversationClient::OnAPIRequestInProgress(bool in_progress) {
  [bridge_ onAPIRequestInProgress:in_progress];
}

void ConversationClient::OnAPIResponseError(mojom::APIError error) {
  [bridge_ onAPIResponseError:(AiChatAPIError)error];
}

void ConversationClient::OnModelDataChanged(
    const std::string& model_key,
    std::vector<mojom::ModelPtr> model_list) {
  NSMutableArray* models =
      [[NSMutableArray alloc] initWithCapacity:model_list.size()];

  for (auto& model : model_list) {
    [models addObject:[[AiChatModel alloc] initWithModelPtr:model->Clone()]];
  }

  [bridge_ onModelChanged:base::SysUTF8ToNSString(model_key) modelList:models];
}

void ConversationClient::OnSuggestedQuestionsChanged(
    const std::vector<std::string>& questions,
    mojom::SuggestionGenerationStatus status) {
  [bridge_
      onSuggestedQuestionsChanged:brave::vector_to_ns(questions)
                           status:(AiChatSuggestionGenerationStatus)status];
}

void ConversationClient::OnAssociatedContentInfoChanged(
    std::vector<mojom::AssociatedContentPtr> site_info) {
  NSMutableArray* infos =
      [[NSMutableArray alloc] initWithCapacity:site_info.size()];

  for (auto& info : site_info) {
    [infos addObject:[[AiChatAssociatedContent alloc]
                         initWithAssociatedContentPtr:info->Clone()]];
  }
  [bridge_ onPageHasContent:[infos copy]];
}

void ConversationClient::OnConversationDeleted() {
  // TODO(petemill): UI should bind to a new conversation. This only
  // needs to be handled when the AIChatStorage feature is enabled, which
  // allows deletion.
}

void ConversationClient::OnStateChanged(const mojom::ServiceStatePtr state) {
  [bridge_ onServiceStateChanged:[[AiChatServiceState alloc]
                                     initWithServiceStatePtr:state->Clone()]];
}

}  // namespace ai_chat
