/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"

#include <utility>

#include "components/sessions/content/session_tab_helper.h"

// The bubble delegate doesn't allow to open popups and we use
// the Browser window delegate to redirect opening new popup content
// to the Browser delegate instead of default one.
// In order to close all popups we also save tab ids of each opened popup window
// and close all with the bubble together.

#define AddNewContents AddNewContents_ChromiumImpl
#include <chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.cc>
#undef AddNewContents

void WebUIContentsWrapper::SetWebContentsAddNewContentsDelegate(
    base::WeakPtr<content::WebContentsDelegate> browser_delegate) {
  browser_delegate_ = std::move(browser_delegate);
}

void WebUIContentsWrapper::ClearPopupIds() {
  popup_ids_.clear();
}

content::WebContents* WebUIContentsWrapper::Host::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  return WebUIContentsWrapper::Host::AddNewContents_ChromiumImpl(
      source, std::move(new_contents), target_url, disposition, window_features,
      user_gesture, was_blocked);
}

content::WebContents* WebUIContentsWrapper::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  if (!browser_delegate_) {
    return host_ ? host_->AddNewContents(
                       source, std::move(new_contents), target_url, disposition,
                       window_features, user_gesture, was_blocked)
                 : nullptr;
  }
  auto* raw_popup_contents = new_contents.get();
  auto* contents = browser_delegate_->AddNewContents(
      source, std::move(new_contents), target_url,
      WindowOpenDisposition::NEW_POPUP, window_features, user_gesture,
      was_blocked);
  if (!contents) {
    return nullptr;
  }
  auto tab_id = sessions::SessionTabHelper::IdForTab(raw_popup_contents).id();
  popup_ids_.push_back(tab_id);
  return contents;
}
