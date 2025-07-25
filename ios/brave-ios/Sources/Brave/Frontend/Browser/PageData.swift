// Copyright 2022 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

import BraveCore
import BraveShared
import BraveShields
import Data
import Foundation
import Shared
import WebKit

/// The data for the current web-page which is needed for loading and executing privacy scripts
///
/// Since frames may be loaded as the user scrolls which may need additional scripts to be injected,
/// We cache information about frames in order to prevent excessive reloading of scripts.
@MainActor struct PageData {
  /// The url of the page (i.e. main frame)
  private(set) var mainFrameURL: URL
  /// A list of all currently available subframes for this current page
  /// These are loaded dyncamically as the user scrolls through the page
  private(set) var allSubframeURLs: Set<URL> = []
  /// The stats class to get the engine data from
  private var groupsManager: AdBlockGroupsManager

  init(mainFrameURL: URL, groupsManager: AdBlockGroupsManager = AdBlockGroupsManager.shared) {
    self.mainFrameURL = mainFrameURL
    self.groupsManager = groupsManager
  }

  /// This method builds all the user scripts that should be included for this page
  @MainActor mutating func addSubframeURL(forRequestURL requestURL: URL, isForMainFrame: Bool) {
    if !isForMainFrame {
      // We need to add any non-main frame urls to our site data
      // We will need this to construct all non-main frame scripts
      allSubframeURLs.insert(requestURL)
    }
  }

  /// A new list of scripts is returned only if a change is detected in the response (for example an HTTPs upgrade).
  /// In some cases (like during an https upgrade) the scripts may change on the response. So we need to update the user scripts
  @MainActor mutating func upgradeFrameURL(
    forResponseURL responseURL: URL,
    isForMainFrame: Bool
  ) -> Bool {
    if isForMainFrame {
      // If it's the main frame url that was upgraded,
      // we need to update it and rebuild the types
      guard mainFrameURL != responseURL else { return false }
      mainFrameURL = responseURL
      return true
    } else if !allSubframeURLs.contains(responseURL) {
      // first try to remove the old unwanted `http` frame URL
      if var components = URLComponents(url: responseURL, resolvingAgainstBaseURL: false),
        components.scheme == "https"
      {
        components.scheme = "http"
        if let downgradedURL = components.url {
          allSubframeURLs.remove(downgradedURL)
        }
      }

      // Now add the new subframe url
      allSubframeURLs.insert(responseURL)
      return true
    } else {
      // Nothing changed. Return nil
      return false
    }
  }

  /// Return the domain for this current page passing any options needed for its persistance
  @MainActor func domain(persistent: Bool) -> Domain {
    return Domain.getOrCreate(forUrl: mainFrameURL, persistent: persistent)
  }

  /// Return all the user script types for this page. The number of script types grows as more frames are loaded.
  @MainActor func makeUserScriptTypes(
    isDeAmpEnabled: Bool,
    isAdBlockEnabled: Bool,
    isBlockFingerprintingEnabled: Bool
  ) async -> Set<UserScriptType> {
    if !mainFrameURL.isWebPage(includeDataURIs: false) {
      // Local application urls do not need page-specific user scripts injected
      return []
    }

    var userScriptTypes: Set<UserScriptType> = [
      .siteStateListener, .gpc(ShieldPreferences.enableGPC.value),
    ]

    // Handle dynamic domain level scripts on the main document.
    // These are scripts that change depending on the domain and the main document

    // Add the `farblingProtection` script if needed
    // Note: The added farbling protection script based on the document url, not the frame's url.
    // It is also added for every frame, including subframes.
    if isBlockFingerprintingEnabled, let baseDomain = mainFrameURL.baseDomain {
      userScriptTypes.insert(.nacl)  // dependency for `farblingProtection`
      userScriptTypes.insert(.farblingProtection(etld: baseDomain))
    }

    // Handle dynamic domain level scripts on the request that don't use shields
    // This shield is always on and doesn't need sheild settings
    if let domainUserScript = DomainUserScript(for: mainFrameURL) {
      userScriptTypes.insert(.domainUserScript(domainUserScript))
    }

    let allEngineScriptTypes = await makeAllEngineScripts(
      isDeAmpEnabled: isDeAmpEnabled,
      isAdBlockEnabled: isAdBlockEnabled
    )
    return userScriptTypes.union(allEngineScriptTypes)
  }

  func makeMainFrameEngineScriptTypes(
    isDeAmpEnabled: Bool,
    isAdBlockEnabled: Bool
  ) async -> Set<UserScriptType> {
    return await groupsManager.makeEngineScriptTypes(
      frameURL: mainFrameURL,
      isMainFrame: true,
      isDeAmpEnabled: isDeAmpEnabled,
      isAdBlockEnabled: isAdBlockEnabled
    )
  }

  func makeAllEngineScripts(
    isDeAmpEnabled: Bool,
    isAdBlockEnabled: Bool
  ) async -> Set<UserScriptType> {
    // Add engine scripts for the main frame
    async let engineScripts = groupsManager.makeEngineScriptTypes(
      frameURL: mainFrameURL,
      isMainFrame: true,
      isDeAmpEnabled: isDeAmpEnabled,
      isAdBlockEnabled: isAdBlockEnabled
    )

    // Add engine scripts for all of the known sub-frames
    async let additionalScriptTypes = allSubframeURLs.asyncConcurrentCompactMap({ frameURL in
      return await self.groupsManager.makeEngineScriptTypes(
        frameURL: frameURL,
        isMainFrame: false,
        isDeAmpEnabled: isDeAmpEnabled,
        isAdBlockEnabled: isAdBlockEnabled
      )
    }).reduce(
      Set<UserScriptType>(),
      { partialResult, scriptTypes in
        return partialResult.union(scriptTypes)
      }
    )

    let allEngineScripts = await (mainFrame: engineScripts, subFrames: additionalScriptTypes)
    return allEngineScripts.mainFrame.union(allEngineScripts.subFrames)
  }
}
