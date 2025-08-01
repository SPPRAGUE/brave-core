// Copyright (c) 2024 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/ai_chat/core/browser/ai_chat_service.h"

#include <algorithm>
#include <compare>
#include <functional>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/clamped_math.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "base/uuid.h"
#include "brave/components/ai_chat/core/browser/ai_chat_credential_manager.h"
#include "brave/components/ai_chat/core/browser/ai_chat_database.h"
#include "brave/components/ai_chat/core/browser/ai_chat_metrics.h"
#include "brave/components/ai_chat/core/browser/associated_content_manager.h"
#include "brave/components/ai_chat/core/browser/constants.h"
#include "brave/components/ai_chat/core/browser/conversation_handler.h"
#include "brave/components/ai_chat/core/browser/model_service.h"
#include "brave/components/ai_chat/core/browser/tab_tracker_service.h"
#include "brave/components/ai_chat/core/browser/utils.h"
#include "brave/components/ai_chat/core/common/constants.h"
#include "brave/components/ai_chat/core/common/features.h"
#include "brave/components/ai_chat/core/common/mojom/ai_chat.mojom-forward.h"
#include "brave/components/ai_chat/core/common/mojom/ai_chat.mojom-shared.h"
#include "brave/components/ai_chat/core/common/mojom/ai_chat.mojom.h"
#include "brave/components/ai_chat/core/common/pref_names.h"
#include "build/build_config.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/url_constants.h"

namespace ai_chat {
namespace {

constexpr base::FilePath::StringViewType kDBFileName =
    FILE_PATH_LITERAL("AIChat");

std::vector<mojom::Conversation*> GetConversationsSortedByUpdatedTime(
    std::map<std::string, mojom::ConversationPtr, std::less<>>&
        conversations_map) {
  std::vector<mojom::Conversation*> conversations;
  for (const auto& kv : conversations_map) {
    auto& conversation = kv.second;
    conversations.push_back(conversation.get());
  }
  std::ranges::sort(conversations, std::greater<>(),
                    &mojom::Conversation::updated_time);
  return conversations;
}

bool IsConversationUpdatedTimeWithinRange(
    std::optional<base::Time> begin_time,
    std::optional<base::Time> end_time,
    mojom::ConversationPtr& conversation) {
  return ((!begin_time.has_value() || begin_time->is_null() ||
           conversation->updated_time >= begin_time) &&
          (!end_time.has_value() || end_time->is_null() || end_time->is_max() ||
           conversation->updated_time <= end_time));
}

std::vector<mojom::AssociatedContentPtr> CloneAssociatedContent(
    const std::vector<mojom::AssociatedContentPtr>& associated_content) {
  std::vector<mojom::AssociatedContentPtr> cloned_content;
  for (const auto& content : associated_content) {
    cloned_content.push_back(content->Clone());
  }
  return cloned_content;
}
}  // namespace

AIChatService::AIChatService(
    ModelService* model_service,
    TabTrackerService* tab_tracker_service,
    std::unique_ptr<AIChatCredentialManager> ai_chat_credential_manager,
    PrefService* profile_prefs,
    AIChatMetrics* ai_chat_metrics,
    os_crypt_async::OSCryptAsync* os_crypt_async,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string_view channel_string,
    base::FilePath profile_path)
    : model_service_(model_service),
      tab_tracker_service_(tab_tracker_service),
      profile_prefs_(profile_prefs),
      ai_chat_metrics_(ai_chat_metrics),
      os_crypt_async_(os_crypt_async),
      url_loader_factory_(url_loader_factory),
      feedback_api_(
          std::make_unique<AIChatFeedbackAPI>(url_loader_factory_,
                                              std::string(channel_string))),
      credential_manager_(std::move(ai_chat_credential_manager)),
      profile_path_(profile_path) {
  DCHECK(profile_prefs_);
  pref_change_registrar_.Init(profile_prefs_);
  pref_change_registrar_.Add(
      prefs::kLastAcceptedDisclaimer,
      base::BindRepeating(&AIChatService::OnUserOptedIn,
                          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kBraveChatStorageEnabled,
      base::BindRepeating(&AIChatService::MaybeInitStorage,
                          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kUserDismissedPremiumPrompt,
      base::BindRepeating(&AIChatService::OnStateChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kUserDismissedStorageNotice,
      base::BindRepeating(&AIChatService::OnStateChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  MaybeInitStorage();

  // Get current premium status to report metrics
  GetPremiumStatus(base::DoNothing());
}

AIChatService::~AIChatService() = default;

mojo::PendingRemote<mojom::Service> AIChatService::MakeRemote() {
  mojo::PendingRemote<mojom::Service> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void AIChatService::Bind(mojo::PendingReceiver<mojom::Service> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AIChatService::Shutdown() {
  // Disconnect remotes
  receivers_.ClearWithReason(0, "Shutting down");
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (ai_chat_db_) {
    ai_chat_db_.Reset();
  }
}

ConversationHandler* AIChatService::CreateConversation() {
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  std::string conversation_uuid = uuid.AsLowercaseString();
  // Create the conversation metadata
  {
    mojom::ConversationPtr conversation = mojom::Conversation::New(
        conversation_uuid, "", base::Time::Now(), false, std::nullopt, 0, 0,
        false, std::vector<mojom::AssociatedContentPtr>());
    conversations_.insert_or_assign(conversation_uuid, std::move(conversation));
  }
  mojom::Conversation* conversation =
      conversations_.at(conversation_uuid).get();
  // Create the ConversationHandler. We don't persist it until it has data.
  std::unique_ptr<ConversationHandler> conversation_handler =
      std::make_unique<ConversationHandler>(
          conversation, this, model_service_, credential_manager_.get(),
          feedback_api_.get(), url_loader_factory_);
  conversation_observations_.AddObservation(conversation_handler.get());

  // Own it
  conversation_handlers_.insert_or_assign(conversation_uuid,
                                          std::move(conversation_handler));

  DVLOG(1) << "Created conversation " << conversation_uuid << "\nNow have "
           << conversations_.size() << " conversations and "
           << conversation_handlers_.size() << " loaded in memory.";

  // TODO(petemill): Is this neccessary? This conversation won't be
  // considered visible until it has entries.
  OnConversationListChanged();

  // Return the raw pointer
  return GetConversation(conversation_uuid);
}

ConversationHandler* AIChatService::GetConversation(
    std::string_view conversation_uuid) {
  auto conversation_handler_it =
      conversation_handlers_.find(conversation_uuid.data());
  if (conversation_handler_it == conversation_handlers_.end()) {
    return nullptr;
  }
  return conversation_handler_it->second.get();
}

void AIChatService::GetConversation(
    std::string_view conversation_uuid,
    base::OnceCallback<void(ConversationHandler*)> callback) {
  if (ConversationHandler* cached_conversation =
          GetConversation(conversation_uuid)) {
    DVLOG(4) << __func__ << " found cached conversation for "
             << conversation_uuid;
    std::move(callback).Run(cached_conversation);
    return;
  }
  // Load from database
  if (!ai_chat_db_) {
    std::move(callback).Run(nullptr);
    return;
  }
  LoadConversationsLazy(base::BindOnce(
      [](base::WeakPtr<AIChatService> instance, std::string conversation_uuid,
         base::OnceCallback<void(ConversationHandler*)> callback,
         ConversationMap& conversations) {
        auto conversation_it = conversations.find(conversation_uuid);
        if (conversation_it == conversations.end()) {
          std::move(callback).Run(nullptr);
          return;
        }
        mojom::ConversationPtr& metadata = conversation_it->second;
        // Get archive content and conversation entries
        instance->ai_chat_db_.AsyncCall(&AIChatDatabase::GetConversationData)
            .WithArgs(metadata->uuid)
            .Then(base::BindOnce(&AIChatService::OnConversationDataReceived,
                                 std::move(instance), metadata->uuid,
                                 std::move(callback)));
      },
      weak_ptr_factory_.GetWeakPtr(), std::string(conversation_uuid),
      std::move(callback)));
}

void AIChatService::OnConversationDataReceived(
    std::string conversation_uuid,
    base::OnceCallback<void(ConversationHandler*)> callback,
    mojom::ConversationArchivePtr data) {
  DVLOG(4) << __func__ << " for " << conversation_uuid
           << " with data: " << data->entries.size() << " entries and "
           << data->associated_content.size() << " contents";
  auto conversation_it = conversations_.find(conversation_uuid);
  if (conversation_it == conversations_.end()) {
    std::move(callback).Run(nullptr);
    return;
  }
  mojom::Conversation* conversation = conversation_it->second.get();
  std::unique_ptr<ConversationHandler> conversation_handler =
      std::make_unique<ConversationHandler>(
          conversation, this, model_service_, credential_manager_.get(),
          feedback_api_.get(), url_loader_factory_, std::move(data));
  conversation_observations_.AddObservation(conversation_handler.get());
  conversation_handlers_.insert_or_assign(conversation_uuid,
                                          std::move(conversation_handler));
  std::move(callback).Run(GetConversation(conversation_uuid));
}

ConversationHandler* AIChatService::GetOrCreateConversationHandlerForContent(
    int associated_content_id,
    base::WeakPtr<AssociatedContentDelegate> associated_content) {
  ConversationHandler* conversation = nullptr;
  auto conversation_uuid_it =
      content_conversations_.find(associated_content_id);
  if (conversation_uuid_it != content_conversations_.end()) {
    auto conversation_uuid = conversation_uuid_it->second;
    // Load from memory or database, but probably not database as if the
    // conversation is in the associated content map then it's probably recent
    // and still in memory.
    conversation = GetConversation(conversation_uuid);
  }
  if (!conversation) {
    // New conversation needed
    conversation = CreateConversationHandlerForContent(associated_content_id,
                                                       associated_content);
  }

  return conversation;
}

ConversationHandler* AIChatService::CreateConversationHandlerForContent(
    int associated_content_id,
    base::WeakPtr<AssociatedContentDelegate> associated_content) {
  ConversationHandler* conversation = CreateConversation();
  // Provide the content delegate, if allowed. If we aren't initially enabling
  // the context we still need to call MaybeAssociateContent so the conversation
  // knows what the current tab is.
  MaybeAssociateContent(
      conversation, associated_content_id,
      features::IsPageContextEnabledInitially() ? associated_content : nullptr);

  return conversation;
}

void AIChatService::DeleteConversations(std::optional<base::Time> begin_time,
                                        std::optional<base::Time> end_time) {
  if (!begin_time.has_value() && !end_time.has_value()) {
    // Delete all conversations
    // Delete in-memory data
    conversation_observations_.RemoveAllObservations();
    conversation_handlers_.clear();
    conversations_.clear();
    content_conversations_.clear();

    // Delete database data
    if (ai_chat_db_) {
      ai_chat_db_.AsyncCall(base::IgnoreResult(&AIChatDatabase::DeleteAllData));
      ReloadConversations();
    }
    if (ai_chat_metrics_ != nullptr) {
      ai_chat_metrics_->RecordConversationsCleared();
    }
    OnConversationListChanged();
    return;
  }

  // Get all keys from conversations_
  std::vector<std::string> conversation_keys;

  for (auto& [uuid, conversation] : conversations_) {
    if (IsConversationUpdatedTimeWithinRange(begin_time, end_time,
                                             conversation)) {
      conversation_keys.push_back(uuid);
    }
  }

  for (const auto& uuid : conversation_keys) {
    DeleteConversation(uuid);
  }
  if (!conversation_keys.empty()) {
    OnConversationListChanged();
  }
}

void AIChatService::DeleteAssociatedWebContent(
    std::optional<base::Time> begin_time,
    std::optional<base::Time> end_time,
    base::OnceCallback<void(bool)> callback) {
  if (!ai_chat_db_) {
    std::move(callback).Run(true);
    return;
  }

  ai_chat_db_.AsyncCall(&AIChatDatabase::DeleteAssociatedWebContent)
      .WithArgs(begin_time, end_time)
      .Then(std::move(callback));

  // Update local data
  ReloadConversations();
}

void AIChatService::MaybeInitStorage() {
  if (IsAIChatHistoryEnabled()) {
    if (!ai_chat_db_) {
      DVLOG(0) << "Initializing OS Crypt Async";
      os_crypt_async_->GetInstance(base::BindOnce(
          &AIChatService::OnOsCryptAsyncReady, weak_ptr_factory_.GetWeakPtr()));
      // Don't init DB until oscrypt is ready - we don't want to use the DB
      // if we can't use encryption.
    }
  } else {
    // Delete all stored data from database
    if (ai_chat_db_) {
      DVLOG(0) << "Unloading AI Chat database due to pref change";
      base::SequenceBound<std::unique_ptr<AIChatDatabase>> ai_chat_db =
          std::move(ai_chat_db_);
      ai_chat_db.AsyncCall(&AIChatDatabase::DeleteAllData)
          .Then(base::BindOnce(&AIChatService::OnDataDeletedForDisabledStorage,
                               weak_ptr_factory_.GetWeakPtr()));
    }
  }
  OnStateChanged();
}

void AIChatService::OnOsCryptAsyncReady(os_crypt_async::Encryptor encryptor) {
  CHECK(features::IsAIChatHistoryEnabled());
  // Pref might have changed since we started this process
  if (!profile_prefs_->GetBoolean(prefs::kBraveChatStorageEnabled)) {
    return;
  }
  ai_chat_db_ = base::SequenceBound<std::unique_ptr<AIChatDatabase>>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      std::make_unique<AIChatDatabase>(profile_path_.Append(kDBFileName),
                                       std::move(encryptor)));
}

void AIChatService::OnDataDeletedForDisabledStorage(bool success) {
  // Remove any conversations from in-memory that aren't connected to UI.
  // This is done now, in the callback from DeleteAllData, in case there
  // was any in-progress operations that would have resulted in adding data
  // back to conversations_ whilst waiting for DeleteAllData to complete.
  std::vector<ConversationHandler*> all_conversation_handlers;
  for (auto& [_, conversation_handler] : conversation_handlers_) {
    all_conversation_handlers.push_back(conversation_handler.get());
  }
  for (auto* conversation_handler : all_conversation_handlers) {
    QueueMaybeUnloadConversation(conversation_handler);
  }
  // Remove any conversation metadata that isn't connected to a still-alive
  // handler.
  for (auto it = conversations_.begin(); it != conversations_.end();) {
    if (!conversation_handlers_.contains(it->first)) {
      it = conversations_.erase(it);
    } else {
      ++it;
    }
  }
  OnConversationListChanged();
  // Re-check the preference since it could have been re-enabled
  // whilst the database operation was in progress. If so, we can re-use
  // the same database instance (post data deletion).
  if (!IsAIChatHistoryEnabled()) {
    // If there is a LoadConversationsLazy in progress, it will get cancelled
    // on destruction of ai_chat_db_ so call the callbacks.
    if (on_conversations_loaded_callbacks_.has_value() &&
        !on_conversations_loaded_callbacks_->empty()) {
      for (auto& callback : on_conversations_loaded_callbacks_.value()) {
        std::move(callback).Run(conversations_);
      }
    }

    ai_chat_db_.Reset();
    cancel_conversation_load_callback_ = base::NullCallback();
    on_conversations_loaded_callbacks_ = std::nullopt;
  }
}

void AIChatService::LoadConversationsLazy(ConversationMapCallback callback) {
  // Send immediately if we have finished loading from storage
  if (!ai_chat_db_ || (on_conversations_loaded_callbacks_.has_value() &&
                       on_conversations_loaded_callbacks_->empty())) {
    std::move(callback).Run(conversations_);
    return;
  }
  if (on_conversations_loaded_callbacks_.has_value()) {
    on_conversations_loaded_callbacks_->push_back(std::move(callback));
    return;
  }

  on_conversations_loaded_callbacks_ = std::vector<ConversationMapCallback>();
  on_conversations_loaded_callbacks_->push_back(std::move(callback));
  ai_chat_db_.AsyncCall(&AIChatDatabase::GetAllConversations)
      .Then(base::BindOnce(&AIChatService::OnLoadConversationsLazyData,
                           weak_ptr_factory_.GetWeakPtr()));
}

void AIChatService::OnLoadConversationsLazyData(
    std::vector<mojom::ConversationPtr> conversations) {
  if (!cancel_conversation_load_callback_.is_null()) {
    std::move(cancel_conversation_load_callback_).Run();
    cancel_conversation_load_callback_ = base::NullCallback();
    return;
  }
  DVLOG(1) << "Loaded " << conversations.size() << " conversations.";
  for (auto& conversation : conversations) {
    std::string uuid = conversation->uuid;
    DVLOG(2) << "Loaded conversation " << conversation->uuid
             << " with details: " << "\n has content: "
             << conversation->has_content
             << "\n last updated: " << conversation->updated_time
             << "\n title: " << conversation->title
             << "\n total tokens: " << conversation->total_tokens
             << "\n trimmed tokens: " << conversation->trimmed_tokens;
    // It's ok to overwrite existing metadata - some operations may modify
    // the database data and we want to keep the in-memory data synchronised.
    auto existing_conversation_it = conversations_.find(uuid);
    if (existing_conversation_it != conversations_.end()) {
      auto& existing_conversation = existing_conversation_it->second;
      existing_conversation->title = conversation->title;
      existing_conversation->total_tokens = conversation->total_tokens;
      existing_conversation->trimmed_tokens = conversation->trimmed_tokens;
      existing_conversation->updated_time = conversation->updated_time;
      existing_conversation->has_content = conversation->has_content;
      existing_conversation->model_key = conversation->model_key;
      existing_conversation->associated_content =
          std::move(conversation->associated_content);
    } else {
      conversations_.emplace(uuid, std::move(conversation));
    }
    auto handler_it = conversation_handlers_.find(uuid);
    if (handler_it != conversation_handlers_.end()) {
      // Notify the handler that metadata is possibly changed
      ConversationHandler* handler = handler_it->second.get();
      // If a reload was asked for, then we should also update the deeper
      // conversation data from the database, since the reload was likely due
      // to underlying data changing.
      ai_chat_db_.AsyncCall(&AIChatDatabase::GetConversationData)
          .WithArgs(uuid)
          .Then(base::BindOnce(
              [](base::WeakPtr<ConversationHandler> handler,
                 mojom::ConversationArchivePtr updated_data) {
                if (!handler) {
                  return;
                }
                DVLOG(1) << handler->get_conversation_uuid() << " read "
                         << updated_data->associated_content.size()
                         << " pieces of associated content from DB";
                handler->OnArchiveContentUpdated(std::move(updated_data));
              },
              handler->GetWeakPtr()));
    }
  }
  if (on_conversations_loaded_callbacks_.has_value()) {
    for (auto& callback : on_conversations_loaded_callbacks_.value()) {
      std::move(callback).Run(conversations_);
    }
    on_conversations_loaded_callbacks_->clear();
  }
  OnConversationListChanged();
}

void AIChatService::ReloadConversations(bool from_cancel) {
  // If in the middle of a conversation load, then make sure data is ignored,
  // and ask again when current load is complete.
  if (!from_cancel && on_conversations_loaded_callbacks_.has_value() &&
      !on_conversations_loaded_callbacks_->empty()) {
    cancel_conversation_load_callback_ =
        base::BindOnce(&AIChatService::ReloadConversations,
                       weak_ptr_factory_.GetWeakPtr(), true);
    return;
  }

  // Collect any previous callbacks and force conversations to load again
  std::vector<ConversationMapCallback> previous_callbacks;
  if (on_conversations_loaded_callbacks_.has_value()) {
    on_conversations_loaded_callbacks_->swap(previous_callbacks);
  }
  on_conversations_loaded_callbacks_ = std::nullopt;
  LoadConversationsLazy(base::DoNothing());

  // Re-queue any previous callbacks
  for (auto& callback : previous_callbacks) {
    LoadConversationsLazy(std::move(callback));
  }
}

void AIChatService::MaybeAssociateContent(
    ConversationHandler* conversation,
    int associated_content_id,
    base::WeakPtr<AssociatedContentDelegate> associated_content) {
  if (associated_content &&
      kAllowedContentSchemes.contains(associated_content->url().scheme())) {
    conversation->associated_content_manager()->AddContent(
        associated_content.get(),
        /*notify_updated=*/true);
  }
  // Record that this is the latest conversation for this content. Even
  // if we don't call SetAssociatedContentDelegate, the conversation still
  // has a default Tab's navigation for which is is associated. The Conversation
  // won't use that Tab's Page for context.
  content_conversations_.insert_or_assign(
      associated_content_id, conversation->get_conversation_uuid());
}

void AIChatService::MarkAgreementAccepted() {
  SetUserOptedIn(profile_prefs_, true);
}

void AIChatService::EnableStoragePref() {
  profile_prefs_->SetBoolean(prefs::kBraveChatStorageEnabled, true);
}

void AIChatService::DismissStorageNotice() {
  profile_prefs_->SetBoolean(prefs::kUserDismissedStorageNotice, true);
}

void AIChatService::DismissPremiumPrompt() {
  profile_prefs_->SetBoolean(prefs::kUserDismissedPremiumPrompt, true);
}

void AIChatService::GetActionMenuList(GetActionMenuListCallback callback) {
  std::move(callback).Run(ai_chat::GetActionMenuList());
}

void AIChatService::GetPremiumStatus(GetPremiumStatusCallback callback) {
  credential_manager_->GetPremiumStatus(
      base::BindOnce(&AIChatService::OnPremiumStatusReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AIChatService::DeleteConversation(const std::string& id) {
  auto handler_it = conversation_handlers_.find(id);
  if (handler_it != conversation_handlers_.end()) {
    conversation_observations_.RemoveObservation(handler_it->second.get());
    conversation_handlers_.erase(id);
    if (ai_chat_metrics_) {
      ai_chat_metrics_->RecordConversationUnload(id);
    }
  }
  bool temporary = false;
  auto conversation_it = conversations_.find(id);
  if (conversation_it != conversations_.end()) {
    temporary = (*conversation_it).second->temporary;
    conversations_.erase(conversation_it);
  }
  DVLOG(1) << "Erased conversation due to deletion request (" << id
           << "). Now have " << conversations_.size()
           << " Conversation metadata items and "
           << conversation_handlers_.size()
           << " ConversationHandler instances.";
  OnConversationListChanged();
  // Update database
  if (ai_chat_db_ && !temporary) {
    ai_chat_db_
        .AsyncCall(base::IgnoreResult(&AIChatDatabase::DeleteConversation))
        .WithArgs(id);
  }
}

void AIChatService::RenameConversation(const std::string& id,
                                       const std::string& new_name) {
  OnConversationTitleChanged(id, new_name);
}

void AIChatService::ConversationExists(const std::string& conversation_uuid,
                                       ConversationExistsCallback callback) {
  std::move(callback).Run(conversations_.contains(conversation_uuid));
}

void AIChatService::OnPremiumStatusReceived(GetPremiumStatusCallback callback,
                                            mojom::PremiumStatus status,
                                            mojom::PremiumInfoPtr info) {
#if BUILDFLAG(IS_ANDROID)
  // There is no UI for android to "refresh" with an iAP - we are likely still
  // authenticating after first iAP, so we should show as active.
  if (status == mojom::PremiumStatus::ActiveDisconnected &&
      profile_prefs_->GetBoolean(prefs::kBraveChatSubscriptionActiveAndroid)) {
    status = mojom::PremiumStatus::Active;
  }
#endif

  last_premium_status_ = status;
  if (ai_chat_metrics_ != nullptr) {
    ai_chat_metrics_->OnPremiumStatusUpdated(
        ai_chat::HasUserOptedIn(profile_prefs_), false, status, info.Clone());
  }
  model_service_->OnPremiumStatus(status);
  std::move(callback).Run(status, std::move(info));
}

bool AIChatService::CanUnloadConversation(ConversationHandler* conversation) {
  // Don't unload if there is active UI for the conversation
  if (conversation->IsAnyClientConnected()) {
    return false;
  }

  // We can keep a conversation with history in memory until there is no active
  // content unless it is a temporary chat which we remove it if no active UI.
  // TODO(petemill): With the history feature enabled, we should unload (if
  // there is no request in progress). However, we can only do this when
  // GetOrCreateConversationHandlerForContent allows a callback so that it
  // can provide an answer after loading the conversation content from storage.
  if (!conversation->GetIsTemporary() &&
      conversation->IsAssociatedContentAlive() &&
      conversation->HasAnyHistory()) {
    return false;
  }

  // Don't unload conversations that are in the middle of a request (they will
  // be unloaded when the request completes).
  //
  // Note: We wait for the request to complete even when history is disabled, as
  // it gives the UI a chance to connect before the conversation is unloaded.
  // This prevents a conversation from being unloaded synchronously when
  // submitting a conversation entry (as we won't delete it until the request
  // resolves), making the below possible:
  //
  // auto conversation = CreateConversation();
  // conversation->SubmitHumanConversationEntry(...);
  // auto id = conversation->get_conversation_uuid();
  //
  // There is still a risk the the will be unloaded before the UI connects, if
  // the request to the backend completes before the UI connects and in that
  // case if:
  // 1. History is enabled: We'll reload the conversation from storage.
  // 2. History is disabled: We'll show a blank conversation.

  if (conversation->IsRequestInProgress()) {
    return false;
  }

  return true;
}

void AIChatService::QueueMaybeUnloadConversation(
    ConversationHandler* conversation_handler) {
  // Only queue the MaybeUnload if we can unload the conversation now.
  if (!CanUnloadConversation(conversation_handler)) {
    return;
  }

  constexpr base::TimeDelta kUnloadDelay = base::Seconds(5);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AIChatService::MaybeUnloadConversation,
                     weak_ptr_factory_.GetWeakPtr(),
                     conversation_handler->GetWeakPtr()),
      kUnloadDelay);
}

void AIChatService::MaybeUnloadConversation(
    base::WeakPtr<ConversationHandler> conversation_handler) {
  // If the conversation has already been destroyed there's nothing to do.
  if (!conversation_handler) {
    return;
  }

  if (!CanUnloadConversation(conversation_handler.get())) {
    return;
  }

  bool has_history = conversation_handler->HasAnyHistory();
  bool is_temporary = conversation_handler->GetIsTemporary();

  auto uuid = conversation_handler->get_conversation_uuid();
  conversation_observations_.RemoveObservation(conversation_handler.get());
  conversation_handlers_.erase(uuid);

  DVLOG(1) << "Unloaded conversation (" << uuid << ") from memory. Now have "
           << conversations_.size() << " Conversation metadata items and "
           << conversation_handlers_.size()
           << " ConversationHandler instances.";
  if (!IsAIChatHistoryEnabled() || !has_history || is_temporary) {
    // Can erase because no active UI and no history, so it's
    // not a real / persistable conversation
    conversations_.erase(uuid);
    std::erase_if(content_conversations_,
                  [&uuid](const auto& kv) { return kv.second == uuid; });
    DVLOG(1) << "Erased conversation (" << uuid << "). Now have "
             << conversations_.size() << " Conversation metadata items and "
             << conversation_handlers_.size()
             << " ConversationHandler instances.";
    OnConversationListChanged();
  }
}

mojom::ServiceStatePtr AIChatService::BuildState() {
  bool has_user_dismissed_storage_notice =
      profile_prefs_->GetBoolean(prefs::kUserDismissedStorageNotice);
  base::Time last_accepted_disclaimer =
      profile_prefs_->GetTime(ai_chat::prefs::kLastAcceptedDisclaimer);

  bool is_user_opted_in = !last_accepted_disclaimer.is_null();

  // Premium prompt is only shown conditionally (e.g. the user hasn't dismissed
  // it and it's been some time since the user started using the feature).
  bool can_show_premium_prompt =
      !profile_prefs_->GetBoolean(prefs::kUserDismissedPremiumPrompt) &&
      !last_accepted_disclaimer.is_null() &&
      last_accepted_disclaimer < base::Time::Now() - base::Days(1);

  bool is_storage_enabled =
      profile_prefs_->GetBoolean(prefs::kBraveChatStorageEnabled);

  mojom::ServiceStatePtr state = mojom::ServiceState::New();
  state->has_accepted_agreement = is_user_opted_in;
  state->is_storage_pref_enabled = is_storage_enabled;
  state->is_storage_notice_dismissed = has_user_dismissed_storage_notice;
  state->can_show_premium_prompt = can_show_premium_prompt;
  return state;
}

void AIChatService::OnStateChanged() {
  mojom::ServiceStatePtr state = BuildState();
  for (auto& remote : observer_remotes_) {
    remote->OnStateChanged(state.Clone());
  }
}

bool AIChatService::IsAIChatHistoryEnabled() {
  return (features::IsAIChatHistoryEnabled() &&
          profile_prefs_->GetBoolean(prefs::kBraveChatStorageEnabled));
}

void AIChatService::OnRequestInProgressChanged(ConversationHandler* handler,
                                               bool in_progress) {
  if (ai_chat_metrics_) {
    ai_chat_metrics_->MaybeRecordLastError(handler);
  }
  // We don't unload a conversation if it has a request in progress, so check
  // again when that changes.
  if (!in_progress) {
    QueueMaybeUnloadConversation(handler);
  }
}

void AIChatService::OnConversationEntryAdded(
    ConversationHandler* handler,
    mojom::ConversationTurnPtr& entry,
    std::optional<PageContents> maybe_associated_content) {
  auto conversation_it = conversations_.find(handler->get_conversation_uuid());
  CHECK(conversation_it != conversations_.end());
  mojom::ConversationPtr& conversation = conversation_it->second;
  std::optional<std::vector<std::string>> associated_content;
  if (maybe_associated_content.has_value()) {
    associated_content = std::vector<std::string>();
    std::ranges::transform(
        maybe_associated_content.value(),
        std::back_inserter(associated_content.value()),
        [](const auto& page_content) { return page_content.get().content; });
  }

  if (!conversation->has_content) {
    HandleFirstEntry(handler, entry, std::move(associated_content),
                     conversation);
  } else {
    HandleNewEntry(handler, entry, std::move(associated_content), conversation);
  }

  conversation->has_content = true;
  conversation->updated_time = entry->created_time;
  OnConversationListChanged();
}

void AIChatService::HandleFirstEntry(
    ConversationHandler* handler,
    mojom::ConversationTurnPtr& entry,
    std::optional<std::vector<std::string>> maybe_associated_content,
    mojom::ConversationPtr& conversation) {
  DVLOG(1) << __func__ << " Conversation " << conversation->uuid
           << " being persisted for first time.";
  CHECK(entry->uuid.has_value());

  std::vector<std::string> associated_content;
  if (maybe_associated_content.has_value()) {
    associated_content = std::move(maybe_associated_content.value());
  }

  // We can persist the conversation metadata for the first time as well as the
  // entry.
  if (ai_chat_db_ && !conversation->temporary) {
    ai_chat_db_.AsyncCall(base::IgnoreResult(&AIChatDatabase::AddConversation))
        .WithArgs(conversation->Clone(), std::move(associated_content),
                  entry->Clone());
  }
  // Record metrics
  if (ai_chat_metrics_ != nullptr) {
    if (handler->GetConversationHistory().size() == 1) {
      ai_chat_metrics_->RecordNewPrompt(handler, conversation, entry);
    }
  }
}

void AIChatService::HandleNewEntry(
    ConversationHandler* handler,
    mojom::ConversationTurnPtr& entry,
    std::optional<std::vector<std::string>> maybe_associated_content,
    mojom::ConversationPtr& conversation) {
  CHECK(entry->uuid.has_value());
  DVLOG(1) << __func__ << " Conversation " << conversation->uuid
           << " persisting new entry. Count of entries: "
           << handler->GetConversationHistory().size();

  // Persist the new entry and update the associated content data, if present
  if (ai_chat_db_ && !conversation->temporary) {
    ai_chat_db_
        .AsyncCall(base::IgnoreResult(&AIChatDatabase::AddConversationEntry))
        .WithArgs(handler->get_conversation_uuid(), entry.Clone(),
                  std::nullopt);

    // update the model name if it changed for this entry
    ai_chat_db_
        .AsyncCall(
            base::IgnoreResult(&AIChatDatabase::UpdateConversationModelKey))
        .WithArgs(handler->get_conversation_uuid(), conversation->model_key);

    if (maybe_associated_content.has_value() &&
        !conversation->associated_content.empty()) {
      ai_chat_db_
          .AsyncCall(
              base::IgnoreResult(&AIChatDatabase::AddOrUpdateAssociatedContent))
          .WithArgs(conversation->uuid,
                    CloneAssociatedContent(conversation->associated_content),
                    std::move(maybe_associated_content.value()));
    }
  }

  // Record metrics
  if (ai_chat_metrics_ != nullptr &&
      entry->character_type == mojom::CharacterType::HUMAN) {
    ai_chat_metrics_->RecordNewPrompt(handler, conversation, entry);
  }
}

void AIChatService::OnConversationEntryRemoved(ConversationHandler* handler,
                                               std::string entry_uuid) {
  // Persist the removal
  if (ai_chat_db_ && !handler->GetIsTemporary()) {
    ai_chat_db_
        .AsyncCall(base::IgnoreResult(&AIChatDatabase::DeleteConversationEntry))
        .WithArgs(entry_uuid);
  }
}

void AIChatService::OnToolUseEventOutput(ConversationHandler* handler,
                                         std::string_view entry_uuid,
                                         size_t event_order,
                                         mojom::ToolUseEventPtr tool_use) {
  // Persist the tool use event
  if (ai_chat_db_ && !handler->GetIsTemporary()) {
    ai_chat_db_
        .AsyncCall(base::IgnoreResult(&AIChatDatabase::UpdateToolUseEvent))
        .WithArgs(entry_uuid, event_order, std::move(tool_use));
  }
}

void AIChatService::OnClientConnectionChanged(ConversationHandler* handler) {
  DVLOG(4) << "Client connection changed for conversation "
           << handler->get_conversation_uuid();
  if (ai_chat_metrics_ != nullptr && !handler->IsAnyClientConnected()) {
    ai_chat_metrics_->RecordConversationUnload(
        handler->get_conversation_uuid());
  }
  QueueMaybeUnloadConversation(handler);
}

void AIChatService::OnConversationTitleChanged(
    const std::string& conversation_uuid,
    const std::string& new_title) {
  auto conversation_it = conversations_.find(conversation_uuid);
  if (conversation_it == conversations_.end()) {
    DLOG(ERROR) << "Conversation not found for title change";
    return;
  }

  auto& conversation_metadata = conversation_it->second;
  conversation_metadata->title = new_title;

  OnConversationListChanged();

  // Persist the change
  if (ai_chat_db_ && !conversation_metadata->temporary) {
    ai_chat_db_
        .AsyncCall(base::IgnoreResult(&AIChatDatabase::UpdateConversationTitle))
        .WithArgs(conversation_uuid, new_title);
  }
}

void AIChatService::OnConversationTokenInfoChanged(
    const std::string& conversation_uuid,
    uint64_t total_tokens,
    uint64_t trimmed_tokens) {
  auto conversation_it = conversations_.find(conversation_uuid);
  if (conversation_it == conversations_.end()) {
    DLOG(ERROR) << "Conversation not found for token info change";
    return;
  }

  auto& conversation_metadata = conversation_it->second;
  conversation_metadata->total_tokens = total_tokens;
  conversation_metadata->trimmed_tokens = trimmed_tokens;

  OnConversationListChanged();

  // Persist the change
  if (ai_chat_db_ && !conversation_metadata->temporary) {
    ai_chat_db_
        .AsyncCall(
            base::IgnoreResult(&AIChatDatabase::UpdateConversationTokenInfo))
        .WithArgs(conversation_uuid, total_tokens, trimmed_tokens);
  }
}

void AIChatService::OnAssociatedContentUpdated(ConversationHandler* handler) {
  if (handler->associated_content_manager()->HasAssociatedContent()) {
    return;
  }
  QueueMaybeUnloadConversation(handler);
}

void AIChatService::GetConversations(GetConversationsCallback callback) {
  LoadConversationsLazy(base::BindOnce(
      [](GetConversationsCallback callback,
         ConversationMap& conversations_map) {
        std::vector<mojom::ConversationPtr> conversations;
        for (const auto& conversation :
             GetConversationsSortedByUpdatedTime(conversations_map)) {
          conversations.push_back(conversation->Clone());
        }
        std::move(callback).Run(std::move(conversations));
      },
      std::move(callback)));
}

void AIChatService::BindConversation(
    const std::string& uuid,
    mojo::PendingReceiver<mojom::ConversationHandler> receiver,
    mojo::PendingRemote<mojom::ConversationUI> conversation_ui_handler) {
  GetConversation(
      std::move(uuid),
      base::BindOnce(
          [](mojo::PendingReceiver<mojom::ConversationHandler> receiver,
             mojo::PendingRemote<mojom::ConversationUI> conversation_ui_handler,
             ConversationHandler* handler) {
            if (!handler) {
              DVLOG(0) << "Failed to get conversation for binding";
              return;
            }
            handler->Bind(std::move(receiver),
                          std::move(conversation_ui_handler));
          },
          std::move(receiver), std::move(conversation_ui_handler)));
}

void AIChatService::BindMetrics(mojo::PendingReceiver<mojom::Metrics> metrics) {
  if (ai_chat_metrics_) {
    ai_chat_metrics_->Bind(std::move(metrics));
  }
}

void AIChatService::BindObserver(
    mojo::PendingRemote<mojom::ServiceObserver> observer,
    BindObserverCallback callback) {
  observer_remotes_.Add(std::move(observer));
  std::move(callback).Run(BuildState());
}

bool AIChatService::HasUserOptedIn() {
  return ai_chat::HasUserOptedIn(profile_prefs_);
}

bool AIChatService::IsPremiumStatus() {
  return ai_chat::IsPremiumStatus(last_premium_status_);
}

std::unique_ptr<EngineConsumer> AIChatService::GetDefaultAIEngine() {
  return GetEngineForModel(model_service_->GetDefaultModelKey());
}

std::unique_ptr<EngineConsumer> AIChatService::GetEngineForModel(
    const std::string& model_key) {
  return model_service_->GetEngineForModel(model_key, url_loader_factory_,
                                           credential_manager_.get());
}

size_t AIChatService::GetInMemoryConversationCountForTesting() {
  return conversation_handlers_.size();
}

void AIChatService::OnUserOptedIn() {
  OnStateChanged();
  bool is_opted_in = HasUserOptedIn();
  if (!is_opted_in) {
    return;
  }
  for (auto& kv : conversation_handlers_) {
    kv.second->OnUserOptedIn();
  }
  if (ai_chat_metrics_ != nullptr) {
    ai_chat_metrics_->RecordEnabled(true, true, {});
  }
}

void AIChatService::OnConversationListChanged() {
  auto conversations = GetConversationsSortedByUpdatedTime(conversations_);
  for (auto& remote : observer_remotes_) {
    std::vector<mojom::ConversationPtr> client_conversations;
    for (const auto& conversation : conversations) {
      client_conversations.push_back(conversation->Clone());
    }
    remote->OnConversationListChanged(std::move(client_conversations));
  }
}

void AIChatService::OpenConversationWithStagedEntries(
    base::WeakPtr<AssociatedContentDelegate> associated_content,
    base::OnceClosure open_ai_chat) {
  if (!associated_content || !associated_content->HasOpenAIChatPermission()) {
    return;
  }

  ConversationHandler* conversation = GetOrCreateConversationHandlerForContent(
      associated_content->content_id(), associated_content);
  CHECK(conversation);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (ai_chat_metrics_ != nullptr) {
    ai_chat_metrics_->HandleOpenViaEntryPoint(EntryPoint::kBraveSearch);
  }
#endif
  // Open AI Chat and trigger a fetch of staged conversations from Brave Search.
  std::move(open_ai_chat).Run();
  conversation->MaybeFetchOrClearContentStagedConversation();
}

void AIChatService::MaybeAssociateContent(
    AssociatedContentDelegate* content,
    const std::string& conversation_uuid) {
  CHECK(content);

  auto* conversation = GetConversation(conversation_uuid);
  if (!conversation) {
    return;
  }

  MaybeAssociateContent(conversation, content->content_id(),
                        content->GetWeakPtr());
}

void AIChatService::DisassociateContent(
    const mojom::AssociatedContentPtr& content,
    const std::string& conversation_uuid) {
  // Note: This will only work if the conversation is already loaded.
  auto* conversation = GetConversation(conversation_uuid);
  if (!conversation) {
    return;
  }
  conversation->associated_content_manager()->RemoveContent(content->uuid);

  // If this conversation is the most recent one for the content, remove it from
  // content_conversations_.
  if (content_conversations_[content->content_id] == conversation_uuid) {
    content_conversations_.erase(content->content_id);
  }
}

void AIChatService::GetSuggestedTopics(const std::vector<Tab>& tabs,
                                       GetSuggestedTopicsCallback callback) {
  if (!cached_focus_topics_.empty()) {
    std::move(callback).Run(cached_focus_topics_);
    return;
  }

  // First time engaging with tab focus, set up tab data observer.
  // tab_tracker_service_ can be nullptr in tests.
  if (tab_tracker_service_ && !tab_data_observer_receiver_.is_bound()) {
    tab_tracker_service_->AddObserver(
        tab_data_observer_receiver_.BindNewPipeAndPassRemote());
  }

  GetEngineForTabOrganization(base::BindOnce(
      &AIChatService::GetSuggestedTopicsWithEngine,
      weak_ptr_factory_.GetWeakPtr(), tabs, std::move(callback)));
}

void AIChatService::GetFocusTabs(const std::vector<Tab>& tabs,
                                 const std::string& topic,
                                 GetFocusTabsCallback callback) {
  GetEngineForTabOrganization(base::BindOnce(
      &AIChatService::GetFocusTabsWithEngine, weak_ptr_factory_.GetWeakPtr(),
      tabs, topic,
      base::BindOnce(&AIChatService::OnGetFocusTabs,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void AIChatService::OnGetFocusTabs(
    GetFocusTabsCallback callback,
    base::expected<std::vector<std::string>, mojom::APIError> result) {
  if (ai_chat_metrics_ && result.has_value() && !result->empty()) {
    ai_chat_metrics_->tab_focus_metrics()->RecordUsage(result.value().size());
  }
  std::move(callback).Run(std::move(result));
}

void AIChatService::GetEngineForTabOrganization(base::OnceClosure callback) {
  GetPremiumStatus(
      base::BindOnce(&AIChatService::ContinueGetEngineForTabOrganization,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AIChatService::ContinueGetEngineForTabOrganization(
    base::OnceClosure callback,
    mojom::PremiumStatus status,
    mojom::PremiumInfoPtr info) {
  bool is_premium = IsPremiumStatus();
  if (tab_organization_engine_) {
    // Check if model name matches the current premium status.
    if ((is_premium &&
         tab_organization_engine_->GetModelName() != kClaudeSonnetModelName) ||
        (!is_premium &&
         tab_organization_engine_->GetModelName() != kClaudeHaikuModelName)) {
      tab_organization_engine_.reset();
    }
  }

  if (!tab_organization_engine_) {
    tab_organization_engine_ = GetEngineForModel(
        is_premium ? kClaudeSonnetModelKey : kClaudeHaikuModelKey);
  }

  std::move(callback).Run();
}

void AIChatService::GetSuggestedTopicsWithEngine(
    const std::vector<Tab>& tabs,
    GetSuggestedTopicsCallback callback) {
  CHECK(tab_organization_engine_);
  auto internal_callback =
      base::BindOnce(&AIChatService::OnSuggestedTopicsReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  tab_organization_engine_->GetSuggestedTopics(tabs,
                                               std::move(internal_callback));
}

void AIChatService::OnSuggestedTopicsReceived(
    GetSuggestedTopicsCallback callback,
    base::expected<std::vector<std::string>, mojom::APIError> topics) {
  if (tab_data_observer_receiver_.is_bound() && topics.has_value()) {
    cached_focus_topics_ = topics.value();
  }

  std::move(callback).Run(std::move(topics));
}

void AIChatService::TabDataChanged(std::vector<mojom::TabDataPtr> tab_data) {
  cached_focus_topics_.clear();
}

void AIChatService::GetFocusTabsWithEngine(const std::vector<Tab>& tabs,
                                           const std::string& topic,
                                           GetFocusTabsCallback callback) {
  CHECK(tab_organization_engine_);
  tab_organization_engine_->GetFocusTabs(tabs, topic, std::move(callback));
}

}  // namespace ai_chat
