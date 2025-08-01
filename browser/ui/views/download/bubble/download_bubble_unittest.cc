/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "base/test/scoped_feature_list.h"
#include "brave/browser/download/brave_download_commands.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/ui/download/download_bubble_info_utils.h"
#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"

using download::DownloadItem;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

namespace {

// Default target path for a mock download item in DownloadItemModelTest.
const base::FilePath::CharType kDefaultTargetFilePath[] =
    FILE_PATH_LITERAL("/foo/bar/foo.bar");

const base::FilePath::CharType kDefaultDisplayFileName[] =
    FILE_PATH_LITERAL("foo.bar");

// Default URL for a mock download item in DownloadItemModelTest.
constexpr char kDefaultURL[] = "http://example.com/foo.bar";
}  // namespace

class DownloadBubbleTest : public testing::Test {
 public:
  DownloadBubbleTest()
      : model_(&item_),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  ~DownloadBubbleTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    testing_profile_manager_.CreateTestingProfile("testing_profile");
  }

  // Sets up defaults for the download item and sets |model_| to a new
  // DownloadItemModel that uses the mock download item.
  void SetupDownloadItemDefaults() {
    ON_CALL(item_, GetReceivedBytes()).WillByDefault(Return(1));
    ON_CALL(item_, GetTotalBytes()).WillByDefault(Return(2));
    ON_CALL(item_, TimeRemaining(_)).WillByDefault(Return(false));
    ON_CALL(item_, GetMimeType()).WillByDefault(Return("text/html"));
    ON_CALL(item_, AllDataSaved()).WillByDefault(Return(false));
    ON_CALL(item_, GetOpenWhenComplete()).WillByDefault(Return(false));
    ON_CALL(item_, GetFileExternallyRemoved()).WillByDefault(Return(false));
    ON_CALL(item_, GetURL()).WillByDefault(ReturnRefOfCopy(GURL(kDefaultURL)));
    ON_CALL(item_, GetFileNameToReportUser())
        .WillByDefault(Return(base::FilePath(kDefaultDisplayFileName)));
    ON_CALL(item_, GetTargetFilePath())
        .WillByDefault(ReturnRefOfCopy(base::FilePath(kDefaultTargetFilePath)));
    ON_CALL(item_, GetFullPath())
        .WillByDefault(ReturnRefOfCopy(base::FilePath(kDefaultTargetFilePath)));
    ON_CALL(item_, GetTargetDisposition())
        .WillByDefault(Return(DownloadItem::TARGET_DISPOSITION_OVERWRITE));
    ON_CALL(item_, IsPaused()).WillByDefault(Return(false));
    ON_CALL(item_, CanResume()).WillByDefault(Return(false));
    ON_CALL(item_, GetInsecureDownloadStatus())
        .WillByDefault(
            Return(download::DownloadItem::InsecureDownloadStatus::SAFE));
    ON_CALL(item_, GetDangerType())
        .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  }

  void SetupCompletedDownloadItem() {
    EXPECT_CALL(item_, GetState())
        .WillRepeatedly(Return(DownloadItem::COMPLETE));
  }

  void SetupInProgressDownloadItem() {
    EXPECT_CALL(item_, GetState())
        .WillRepeatedly(Return(DownloadItem::IN_PROGRESS));
  }

  void SetupCancelledDownloadItem() {
    EXPECT_CALL(item_, GetState())
        .WillRepeatedly(Return(DownloadItem::CANCELLED));
  }

  bool ContainsDeleteLocalFileCommand() {
    auto quick_actions = QuickActionsForDownload(model_);
    return std::ranges::find(
               quick_actions, BraveDownloadCommands::DELETE_LOCAL_FILE,
               &DownloadBubbleQuickAction::command) != quick_actions.end();
  }

  content::BrowserTaskEnvironment task_environment_;
  NiceMock<download::MockDownloadItem> item_;
  DownloadItemModel model_;
  TestingProfileManager testing_profile_manager_;
};

TEST_F(DownloadBubbleTest, ContextMenuCompletedItemTest) {
  SetupDownloadItemDefaults();
  SetupCompletedDownloadItem();

  // Check complete item has remove from list menu entry;
  DownloadShelfContextMenuView ctx_menu(model_.GetWeakPtr());
  auto* menu_model = ctx_menu.GetMenuModel();
  EXPECT_TRUE(
      menu_model->GetIndexOfCommandId(BraveDownloadCommands::REMOVE_FROM_LIST));
}

TEST_F(DownloadBubbleTest, ContextMenuInProgressItemTest) {
  SetupDownloadItemDefaults();
  SetupInProgressDownloadItem();

  // Check in-progress item doesn't have remove from list menu entry;
  DownloadShelfContextMenuView ctx_menu(model_.GetWeakPtr());
  auto* menu_model = ctx_menu.GetMenuModel();
  EXPECT_FALSE(
      menu_model->GetIndexOfCommandId(BraveDownloadCommands::REMOVE_FROM_LIST));
}

TEST_F(DownloadBubbleTest, ContextMenuCancelledItemTest) {
  SetupDownloadItemDefaults();
  SetupCancelledDownloadItem();

  // Check cancelled item has remove from list menu entry;
  DownloadShelfContextMenuView ctx_menu(model_.GetWeakPtr());
  auto* menu_model = ctx_menu.GetMenuModel();
  EXPECT_TRUE(
      menu_model->GetIndexOfCommandId(BraveDownloadCommands::REMOVE_FROM_LIST));
}

TEST_F(DownloadBubbleTest, DeleteLocalFileCommand_Incomplete) {
  SetupDownloadItemDefaults();

  // When download isn't complete, QuickActionsForDownload should not contain
  // kDeleteLocalFile command.
  ASSERT_NE(model_.GetState(), DownloadItem::COMPLETE);
  EXPECT_FALSE(ContainsDeleteLocalFileCommand());
}

TEST_F(DownloadBubbleTest, DeleteLocalFileCommand_Cancelled) {
  SetupDownloadItemDefaults();
  SetupCancelledDownloadItem();

  // When download is cancelled, QuickActionsForDownload should not contain
  // kDeleteLocalFile command.
  ASSERT_EQ(model_.GetState(), DownloadItem::CANCELLED);
  EXPECT_FALSE(ContainsDeleteLocalFileCommand());
}

TEST_F(DownloadBubbleTest, DeleteLocalFileCommand_Complete) {
  SetupDownloadItemDefaults();
  SetupCompletedDownloadItem();

  // When download is complete, QuickActionsForDownload should contain
  // kDeleteLocalFile command.
  ASSERT_EQ(model_.GetState(), DownloadItem::COMPLETE);
  EXPECT_TRUE(ContainsDeleteLocalFileCommand());
}

TEST_F(DownloadBubbleTest, BraveDownloadCommands_DeleteLocalFileEnabled) {
  SetupDownloadItemDefaults();
  SetupCompletedDownloadItem();

  // When download is complete and has a full path, DeleteLocalFile command
  // should be enabled.
  ASSERT_EQ(model_.GetState(), DownloadItem::COMPLETE);
  ASSERT_FALSE(model_.GetFileExternallyRemoved());
  ASSERT_FALSE(model_.GetFullPath().empty());

  BraveDownloadCommands commands(model_.GetWeakPtr());
  EXPECT_TRUE(
      commands.IsCommandEnabled(BraveDownloadCommands::DELETE_LOCAL_FILE));
}

TEST_F(DownloadBubbleTest,
       BraveDownloadCommands_DeleteLocalFileDisabledWhenFullPathEmpty) {
  SetupDownloadItemDefaults();
  SetupCompletedDownloadItem();

  // When download is complete but has no full path, DeleteLocalFile command
  // should be disabled.
  ASSERT_EQ(model_.GetState(), DownloadItem::COMPLETE);
  ASSERT_FALSE(model_.GetFileExternallyRemoved());
  EXPECT_CALL(item_, GetFullPath()).WillOnce(ReturnRefOfCopy(base::FilePath()));

  BraveDownloadCommands commands(model_.GetWeakPtr());
  EXPECT_FALSE(
      commands.IsCommandEnabled(BraveDownloadCommands::DELETE_LOCAL_FILE));
}

TEST_F(DownloadBubbleTest,
       BraveDownloadCommands_DeleteLocalFileDisabledWhenExternallyRemoved) {
  SetupDownloadItemDefaults();
  SetupCompletedDownloadItem();

  // When download is complete but has been externally removed, DeleteLocalFile
  ASSERT_EQ(model_.GetState(), DownloadItem::COMPLETE);
  ASSERT_FALSE(item_.GetFullPath().empty());
  EXPECT_CALL(item_, GetFileExternallyRemoved()).WillOnce(Return(true));

  BraveDownloadCommands commands(model_.GetWeakPtr());
  EXPECT_FALSE(
      commands.IsCommandEnabled(BraveDownloadCommands::DELETE_LOCAL_FILE));
}
