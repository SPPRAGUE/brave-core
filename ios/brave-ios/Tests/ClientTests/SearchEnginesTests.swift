// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

import Foundation
import Preferences
import Shared
import XCTest

@testable import Brave

class SearchEnginesTests: XCTestCase {

  private let defaultSearchEngineName = "Google"
  // BRAVE TODO: This list is not accurate because Brave uses many more engines
  private let expectedEngineNames = ["Qwant", "Bing", "DuckDuckGo", "Google", "StartPage"]

  override func setUp() {
    super.setUp()

    Preferences.Search.defaultEngineName.reset()
    Preferences.Search.defaultPrivateEngineName.reset()
    Preferences.Search.disabledEngines.reset()
    Preferences.Search.orderedEngines.reset()
    Preferences.Search.showSuggestions.reset()

    Preferences.Search.defaultEngineName.reset()
    Preferences.Search.defaultPrivateEngineName.reset()
  }

  func testIncludesExpectedEngines() async {
    // Verify that the set of shipped engines includes the expected subset.
    let engines = SearchEngines()
    await engines.loadSearchEngines()
    let orderedEngines = engines.orderedEngines
    XCTAssertTrue(orderedEngines.count >= expectedEngineNames.count)

    for engineName in expectedEngineNames {
      XCTAssertTrue(orderedEngines.filter { engine in engine.shortName == engineName }.count > 0)
    }
  }

  func testDefaultEngineOnStartup() async throws {
    // If this is our first run, Google should be first for the en locale.
    let engines = SearchEngines(locale: Locale(identifier: "pl_PL"))
    await engines.loadSearchEngines()
    XCTAssertEqual(
      try XCTUnwrap(engines.defaultEngine(forType: .standard)).shortName,
      defaultSearchEngineName
    )
    // The default is `DefaultSearchEngineName` for both regular and private browsing.
    // Different search engine options might apply to certain regions.
    // Default locale for running tests should be en_US.
    XCTAssertEqual(
      try XCTUnwrap(engines.defaultEngine(forType: .privateMode)).shortName,
      defaultSearchEngineName
    )

    let orderedEngines = engines.orderedEngines.compactMap { $0.shortName }
    XCTAssert(orderedEngines.contains(defaultSearchEngineName))
  }

  func testAddingAndDeletingCustomEngines() async throws {
    let testEngine = OpenSearchEngine(
      engineID: "ATester",
      shortName: "ATester",
      image: UIImage(),
      searchTemplate: "http://firefox.com/find?q={searchTerm}",
      suggestTemplate: nil,
      isCustomEngine: true
    )
    let engines = SearchEngines()
    await engines.loadSearchEngines()
    try await engines.addSearchEngine(testEngine)

    XCTAssertEqual(engines.orderedEngines[1].engineID, testEngine.engineID)

    try await engines.deleteCustomEngine(testEngine)
    let deleted = engines.orderedEngines.filter { $0 == testEngine }
    XCTAssertEqual(deleted, [])
  }

  func testDefaultEngine() async {
    let engines = SearchEngines()
    await engines.loadSearchEngines()
    let engineSet = engines.orderedEngines

    engines.updateDefaultEngine(engineSet[0].shortName, forType: .standard)
    XCTAssertTrue(engines.isEngineDefault(engineSet[0], type: .standard))
    XCTAssertFalse(engines.isEngineDefault(engineSet[1], type: .standard))
    // The first ordered engine is the default.
    XCTAssertEqual(engines.orderedEngines[0].shortName, engineSet[0].shortName)

    engines.updateDefaultEngine(engineSet[1].shortName, forType: .standard)
    XCTAssertFalse(engines.isEngineDefault(engineSet[0], type: .standard))
    XCTAssertTrue(engines.isEngineDefault(engineSet[1], type: .standard))
    // The first ordered engine is the default.
    XCTAssertEqual(engines.orderedEngines[0].shortName, engineSet[1].shortName)

    let engines2 = SearchEngines()
    await engines2.loadSearchEngines()
    // The default engine should have been persisted.
    XCTAssertTrue(engines2.isEngineDefault(engineSet[1], type: .standard))
    // The first ordered engine is the default.
    XCTAssertEqual(engines.orderedEngines[0].shortName, engineSet[1].shortName)
  }

  func testSetPrivateDefaultEngine() async {
    let engines = SearchEngines()
    await engines.loadSearchEngines()
    let engineSet = engines.orderedEngines

    let firstEngine = engineSet[0]
    let secondEngine = engineSet[1]

    engines.updateDefaultEngine(firstEngine.shortName, forType: .standard)
    XCTAssertTrue(engines.isEngineDefault(firstEngine, type: .privateMode))
    XCTAssertFalse(engines.isEngineDefault(secondEngine, type: .privateMode))
  }

  func testOrderedEngines() async {
    let engines = SearchEngines()
    await engines.loadSearchEngines()

    engines.orderedEngines = [
      expectedEngineNames[4], expectedEngineNames[2], expectedEngineNames[0],
    ].map { name in
      for engine in engines.orderedEngines {
        if engine.shortName == name {
          return engine
        }
      }
      XCTFail("Could not find engine: \(name)")
      return engines.orderedEngines.first!
    }
    XCTAssertEqual(engines.orderedEngines[0].shortName, expectedEngineNames[4])
    XCTAssertEqual(engines.orderedEngines[1].shortName, expectedEngineNames[2])
    XCTAssertEqual(engines.orderedEngines[2].shortName, expectedEngineNames[0])

    let engines2 = SearchEngines()
    await engines2.loadSearchEngines()
    // The ordering should have been persisted.
    XCTAssertEqual(engines2.orderedEngines[0].shortName, expectedEngineNames[4])
    XCTAssertEqual(engines2.orderedEngines[1].shortName, expectedEngineNames[2])
    XCTAssertEqual(engines2.orderedEngines[2].shortName, expectedEngineNames[0])
  }

  func testQuickSearchEngines() async {
    let engines = SearchEngines()
    await engines.loadSearchEngines()
    let engineSet = engines.orderedEngines

    // You can't disable the default engine.
    engines.updateDefaultEngine(engineSet[1].shortName, forType: .standard)
    engines.disableEngine(engineSet[1], type: .standard)
    XCTAssertTrue(engines.isEngineEnabled(engineSet[1]))

    // The default engine is included in the quick search engines.
    XCTAssertEqual(
      1,
      engines.quickSearchEngines.filter { engine in engine.shortName == engineSet[0].shortName }
        .count
    )

    // Enable and disable work.
    engines.enableEngine(engineSet[0])
    XCTAssertTrue(engines.isEngineEnabled(engineSet[0]))
    XCTAssertEqual(
      1,
      engines.quickSearchEngines.filter { engine in engine.shortName == engineSet[0].shortName }
        .count
    )

    engines.disableEngine(engineSet[0], type: .standard)
    XCTAssertFalse(engines.isEngineEnabled(engineSet[0]))
    XCTAssertEqual(
      0,
      engines.quickSearchEngines.filter { engine in engine.shortName == engineSet[0].shortName }
        .count
    )

    // Setting the default engine enables it.
    engines.updateDefaultEngine(engineSet[0].shortName, forType: .standard)
    XCTAssertTrue(engines.isEngineEnabled(engineSet[1]))

    // Setting the order may change the default engine, which enables it.
    engines.orderedEngines = [engineSet[2], engineSet[1], engineSet[0]]
    XCTAssertTrue(engines.isEngineEnabled(engineSet[2]))

    // The enabling should be persisted.
    engines.enableEngine(engineSet[2])
    engines.disableEngine(engineSet[1], type: .standard)
    engines.enableEngine(engineSet[0])

    let engines2 = SearchEngines()
    await engines2.loadSearchEngines()
    XCTAssertTrue(engines2.isEngineEnabled(engineSet[2]))
    XCTAssertFalse(engines2.isEngineEnabled(engineSet[1]))
    XCTAssertTrue(engines2.isEngineEnabled(engineSet[0]))
  }

  func testSearchSuggestionSettings() {
    let engines = SearchEngines()

    // By default, you shouldnt see search suggestions as this sends all users searches to their selected search
    // engine
    XCTAssertFalse(engines.shouldShowSearchSuggestions)

    // Setting should be persisted.
    engines.shouldShowSearchSuggestions = true

    let engines2 = SearchEngines()
    XCTAssertTrue(engines2.shouldShowSearchSuggestions)
  }

  func testGetOrderedEngines() async {
    // setup an existing search engine in the profile
    let engines = SearchEngines(locale: Locale(identifier: "pl_PL"))
    await engines.loadSearchEngines()
    XCTAssert(engines.orderedEngines.count > 1, "There should be more than one search engine")
    // default engine should be on second place if a priority engine is present.
    XCTAssertEqual(
      engines.orderedEngines[0].shortName,
      "Google",
      "Google should be the first search engine"
    )
  }

  func testSearchEngineParamsNewUser() async {
    Preferences.General.isFirstLaunch.value = true

    let searchEngines = SearchEngines()
    await searchEngines.loadSearchEngines()

    expectddgClientName(
      locales: ["de-DE"],
      expectedClientName: "bravened",
      searchEngines: searchEngines
    )
    expectddgClientName(
      locales: ["en-IE", "en-AU", "en-NZ"],
      expectedClientName: "braveed",
      searchEngines: searchEngines
    )
    expectddgClientName(
      locales: ["en-US, pl-PL"],
      expectedClientName: OpenSearchEngine.defaultSearchClientName,
      searchEngines: searchEngines
    )

    XCTAssert(
      getQwant(searchEngines: searchEngines).searchURLForQuery("test")!.absoluteString.contains(
        "client=brz-brave"
      )
    )
  }

  func testSearchEngineParamsExistingUser() async {
    Preferences.General.isFirstLaunch.value = false

    let searchEngines = SearchEngines()
    await searchEngines.loadSearchEngines()
    expectddgClientName(
      locales: ["de-DE"],
      expectedClientName: "bravened",
      searchEngines: searchEngines
    )
    expectddgClientName(
      locales: ["en-IE", "en-AU", "en-NZ"],
      expectedClientName: "braveed",
      searchEngines: searchEngines
    )
    expectddgClientName(
      locales: ["en-US, pl-PL"],
      expectedClientName: OpenSearchEngine.defaultSearchClientName,
      searchEngines: searchEngines
    )

    XCTAssert(
      getQwant(searchEngines: searchEngines).searchURLForQuery("test")!.absoluteString.contains(
        "client=brz-brave"
      )
    )
  }

  private func expectddgClientName(
    locales: [String],
    expectedClientName: String,
    searchEngines: SearchEngines
  ) {
    locales.forEach {
      let ddg = getDdg(searchEngines: searchEngines)
      let locale = Locale(identifier: $0)
      print(ddg.searchURLForQuery("test", locale: locale)!)

      let query = ddg.searchURLForQuery("test", locale: locale)
      XCTAssertNotNil(query)

      let tParam = URLComponents(url: query!, resolvingAgainstBaseURL: false)?
        .queryItems?
        .first(where: { $0.name == "t" })

      XCTAssertEqual(tParam?.value, expectedClientName)
    }
  }

  private func getDdg(searchEngines: SearchEngines) -> OpenSearchEngine {
    searchEngines.orderedEngines.first(where: {
      $0.shortName == OpenSearchEngine.EngineNames.duckDuckGo
    })!
  }

  private func getQwant(searchEngines: SearchEngines) -> OpenSearchEngine {
    searchEngines.orderedEngines.first(where: {
      $0.shortName == OpenSearchEngine.EngineNames.qwant
    })!
  }
}
