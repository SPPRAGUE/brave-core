// Copyright (c) 2022 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at http://mozilla.org/MPL/2.0/.

#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "brave/browser/ui/views/side_panel/playlist/playlist_side_panel_coordinator.h"

#define PopulateGlobalEntries PopulateGlobalEntries_ChromiumImpl
#include <chrome/browser/ui/views/side_panel/side_panel_util.cc>
#undef PopulateGlobalEntries

// static
void SidePanelUtil::PopulateGlobalEntries(Browser* browser,
                                          SidePanelRegistry* global_registry) {
  PopulateGlobalEntries_ChromiumImpl(browser, global_registry);

  // the playlist coordinator is not created for popup windows, or for
  // desktop PWAs.
  if (auto* playlist_coordinator =
          browser->GetFeatures().playlist_side_panel_coordinator()) {
    playlist_coordinator->CreateAndRegisterEntry(global_registry);
  }
}
