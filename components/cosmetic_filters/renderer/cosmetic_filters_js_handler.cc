/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/cosmetic_filters/renderer/cosmetic_filters_js_handler.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "brave/components/brave_shields/core/common/features.h"
#include "brave/components/content_settings/renderer/brave_content_settings_agent_impl.h"
#include "brave/components/cosmetic_filters/resources/grit/cosmetic_filters_generated.h"
#include "content/public/renderer/render_frame.h"
#include "gin/converter.h"
#include "gin/function_template.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/web/web_css_origin.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8.h"

namespace {

constexpr auto kVettedSearchEngines = base::MakeFixedFlatSet<std::string_view>(
    {"duckduckgo", "qwant", "bing", "startpage", "google", "yandex", "ecosia",
     "brave"});

// Entry point to content_cosmetic.ts script.
constexpr char kObservingScriptletEntryPoint[] =
    "window.content_cosmetic.tryScheduleQueuePump()";

constexpr char kScriptletInitScript[] =
    R"((function() {
          let text = '(function() {\nconst scriptletGlobals = (() => {\nconst forwardedMapMethods = ["has", "get", "set"];\nconst handler = {\nget(target, prop) { if (forwardedMapMethods.includes(prop)) { return Map.prototype[prop].bind(target) } return target.get(prop); },\nset(target, prop, value) { if (!forwardedMapMethods.includes(prop)) { target.set(prop, value); } }\n};\nreturn new Proxy(new Map(%s), handler);\n})();\nlet deAmpEnabled = %s;\n' + %s + '})()';
          let script;
          try {
            script = document.createElement('script');
            const textNode = document.createTextNode(text);
            script.appendChild(textNode);
            (document.head || document.documentElement).appendChild(script);
          } catch (ex) {
            /* Unused catch */
          }
          if (script) {
            if (script.parentNode) {
              script.parentNode.removeChild(script);
            }
            script.textContent = '';
          }
        })();)";

constexpr char kPreInitScript[] =
    R"((function() {
          if (window.content_cosmetic == undefined) {
            window.content_cosmetic = {};
          }
          %s
        })();)";

constexpr char kCosmeticFilteringInitScript[] =
    R"({
        const CC = window.content_cosmetic
        if (CC.hide1pContent === undefined)
          CC.hide1pContent = %s;
        if (CC.generichide === undefined)
          CC.generichide = %s;
        if (CC.firstSelectorsPollingDelayMs === undefined)
          CC.firstSelectorsPollingDelayMs = %s;
        if (CC.switchToSelectorsPollingThreshold === undefined)
          CC.switchToSelectorsPollingThreshold = %s;
        if (CC.fetchNewClassIdRulesThrottlingMs === undefined)
          CC.fetchNewClassIdRulesThrottlingMs = %s;
       })";

constexpr char kHideSelectorsInjectScript[] =
    R"((function() {
          let nextIndex =
              window.content_cosmetic.cosmeticStyleSheet.rules.length;
          const selectors = %s;
          selectors.forEach(selector => {
            if ((typeof selector === 'string') &&
                (window.content_cosmetic.hide1pContent ||
                !window.content_cosmetic.allSelectorsToRules.has(selector))) {
              let rule = selector + '{display:none !important;}';
              try {
                window.content_cosmetic.cosmeticStyleSheet.insertRule(
                  `${rule}`, nextIndex);
                if (!window.content_cosmetic.hide1pContent) {
                  window.content_cosmetic.allSelectorsToRules.set(
                    selector, nextIndex);
                  window.content_cosmetic.firstRunQueue.add(selector);
                }
                nextIndex++;
              } catch (e) {
                console.warn('Brave Shields ignored an invalid CSS injection: ' + rule)
              }
            }
          });
          if (!document.adoptedStyleSheets.includes(
              window.content_cosmetic.cosmeticStyleSheet)) {
            document.adoptedStyleSheets =
              [window.content_cosmetic.cosmeticStyleSheet,
                ...document.adoptedStyleSheets];
          };
        })();)";

constexpr char kIsDarkModeEnabledPropery[] = "isDarkModeEnabled";
constexpr char kBackgroundColorPropery[] = "bgcolor";

constexpr char kBtnCreateDisabledTextPropery[] = "btnCreateDisabledText";
constexpr char kBtnCreateEnabledTextPropery[] = "btnCreateEnabledText";
constexpr char kBtnManageTextPropery[] = "btnManageText";
constexpr char kBtnShowRulesBoxTextPropery[] = "btnShowRulesBoxText";
constexpr char kBtnHideRulesBoxTextPropery[] = "btnHideRulesBoxText";
constexpr char kBtnQuitTextPropery[] = "btnQuitText";

std::string LoadDataResource(const int id) {
  auto& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  if (resource_bundle.IsGzipped(id)) {
    return resource_bundle.LoadDataResourceString(id);
  }

  return std::string(resource_bundle.GetRawDataResource(id));
}

bool IsVettedSearchEngine(const GURL& url) {
  std::string domain_and_registry =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  size_t registry_len = net::registry_controlled_domains::GetRegistryLength(
      url, net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (domain_and_registry.length() > registry_len + 1) {
    std::string_view host =
        std::string_view(domain_and_registry)
            .substr(0, domain_and_registry.length() - registry_len - 1);
    if (kVettedSearchEngines.contains(host)) {
      return true;
    }
  }

  return false;
}

// ID is used in TRACE_ID_WITH_SCOPE(). Must be unique accoss the process.
int MakeUniquePerfId() {
  static int counter = 0;
  ++counter;
  return counter;
}

constexpr const char TRACE_CATEGORY[] = "brave.adblock";

// Gets content settings for a given frame's security origin, accounting for
// intermediaries like `about:blank`.
blink::WebContentSettingsClient* GetWebContentSettingsClient(
    content::RenderFrame* render_frame) {
  CHECK(render_frame);

  blink::WebLocalFrame* web_local_frame = render_frame->GetWebFrame();
  while (web_local_frame) {
    blink::WebContentSettingsClient* content_settings =
        web_local_frame->GetContentSettingsClient();
    if (content_settings && content_settings->HasContentSettingsRules()) {
      return content_settings;
    }

    blink::WebFrame* parent_web_frame = web_local_frame->Parent();
    if (!parent_web_frame || !parent_web_frame->IsWebLocalFrame()) {
      return content_settings;
    }

    web_local_frame = static_cast<blink::WebLocalFrame*>(parent_web_frame);
  }

  return nullptr;
}

mojo::AssociatedRemote<cosmetic_filters::mojom::CosmeticFiltersHandler>
MakeCosmeticFiltersHandler(content::RenderFrame* render_frame) {
  mojo::AssociatedRemote<cosmetic_filters::mojom::CosmeticFiltersHandler>
      handler;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(&handler);
  CHECK(handler);
  handler.reset_on_disconnect();
  return handler;
}

v8::Maybe<bool> CreateDataProperty(v8::Local<v8::Context> context,
                                   v8::Local<v8::Object> object,
                                   std::string_view name,
                                   v8::Local<v8::Value> value) {
  const v8::Local<v8::String> name_str =
      gin::StringToV8(v8::Isolate::GetCurrent(), name);

  return object->CreateDataProperty(context, name_str, value);
}

}  // namespace

namespace cosmetic_filters {

// A class to record performance events from content_filter.ts.
// The events are reported as async traces and/or UMAs and can be retrived by
// brave://tracing & brave://histograms.
class CosmeticFilterPerfTracker {
 public:
  int OnHandleMutationsBegin() {
    const auto event_id = MakeUniquePerfId();
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        TRACE_CATEGORY, "HandleMutations",
        TRACE_ID_WITH_SCOPE("HandleMutations", event_id));
    return event_id;
  }

  void OnHandleMutationsEnd(int event_id) {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        TRACE_CATEGORY, "HandleMutations",
        TRACE_ID_WITH_SCOPE("HandleMutations", event_id));
  }

  int OnQuerySelectorsBegin() {
    const auto event_id = MakeUniquePerfId();
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        TRACE_CATEGORY, "QuerySelectors",
        TRACE_ID_WITH_SCOPE("QuerySelectors", event_id));
    return event_id;
  }

  void OnQuerySelectorsEnd(int event_id) {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        TRACE_CATEGORY, "QuerySelectors",
        TRACE_ID_WITH_SCOPE("QuerySelectors", event_id));
  }
};

CosmeticFiltersJSHandler::CosmeticFiltersJSHandler(
    content::RenderFrame* render_frame,
    const int32_t isolated_world_id)
    : render_frame_(render_frame),
      isolated_world_id_(isolated_world_id),
      enabled_1st_party_cf_(false),
      v8_value_converter_(content::V8ValueConverter::Create()) {
  EnsureConnected();

  render_frame_->GetAssociatedInterfaceRegistry()
      ->AddInterface<mojom::CosmeticFiltersAgent>(base::BindRepeating(
          &CosmeticFiltersJSHandler::Bind, weak_ptr_factory_.GetWeakPtr()));

  const bool perf_tracker_enabled = base::FeatureList::IsEnabled(
      brave_shields::features::kCosmeticFilteringExtraPerfMetrics);
  if (perf_tracker_enabled) {
    perf_tracker_ = std::make_unique<CosmeticFilterPerfTracker>();
  }
}

CosmeticFiltersJSHandler::~CosmeticFiltersJSHandler() = default;

void CosmeticFiltersJSHandler::HiddenClassIdSelectors(
    const std::string& input) {
  if (!EnsureConnected())
    return;

  cosmetic_filters_resources_->HiddenClassIdSelectors(
      input, exceptions_,
      base::BindOnce(&CosmeticFiltersJSHandler::OnHiddenClassIdSelectors,
                     base::Unretained(this)));
}

bool CosmeticFiltersJSHandler::OnIsFirstParty(const std::string& url_string) {
  const auto url = url_.Resolve(url_string);
  if (!url.is_valid())
    return false;

  return net::registry_controlled_domains::SameDomainOrHost(
      url, url_, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

void CosmeticFiltersJSHandler::OnAddSiteCosmeticFilter(
    const std::string& selector) {
  const auto host = url_.host();
  GetElementPickerRemoteHandler()->AddSiteCosmeticFilter(selector);
}

void CosmeticFiltersJSHandler::OnManageCustomFilters() {
  GetElementPickerRemoteHandler()->ManageCustomFilters();
}

mojo::AssociatedRemote<cosmetic_filters::mojom::CosmeticFiltersHandler>&
CosmeticFiltersJSHandler::GetElementPickerRemoteHandler() {
  if (!element_picker_actions_handler_ ||
      !element_picker_actions_handler_.is_connected()) {
    element_picker_actions_handler_ = MakeCosmeticFiltersHandler(render_frame_);
    element_picker_actions_handler_.set_disconnect_handler(base::BindOnce(
        &CosmeticFiltersJSHandler::OnElementPickerRemoteHandlerDisconnect,
        weak_ptr_factory_.GetWeakPtr()));
  }
  return element_picker_actions_handler_;
}

void CosmeticFiltersJSHandler::OnElementPickerRemoteHandlerDisconnect() {
  element_picker_actions_handler_.reset();
}

v8::Local<v8::Promise> CosmeticFiltersJSHandler::GetCosmeticFilterThemeInfo(
    v8::Isolate* isolate) {
  v8::MaybeLocal<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(isolate->GetCurrentContext());

  if (!resolver.IsEmpty()) {
    auto promise_resolver =
        std::make_unique<v8::Global<v8::Promise::Resolver>>();
    promise_resolver->Reset(isolate, resolver.ToLocalChecked());
    auto context_old = std::make_unique<v8::Global<v8::Context>>(
        isolate, isolate->GetCurrentContext());

    GetElementPickerRemoteHandler()->GetElementPickerThemeInfo(base::BindOnce(
        &CosmeticFiltersJSHandler::OnGetCosmeticFilterThemeInfo,
        weak_ptr_factory_.GetWeakPtr(), std::move(promise_resolver), isolate,
        std::move(context_old)));

    return resolver.ToLocalChecked()->GetPromise();
  }

  return v8::Local<v8::Promise>();
}

void CosmeticFiltersJSHandler::OnGetCosmeticFilterThemeInfo(
    std::unique_ptr<v8::Global<v8::Promise::Resolver>> promise_resolver,
    v8::Isolate* isolate,
    std::unique_ptr<v8::Global<v8::Context>> context_old,
    bool is_dark_mode_enabled,
    int32_t background_color) {
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_old->Get(isolate);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(isolate, context->GetMicrotaskQueue(),
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Promise::Resolver> resolver = promise_resolver->Get(isolate);
  v8::Local<v8::Value> result;
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  CHECK(CreateDataProperty(context, object, kIsDarkModeEnabledPropery,
                           v8_value_converter_->ToV8Value(
                               base::Value(is_dark_mode_enabled), context))
            .ToChecked());
  CHECK(CreateDataProperty(context, object, kBackgroundColorPropery,
                           v8_value_converter_->ToV8Value(
                               base::Value(background_color), context))
            .ToChecked());
  result = object;

  std::ignore = resolver->Resolve(context, result);
}

v8::Local<v8::Promise> CosmeticFiltersJSHandler::GetLocalizedTexts(
    v8::Isolate* isolate) {
  v8::MaybeLocal<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(isolate->GetCurrentContext());

  if (!resolver.IsEmpty()) {
    auto promise_resolver =
        std::make_unique<v8::Global<v8::Promise::Resolver>>();
    promise_resolver->Reset(isolate, resolver.ToLocalChecked());
    auto context_old = std::make_unique<v8::Global<v8::Context>>(
        isolate, isolate->GetCurrentContext());

    GetElementPickerRemoteHandler()->GetElementPickerLocalizedTexts(
        base::BindOnce(&CosmeticFiltersJSHandler::OnGetLocalizedTexts,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(promise_resolver), isolate,
                       std::move(context_old)));

    return resolver.ToLocalChecked()->GetPromise();
  }

  return v8::Local<v8::Promise>();
}

void CosmeticFiltersJSHandler::OnGetLocalizedTexts(
    std::unique_ptr<v8::Global<v8::Promise::Resolver>> promise_resolver,
    v8::Isolate* isolate,
    std::unique_ptr<v8::Global<v8::Context>> context_old,
    mojom::ElementPickerLocalizationPtr loc) {
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_old->Get(isolate);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(isolate, context->GetMicrotaskQueue(),
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Promise::Resolver> resolver = promise_resolver->Get(isolate);
  v8::Local<v8::Value> result;
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  CHECK(CreateDataProperty(context, object, kBtnCreateDisabledTextPropery,
                           v8_value_converter_->ToV8Value(
                               base::Value(loc->create_disabled_text), context))
            .ToChecked());
  CHECK(CreateDataProperty(context, object, kBtnCreateEnabledTextPropery,
                           v8_value_converter_->ToV8Value(
                               base::Value(loc->create_enabled_text), context))
            .ToChecked());
  CHECK(CreateDataProperty(context, object, kBtnManageTextPropery,
                           v8_value_converter_->ToV8Value(
                               base::Value(loc->manage_text), context))
            .ToChecked());
  CHECK(CreateDataProperty(context, object, kBtnShowRulesBoxTextPropery,
                           v8_value_converter_->ToV8Value(
                               base::Value(loc->show_rules_text), context))
            .ToChecked());
  CHECK(CreateDataProperty(context, object, kBtnHideRulesBoxTextPropery,
                           v8_value_converter_->ToV8Value(
                               base::Value(loc->hide_rules_text), context))
            .ToChecked());
  CHECK(CreateDataProperty(context, object, kBtnQuitTextPropery,
                           v8_value_converter_->ToV8Value(
                               base::Value(loc->quit_text), context))
            .ToChecked());
  result = object;

  std::ignore = resolver->Resolve(context, result);
}

v8::Local<v8::Value> CosmeticFiltersJSHandler::GetPlatform(
    v8::Isolate* isolate) {
  return gin::StringToV8(isolate,
#if BUILDFLAG(IS_ANDROID)
                         "android"
#else   // !BUILDFLAG(IS_ANDROID)
                         "desktop"
#endif  // !BUILDFLAG(IS_ANDROID)
  );
}

void CosmeticFiltersJSHandler::AddJavaScriptObjectToFrame(
    v8::Local<v8::Context> context) {
  CHECK(render_frame_);
  v8::Isolate* isolate =
      render_frame_->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  CreateWorkerObject(isolate, context);
  bundle_injected_ = false;
}

// Stylesheets injected this way will be able to override `!important` styles
// from in-page styles, but cannot be reverted.
// `WebDocument::RemoveInsertedStyleSheet` works, but using a single stylesheet
// per rule has a significant performance impact and should be avoided.
void CosmeticFiltersJSHandler::InjectStylesheet(const std::string& stylesheet) {
  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();

  blink::WebStyleSheetKey* style_sheet_key = nullptr;
  blink::WebString stylesheet_webstring =
      blink::WebString::FromUTF8(stylesheet);
  web_frame->GetDocument().InsertStyleSheet(
      stylesheet_webstring, style_sheet_key, blink::WebCssOrigin::kUser);
}

void CosmeticFiltersJSHandler::CreateWorkerObject(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context) {
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Object> cosmetic_filters_obj;
  v8::Local<v8::Value> cosmetic_filters_value;
  if (!global->Get(context, gin::StringToV8(isolate, "cf_worker"))
           .ToLocal(&cosmetic_filters_value) ||
      !cosmetic_filters_value->IsObject()) {
    cosmetic_filters_obj = v8::Object::New(isolate);
    global
        ->Set(context, gin::StringToSymbol(isolate, "cf_worker"),
              cosmetic_filters_obj)
        .Check();
    BindFunctionsToObject(isolate, cosmetic_filters_obj);
  }
}

void CosmeticFiltersJSHandler::BindFunctionsToObject(
    v8::Isolate* isolate,
    v8::Local<v8::Object> javascript_object) {
  BindFunctionToObject(
      isolate, javascript_object, "hiddenClassIdSelectors",
      base::BindRepeating(&CosmeticFiltersJSHandler::HiddenClassIdSelectors,
                          base::Unretained(this)));
  BindFunctionToObject(
      isolate, javascript_object, "isFirstPartyUrl",
      base::BindRepeating(&CosmeticFiltersJSHandler::OnIsFirstParty,
                          base::Unretained(this)));
  BindFunctionToObject(
      isolate, javascript_object, "injectStylesheet",
      base::BindRepeating(&CosmeticFiltersJSHandler::InjectStylesheet,
                          base::Unretained(this)));

  BindFunctionToObject(
      isolate, javascript_object, "addSiteCosmeticFilter",
      base::BindRepeating(&CosmeticFiltersJSHandler::OnAddSiteCosmeticFilter,
                          base::Unretained(this)));
  BindFunctionToObject(
      isolate, javascript_object, "manageCustomFilters",
      base::BindRepeating(&CosmeticFiltersJSHandler::OnManageCustomFilters,
                          base::Unretained(this)));
  BindFunctionToObject(
      isolate, javascript_object, "getElementPickerThemeInfo",
      base::BindRepeating(&CosmeticFiltersJSHandler::GetCosmeticFilterThemeInfo,
                          base::Unretained(this)));
  BindFunctionToObject(
      isolate, javascript_object, "getLocalizedTexts",
      base::BindRepeating(&CosmeticFiltersJSHandler::GetLocalizedTexts,
                          base::Unretained(this)));

  BindFunctionToObject(
      isolate, javascript_object, "getPlatform",
      base::BindRepeating(&CosmeticFiltersJSHandler::GetPlatform,
                          base::Unretained(this), isolate));

  if (perf_tracker_) {
    BindFunctionToObject(
        isolate, javascript_object, "onHandleMutationsBegin",
        base::BindRepeating(&CosmeticFilterPerfTracker::OnHandleMutationsBegin,
                            base::Unretained(perf_tracker_.get())));
    BindFunctionToObject(
        isolate, javascript_object, "onHandleMutationsEnd",
        base::BindRepeating(&CosmeticFilterPerfTracker::OnHandleMutationsEnd,
                            base::Unretained(perf_tracker_.get())));
    BindFunctionToObject(
        isolate, javascript_object, "onQuerySelectorsBegin",
        base::BindRepeating(&CosmeticFilterPerfTracker::OnQuerySelectorsBegin,
                            base::Unretained(perf_tracker_.get())));
    BindFunctionToObject(
        isolate, javascript_object, "onQuerySelectorsEnd",
        base::BindRepeating(&CosmeticFilterPerfTracker::OnQuerySelectorsEnd,
                            base::Unretained(perf_tracker_.get())));
  }
}

template <typename Sig>
void CosmeticFiltersJSHandler::BindFunctionToObject(
    v8::Isolate* isolate,
    v8::Local<v8::Object> javascript_object,
    const std::string& name,
    const base::RepeatingCallback<Sig>& callback) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  // Get the isolate associated with this object.
  javascript_object
      ->Set(context, gin::StringToSymbol(isolate, name),
            gin::CreateFunctionTemplate(isolate, callback)
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
}

bool CosmeticFiltersJSHandler::EnsureConnected() {
  if (!cosmetic_filters_resources_.is_bound()) {
    render_frame_->GetBrowserInterfaceBroker().GetInterface(
        cosmetic_filters_resources_.BindNewPipeAndPassReceiver());
    cosmetic_filters_resources_.set_disconnect_handler(
        base::BindOnce(&CosmeticFiltersJSHandler::OnRemoteDisconnect,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  return cosmetic_filters_resources_.is_bound();
}

void CosmeticFiltersJSHandler::OnRemoteDisconnect() {
  cosmetic_filters_resources_.reset();
  EnsureConnected();
}

void CosmeticFiltersJSHandler::Bind(
    mojo::PendingAssociatedReceiver<mojom::CosmeticFiltersAgent> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void CosmeticFiltersJSHandler::LaunchContentPicker() {
  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
  if (web_frame->IsProvisional()) {
    return;
  }
  web_frame->ExecuteScriptInIsolatedWorld(
      isolated_world_id_,
      blink::WebScriptSource(blink::WebString::FromUTF8(
          LoadDataResource(IDR_COSMETIC_FILTERS_ELEMENT_PICKER_BUNDLE_JS))),
      blink::BackForwardCacheAware::kAllow);
}

bool CosmeticFiltersJSHandler::ProcessURL(
    const GURL& url,
    std::optional<base::OnceClosure> callback) {
  resources_dict_ = std::nullopt;
  url_ = url;
  enabled_1st_party_cf_ = false;

  // Trivially, don't make exceptions for malformed URLs.
  if (!EnsureConnected() || url_.is_empty() || !url_.is_valid())
    return false;

  auto* content_settings = GetWebContentSettingsClient(render_frame_);
  CHECK(content_settings);

  const bool force_cosmetic_filtering =
      render_frame_->GetBlinkPreferences().force_cosmetic_filtering;
  const bool page_in_reader_mode =
      render_frame_->GetBlinkPreferences().page_in_reader_mode;
  if (page_in_reader_mode ||
      (!force_cosmetic_filtering &&
       !content_settings->IsCosmeticFilteringEnabled(url_))) {
    return false;
  }

  enabled_1st_party_cf_ =
      force_cosmetic_filtering ||
      render_frame_->GetWebFrame()->IsCrossOriginToOutermostMainFrame() ||
      content_settings->IsFirstPartyCosmeticFilteringEnabled(url_) ||
      net::registry_controlled_domains::SameDomainOrHost(
          url_,
          url::Origin::CreateFromNormalizedTuple("https", "youtube.com", 443),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (callback.has_value()) {
    SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
        "Brave.CosmeticFilters.UrlCosmeticResources");
    TRACE_EVENT1("brave.adblock", "UrlCosmeticResources", "url", url_.spec());
    cosmetic_filters_resources_->UrlCosmeticResources(
        url_.spec(), enabled_1st_party_cf_,
        base::BindOnce(&CosmeticFiltersJSHandler::OnUrlCosmeticResources,
                       base::Unretained(this), std::move(callback.value())));
  } else {
    TRACE_EVENT1("brave.adblock", "UrlCosmeticResourcesSync", "url",
                 url_.spec());
    SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
        "Brave.CosmeticFilters.UrlCosmeticResourcesSync");
    base::Value result;
    cosmetic_filters_resources_->UrlCosmeticResources(
        url_.spec(), enabled_1st_party_cf_, &result);

    auto* dict = result.GetIfDict();
    if (dict)
      resources_dict_ = std::move(*dict);
  }

  return true;
}

void CosmeticFiltersJSHandler::OnUrlCosmeticResources(
    base::OnceClosure callback,
    base::Value result) {
  if (!EnsureConnected())
    return;

  auto* dict = result.GetIfDict();
  if (dict)
    resources_dict_ = std::move(*dict);

  std::move(callback).Run();
}

void CosmeticFiltersJSHandler::ApplyRules(bool de_amp_enabled) {
  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
  if (!resources_dict_ || web_frame->IsProvisional())
    return;

  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("Brave.CosmeticFilters.ApplyRules");
  TRACE_EVENT1("brave.adblock", "ApplyRules", "url", url_.spec());

  std::string scriptlet_script;
  base::Value* injected_script = resources_dict_->Find("injected_script");

  if (injected_script &&
      base::JSONWriter::Write(*injected_script, &scriptlet_script)) {
    const bool scriptlet_debug_enabled = base::FeatureList::IsEnabled(
        brave_shields::features::kBraveAdblockScriptletDebugLogs);

    scriptlet_script =
        absl::StrFormat(kScriptletInitScript,
                        scriptlet_debug_enabled ? "[[\"canDebug\", true]]" : "",
                        de_amp_enabled ? "true" : "false", scriptlet_script);
  }
  if (!scriptlet_script.empty()) {
    web_frame->ExecuteScriptInIsolatedWorld(
        isolated_world_id_,
        blink::WebScriptSource(blink::WebString::FromUTF8(scriptlet_script)),
        blink::BackForwardCacheAware::kAllow);
  }

  // Working on css rules
  generichide_ = resources_dict_->FindBool("generichide").value_or(false);
  namespace bf = brave_shields::features;
  std::string cosmetic_filtering_init_script = absl::StrFormat(
      kCosmeticFilteringInitScript, enabled_1st_party_cf_ ? "true" : "false",
      generichide_ ? "true" : "false",
      render_frame_->IsMainFrame()
          ? "undefined"
          : bf::kCosmeticFilteringSubFrameFirstSelectorsPollingDelayMs.Get(),
      bf::kCosmeticFilteringswitchToSelectorsPollingThreshold.Get(),
      bf::kCosmeticFilteringFetchNewClassIdRulesThrottlingMs.Get());
  std::string pre_init_script =
      absl::StrFormat(kPreInitScript, cosmetic_filtering_init_script);

  web_frame->ExecuteScriptInIsolatedWorld(
      isolated_world_id_,
      blink::WebScriptSource(blink::WebString::FromUTF8(pre_init_script)),
      blink::BackForwardCacheAware::kAllow);
  ExecuteObservingBundleEntryPoint();

  CSSRulesRoutine(*resources_dict_);

  const std::string* procedural_actions_script =
      resources_dict_->FindString("procedural_actions_script");
  if (procedural_actions_script) {
    v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
    v8::HandleScope handle_scope(isolate);

    v8::Local<v8::Value> v8_stylesheet =
        web_frame->ExecuteScriptInIsolatedWorldAndReturnValue(
            isolated_world_id_,
            blink::WebScriptSource(
                blink::WebString::FromUTF8(*procedural_actions_script)),
            blink::BackForwardCacheAware::kAllow);
    if (!v8_stylesheet.IsEmpty() && v8_stylesheet->IsString()) {
      v8::Local<v8::String> v8_str = v8_stylesheet.As<v8::String>();
      int length = v8_str->Utf8LengthV2(isolate);
      if (length > 0) {
        std::string str;
        gin::Converter<std::string>::FromV8(isolate, v8_str, &str);

        InjectStylesheet(str);
      }
    }
  }
}

void CosmeticFiltersJSHandler::CSSRulesRoutine(
    const base::Value::Dict& resources_dict) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("Brave.CosmeticFilters.CSSRulesRoutine");
  TRACE_EVENT1("brave.adblock", "CSSRulesRoutine", "url", url_.spec());

  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
  const auto* cf_exceptions_list = resources_dict.FindList("exceptions");
  if (cf_exceptions_list) {
    for (const auto& item : *cf_exceptions_list) {
      if (item.is_string()) {
        exceptions_.push_back(item.GetString());
      }
    }
  }
  // If its a vetted engine AND we're not in aggressive mode, don't apply
  // cosmetic filtering from the default engine.
  const auto* hide_selectors_list =
      (IsVettedSearchEngine(url_) && !enabled_1st_party_cf_)
          ? nullptr
          : resources_dict.FindList("hide_selectors");

  std::string stylesheet = "";

  if (hide_selectors_list && !hide_selectors_list->empty()) {
    // treat `hide_selectors` the same as `force_hide_selectors` if aggressive
    // mode is enabled.
    if (enabled_1st_party_cf_) {
      for (auto& selector : *hide_selectors_list) {
        if (selector.is_string()) {
          stylesheet += selector.GetString() + "{display:none !important}";
        }
      }
    } else {
      std::string json_selectors;
      base::JSONWriter::Write(*hide_selectors_list, &json_selectors);
      if (json_selectors.empty()) {
        json_selectors = "[]";
      }
      // Building a script for stylesheet modifications
      std::string new_selectors_script =
          absl::StrFormat(kHideSelectorsInjectScript, json_selectors);
      web_frame->ExecuteScriptInIsolatedWorld(
          isolated_world_id_,
          blink::WebScriptSource(
              blink::WebString::FromUTF8(new_selectors_script)),
          blink::BackForwardCacheAware::kAllow);
    }
  }

  const auto* force_hide_selectors_list =
      resources_dict.FindList("force_hide_selectors");
  if (force_hide_selectors_list) {
    for (auto& selector : *force_hide_selectors_list) {
      if (selector.is_string()) {
        stylesheet += selector.GetString() + "{display:none !important}";
      }
    }
  }

  if (!stylesheet.empty()) {
    InjectStylesheet(stylesheet);
  }

  if (!enabled_1st_party_cf_)
    ExecuteObservingBundleEntryPoint();
}

void CosmeticFiltersJSHandler::OnHiddenClassIdSelectors(
    base::Value::Dict result) {
  if (generichide_) {
    return;
  }

  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Brave.CosmeticFilters.OnHiddenClassIdSelectors");
  TRACE_EVENT1("brave.adblock", "OnHiddenClassIdSelectors", "url", url_.spec());

  base::Value::List* hide_selectors = result.FindList("hide_selectors");
  if (!hide_selectors) {
    return;
  }

  base::Value::List* force_hide_selectors =
      result.FindList("force_hide_selectors");

  if (force_hide_selectors && force_hide_selectors->size() != 0) {
    std::string stylesheet = "";
    for (auto& selector : *force_hide_selectors) {
      if (selector.is_string()) {
        stylesheet += selector.GetString() + "{display:none !important}";
      }
    }
    InjectStylesheet(stylesheet);
  }

  // If its a vetted engine AND we're not in aggressive
  // mode, don't check elements from the default engine (in hide_selectors).
  if (!enabled_1st_party_cf_ && IsVettedSearchEngine(url_))
    return;

  if (enabled_1st_party_cf_) {
    std::string stylesheet = "";
    for (auto& selector : *hide_selectors) {
      if (selector.is_string()) {
        stylesheet += selector.GetString() + "{display:none !important}";
      }
    }
    InjectStylesheet(stylesheet);
  } else {
    blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
    std::string json_selectors;
    if (!base::JSONWriter::Write(*hide_selectors, &json_selectors) ||
        json_selectors.empty()) {
      json_selectors = "[]";
    }
    // Building a script for stylesheet modifications
    std::string new_selectors_script =
        absl::StrFormat(kHideSelectorsInjectScript, json_selectors);
    if (hide_selectors->size() != 0) {
      web_frame->ExecuteScriptInIsolatedWorld(
          isolated_world_id_,
          blink::WebScriptSource(
              blink::WebString::FromUTF8(new_selectors_script)),
          blink::BackForwardCacheAware::kAllow);
    }

    if (!enabled_1st_party_cf_)
      ExecuteObservingBundleEntryPoint();
  }
}

void CosmeticFiltersJSHandler::ExecuteObservingBundleEntryPoint() {
  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
  CHECK(web_frame);

  if (!bundle_injected_) {
    SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
        "Brave.CosmeticFilters.ExecuteObservingBundleEntryPoint");
    TRACE_EVENT1("brave.adblock", "ExecuteObservingBundleEntryPoint", "url",
                 url_.spec());

    static base::NoDestructor<std::string> s_observing_script(
        LoadDataResource(IDR_COSMETIC_FILTERS_COSMETIC_FILTERS_BUNDLE_JS));
    bundle_injected_ = true;

    web_frame->ExecuteScriptInIsolatedWorld(
        isolated_world_id_,
        blink::WebScriptSource(blink::WebString::FromUTF8(*s_observing_script)),
        blink::BackForwardCacheAware::kAllow);

    // kObservingScriptletEntryPoint was called by `s_observing_script`.
    return;
  }

  web_frame->ExecuteScriptInIsolatedWorld(
      isolated_world_id_,
      blink::WebScriptSource(
          blink::WebString::FromUTF8(kObservingScriptletEntryPoint)),
      blink::BackForwardCacheAware::kAllow);
}

}  // namespace cosmetic_filters
