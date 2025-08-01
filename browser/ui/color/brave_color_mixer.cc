/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/color/brave_color_mixer.h"

#include "base/numerics/safe_conversions.h"
#include "brave/browser/themes/brave_dark_mode_utils.h"
#include "brave/browser/ui/color/brave_color_id.h"
#include "brave/browser/ui/color/color_palette.h"
#include "brave/components/brave_vpn/common/buildflags/buildflags.h"
#include "brave/components/brave_wayback_machine/buildflags/buildflags.h"
#include "brave/components/playlist/common/buildflags/buildflags.h"
#include "brave/components/speedreader/common/buildflags/buildflags.h"
#include "brave/ui/color/nala/nala_color_id.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "chrome/browser/ui/color/material_chrome_color_mixer.h"
#include "chrome/browser/ui/color/material_omnibox_color_mixer.h"
#include "chrome/browser/ui/color/material_side_panel_color_mixer.h"
#include "chrome/browser/ui/color/material_tab_strip_color_mixer.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(ENABLE_PLAYLIST_WEBUI)
#include "brave/browser/ui/color/playlist/playlist_color_mixer.h"
#include "brave/components/playlist/common/features.h"
#endif

namespace {

// Copied from //chrome/browser/ui/omnibox//omnibox_theme.h
// As below values are not changed for several years, it would be safe to copy.
// Can't include //chrome/browser/ui/omnibox in here due to circular deps.
constexpr float kOmniboxOpacityHovered = 0.10f;
constexpr float kOmniboxOpacitySelected = 0.16f;

SkColor PickColorContrastingToToolbar(const ui::ColorProviderKey& key,
                                      const ui::ColorMixer& mixer,
                                      SkColor color1,
                                      SkColor color2) {
  // Custom theme's toolbar color will be the final color.
  auto toolbar_color = mixer.GetResultColor(kColorToolbar);
  SkColor custom_toolbar_color;
  if (key.custom_theme &&
      key.custom_theme->GetColor(ThemeProperties::COLOR_TOOLBAR,
                                 &custom_toolbar_color)) {
    toolbar_color = custom_toolbar_color;
  }
  return color_utils::PickContrastingColor(color1, color2, toolbar_color);
}

bool HasCustomToolbarColor(const ui::ColorProviderKey& key) {
  SkColor custom_toolbar_color;
  return key.custom_theme &&
         key.custom_theme->GetColor(ThemeProperties::COLOR_TOOLBAR,
                                    &custom_toolbar_color);
}

#if BUILDFLAG(ENABLE_BRAVE_VPN) || BUILDFLAG(ENABLE_SPEEDREADER)
SkColor PickSimilarColorToToolbar(const ui::ColorProviderKey& key,
                                  const ui::ColorMixer& mixer,
                                  SkColor light_theme_color,
                                  SkColor dark_theme_color) {
  auto toolbar_color = mixer.GetResultColor(kColorToolbar);
  SkColor custom_toolbar_color;
  if (key.custom_theme &&
      key.custom_theme->GetColor(ThemeProperties::COLOR_TOOLBAR,
                                 &custom_toolbar_color)) {
    toolbar_color = custom_toolbar_color;
  }

  // Give min constrast color.
  return color_utils::IsDark(toolbar_color) ? dark_theme_color
                                            : light_theme_color;
}
#endif

bool IsHighContrast() {
#if !defined(USE_AURA)
  ui::NativeTheme* native_theme = nullptr;
#else
  // For high contrast, selected rows use inverted colors to stand out more.
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
#endif  // !defined(USE_AURA)
  return native_theme && native_theme->UserHasContrastPreference();
}

SkColor BlendIdTowardsMaxContrast(SkColor color, float opacity) {
  return color_utils::BlendTowardMaxContrast(color,
                                             base::ClampRound(opacity * 0xff));
}

// Hover/Select for the Omnibox are based on opacity and the result color. The
// calculation is always the same.
void AddOmniboxHoverSelect(ui::ColorMixer& mixer) {
  mixer[kColorOmniboxResultsBackgroundHovered] = {BlendIdTowardsMaxContrast(
      mixer.GetResultColor(kColorOmniboxResultsBackground),
      kOmniboxOpacityHovered)};
  mixer[kColorOmniboxResultsBackgroundSelected] = {BlendIdTowardsMaxContrast(
      mixer.GetResultColor(kColorOmniboxResultsBackground),
      kOmniboxOpacitySelected)};
}

#if BUILDFLAG(ENABLE_BRAVE_VPN)
void AddBraveVpnColorMixer(ui::ColorProvider* provider,
                           const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();
  if (key.custom_theme) {
    mixer[kColorBraveVpnButtonIconConnected] = {PickSimilarColorToToolbar(
        key, mixer, mixer.GetResultColor(nala::kColorPrimitiveGreen50),
        mixer.GetResultColor(nala::kColorPrimitiveGreen60))};
    mixer[kColorBraveVpnButtonIconDisconnected] = {PickSimilarColorToToolbar(
        key, mixer, mixer.GetResultColor(nala::kColorPrimitiveNeutral30),
        mixer.GetResultColor(nala::kColorPrimitiveNeutral80))};
    mixer[kColorBraveVpnButtonIconError] = {PickSimilarColorToToolbar(
        key, mixer, mixer.GetResultColor(nala::kColorPrimitiveRed60),
        mixer.GetResultColor(nala::kColorPrimitiveRed50))};
    mixer[kColorBraveVpnButtonErrorBackgroundNormal] = {
        PickSimilarColorToToolbar(
            key, mixer, mixer.GetResultColor(nala::kColorPrimitiveRed95),
            mixer.GetResultColor(nala::kColorPrimitiveRed15))};
  } else {
    mixer[kColorBraveVpnButtonIconConnected] = {
        nala::kColorSystemfeedbackSuccessIcon};
    mixer[kColorBraveVpnButtonIconDisconnected] = {nala::kColorIconDefault};
    mixer[kColorBraveVpnButtonIconError] = {
        nala::kColorSystemfeedbackErrorIcon};
    mixer[kColorBraveVpnButtonErrorBackgroundNormal] = {
        nala::kColorSystemfeedbackErrorBackground};
  }

  mixer[kColorBraveVpnButtonBackgroundNormal] = {kColorToolbar};
}
#endif

#if BUILDFLAG(ENABLE_SPEEDREADER)
void AddBraveSpeedreaderColorMixer(ui::ColorProvider* provider,
                                   const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  if (key.custom_theme) {
    auto mix_custom = [&mixer, &key](ui::ColorId light_id,
                                     ui::ColorId dark_id) -> SkColor {
      return PickSimilarColorToToolbar(key, mixer,
                                       mixer.GetResultColor(light_id),
                                       mixer.GetResultColor(dark_id));
    };

    mixer[kColorSpeedreaderIcon] = {mix_custom(nala::kColorPrimitivePrimary35,
                                               nala::kColorPrimitivePrimary80)};

    mixer[kColorSpeedreaderToolbarForeground] = {mix_custom(
        nala::kColorPrimitiveNeutral30, nala::kColorPrimitiveNeutral80)};

    mixer[kColorSpeedreaderToolbarBorder] = {mix_custom(
        nala::kColorPrimitiveNeutral90, nala::kColorPrimitiveNeutral25)};

    mixer[kColorSpeedreaderToolbarButtonHover] = {mix_custom(
        nala::kColorPrimitiveNeutral95, nala::kColorPrimitiveNeutral10)};
    mixer[kColorSpeedreaderToolbarButtonActive] = {mix_custom(
        nala::kColorPrimitiveNeutral90, nala::kColorPrimitiveNeutral5)};
    mixer[kColorSpeedreaderToolbarButtonActiveText] = {mix_custom(
        nala::kColorPrimitivePrimary35, nala::kColorPrimitivePrimary80)};
    mixer[kColorSpeedreaderToolbarButtonBorder] = {mix_custom(
        nala::kColorPrimitiveNeutral90, nala::kColorPrimitiveNeutral25)};
  } else {
    mixer[kColorSpeedreaderIcon] = {nala::kColorIconInteractive};
    mixer[kColorSpeedreaderToolbarForeground] = {nala::kColorIconDefault};
    mixer[kColorSpeedreaderToolbarBorder] = {
        nala::kColorDesktopbrowserToolbarButtonOutline};

    mixer[kColorSpeedreaderToolbarButtonHover] = {
        nala::kColorDesktopbrowserToolbarButtonHover};
    mixer[kColorSpeedreaderToolbarButtonActive] = {
        nala::kColorDesktopbrowserToolbarButtonActive};
    mixer[kColorSpeedreaderToolbarButtonActiveText] = {
        nala::kColorIconInteractive};
    mixer[kColorSpeedreaderToolbarButtonBorder] = {
        nala::kColorDesktopbrowserToolbarButtonOutline};
  }

  mixer[kColorSpeedreaderToolbarBackground] = {kColorToolbar};
}
#endif

void AddChromeLightThemeColorMixer(ui::ColorProvider* provider,
                                   const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorTabThrobber] = {SkColorSetRGB(0xd7, 0x55, 0x26)};
  mixer[kColorBookmarkBarForeground] = {kColorTabForegroundActiveFrameActive};
  mixer[ui::kColorBadgeBackground] = {SkColorSetRGB(95, 92, 241)};
  mixer[ui::kColorBadgeForeground] = {SkColorSetRGB(245, 244, 254)};
  mixer[kColorForTest] = {kLightColorForTest};
  mixer[kColorNewTabButtonBackgroundFrameActive] = {ui::kColorFrameActive};
  mixer[kColorNewTabButtonBackgroundFrameInactive] = {ui::kColorFrameInactive};
  mixer[kColorNewTabPageBackground] = {kBraveNewTabBackgroundLight};
  mixer[kColorTabStrokeFrameActive] = {ui::kColorRefNeutral80};
  mixer[kColorTabStrokeFrameInactive] = {kColorTabStrokeFrameActive};
  mixer[kColorToolbarButtonIconInactive] = {
      ui::SetAlpha(kColorToolbarButtonIcon, kBraveDisabledControlAlpha)};
  mixer[ui::kColorToggleButtonThumbOff] = {SK_ColorWHITE};
  mixer[ui::kColorToggleButtonThumbOn] = {SkColorSetRGB(0x4C, 0x54, 0xD2)};
  mixer[ui::kColorToggleButtonTrackOff] = {SkColorSetRGB(0xDA, 0xDC, 0xE8)};
  mixer[ui::kColorToggleButtonTrackOn] = {SkColorSetRGB(0xE1, 0xE2, 0xF6)};

  mixer[kColorTabCloseButtonFocusRingActive] = {
      ui::kColorFocusableBorderFocused};
  mixer[kColorTabCloseButtonFocusRingInactive] = {
      ui::kColorFocusableBorderFocused};
  mixer[kColorTabFocusRingActive] = {ui::kColorFocusableBorderFocused};
  mixer[kColorTabFocusRingInactive] = {ui::kColorFocusableBorderFocused};

  // Upstream uses tab's background color as omnibox chip background color.
  // In our light mode, there is no difference between location bar's bg
  // color and tab's bg color. So, it looks like chip's bg color is transparent.
  // Use frame color as chip background to have different bg color.
  mixer[kColorOmniboxChipBackground] = {ui::kColorFrameActive};
}

void AddChromeDarkThemeColorMixer(ui::ColorProvider* provider,
                                  const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorTabThrobber] = {SkColorSetRGB(0xd7, 0x55, 0x26)};
  mixer[kColorBookmarkBarForeground] = {kColorTabForegroundActiveFrameActive};
  mixer[ui::kColorBadgeBackground] = {SkColorSetRGB(135, 132, 244)};
  mixer[ui::kColorBadgeForeground] = {SkColorSetRGB(14, 14, 52)};
  mixer[kColorForTest] = {kDarkColorForTest};
  mixer[kColorNewTabButtonBackgroundFrameActive] = {ui::kColorFrameActive};
  mixer[kColorNewTabButtonBackgroundFrameInactive] = {ui::kColorFrameInactive};
  mixer[kColorNewTabPageBackground] = {kBraveNewTabBackgroundDark};
  mixer[kColorTabStrokeFrameActive] = {ui::kColorRefNeutral25};
  mixer[kColorTabStrokeFrameInactive] = {kColorToolbar};
  mixer[kColorToolbarButtonIconInactive] = {
      ui::SetAlpha(kColorToolbarButtonIcon, kBraveDisabledControlAlpha)};
  mixer[kColorToolbarTopSeparatorFrameActive] = {kColorToolbar};
  mixer[kColorToolbarTopSeparatorFrameInactive] = {kColorToolbar};
  mixer[ui::kColorToggleButtonThumbOff] = {SK_ColorWHITE};
  mixer[ui::kColorToggleButtonThumbOn] = {SkColorSetRGB(0x44, 0x36, 0xE1)};
  mixer[ui::kColorToggleButtonTrackOff] = {SkColorSetRGB(0x5E, 0x61, 0x75)};
  mixer[ui::kColorToggleButtonTrackOn] = {SkColorSetRGB(0x76, 0x79, 0xB1)};

  mixer[kColorTabCloseButtonFocusRingActive] = {
      ui::kColorFocusableBorderFocused};
  mixer[kColorTabCloseButtonFocusRingInactive] = {
      ui::kColorFocusableBorderFocused};
  mixer[kColorTabFocusRingActive] = {ui::kColorFocusableBorderFocused};
  mixer[kColorTabFocusRingInactive] = {ui::kColorFocusableBorderFocused};
}

void AddChromeColorMixerForAllThemes(ui::ColorProvider* provider,
                                     const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  // Use same ink drop effect for all themes including custome themes.
  // Toolbar button's inkdrop highlight/visible colors depends on toolbar color.
  auto get_toolbar_ink_drop_color = [](float dark_opacity, float light_opacity,
                                       SkColor input,
                                       const ui::ColorMixer& mixer) {
    const float highlight_opacity =
        color_utils::IsDark(mixer.GetResultColor(kColorToolbar))
            ? dark_opacity
            : light_opacity;
    return SkColorSetA(SK_ColorBLACK, 0xFF * highlight_opacity);
  };
  mixer[kColorToolbarInkDropHover] = {
      base::BindRepeating(get_toolbar_ink_drop_color, 0.25f, 0.05f)};
  mixer[kColorToolbarInkDropRipple] = {
      base::BindRepeating(get_toolbar_ink_drop_color, 0.4f, 0.1f)};

  mixer[ui::kColorFrameActive] = {ui::kColorSysHeader};
  // TODO(simonhong): Should we adjust frame color for inactive window?
  mixer[ui::kColorFrameInactive] = {ui::kColorFrameActive};

  if (key.custom_theme) {
    return;
  }

  // We don't show border when omnibox doesn't have focus but still
  // contains in-progress user input.
  mixer[kColorLocationBarBorderOnMismatch] = {SK_ColorTRANSPARENT};
}

void AddBraveColorMixerForAllThemes(ui::ColorProvider* provider,
                                    const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  // Custom theme will use this color. Other themes could have another
  // color by their color mixers.
  mixer[kColorToolbarButtonActivated] = {SkColorSetRGB(0x7C, 0x91, 0xFF)};
  mixer[kColorSidebarButtonPressed] = {kColorToolbarButtonActivated};

  mixer[kColorLocationBarFocusRing] = {nala::kColorPrimitivePrimary50};

  // Search conversion button in omnibox.
  mixer[kColorSearchConversionButtonText] = {nala::kColorPrimary60};
  mixer[kColorSearchConversionButtonBorder] = {nala::kColorPrimary20};
  mixer[kColorSearchConversionButtonBackground] = {nala::kColorPrimary10};
  mixer[kColorSearchConversionButtonBackgroundHovered] = {
      nala::kColorPrimary20};
  mixer[kColorSearchConversionButtonText] = {nala::kColorPrimary60};
  mixer[kColorSearchConversionButtonCloseButton] = {
      ui::AlphaBlend({nala::kColorIconInteractive},
                     {kColorSearchConversionButtonBackground}, 0.5 * 0xff)};
  mixer[kColorSearchConversionButtonCloseButtonHovered] = {
      ui::AlphaBlend({nala::kColorIconInteractive},
                     {kColorSearchConversionButtonBackground}, 0.7 * 0xff)};
  mixer[kColorSearchConversionButtonCaratRight] = {
      kColorSearchConversionButtonCloseButton};
  mixer[kColorSearchConversionButtonShadow1] = {
      SkColorSetA(SK_ColorBLACK, 0.05 * 255)};
  mixer[kColorSearchConversionButtonShadow2] = {
      SkColorSetA(SK_ColorBLACK, 0.1 * 255)};
}

}  // namespace

void AddBravifiedChromeThemeColorMixer(ui::ColorProvider* provider,
                                       const ui::ColorProviderKey& key) {
  AddChromeColorMixerForAllThemes(provider, key);

  AddMaterialChromeColorMixer(provider, key);
  AddMaterialOmniboxColorMixer(provider, key);
  AddMaterialTabStripColorMixer(provider, key);
  AddMaterialSidePanelColorMixer(provider, key);

  // TODO(simonhong): Use leo color when it's ready.
  // TODO(simonhong): Move these overriding to
  // AddChromeColorMixerForAllThemes().
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorAppMenuHighlightSeverityLow] = {
      PickColorContrastingToToolbar(key, mixer, SkColorSetRGB(0x00, 0x46, 0x07),
                                    SkColorSetRGB(0x58, 0xE1, 0x55))};
  mixer[kColorAppMenuHighlightSeverityMedium] = {
      PickColorContrastingToToolbar(key, mixer, SkColorSetRGB(0x4A, 0x39, 0x00),
                                    SkColorSetRGB(0xF1, 0xC0, 0x0F))};
  mixer[kColorAppMenuHighlightSeverityHigh] = {
      PickColorContrastingToToolbar(key, mixer, SkColorSetRGB(0x7D, 0x00, 0x1A),
                                    SkColorSetRGB(0xFF, 0xB3, 0xB2))};

  if (key.custom_theme) {
    return;
  }

  mixer[kColorToolbar] = {ui::kColorSysBase};
  mixer[kColorDownloadToolbarButtonActive] = {nala::kColorIconInteractive};
  mixer[kColorDownloadToolbarButtonRingBackground] = {SkColorSetA(
      mixer.GetResultColor(kColorDownloadToolbarButtonActive), 0.3 * 255)};

  key.color_mode == ui::ColorProviderKey::ColorMode::kDark
      ? AddChromeDarkThemeColorMixer(provider, key)
      : AddChromeLightThemeColorMixer(provider, key);
}

void AddBraveLightThemeColorMixer(ui::ColorProvider* provider,
                                  const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorForTest] = {kLightColorForTest};

  mixer[kColorBookmarkBarInstructionsText] = {
      PickColorContrastingToToolbar(key, mixer, SkColorSetRGB(0x49, 0x50, 0x57),
                                    SkColorSetRGB(0xFF, 0xFF, 0xFF))};
  mixer[kColorMenuItemSubText] = {SkColorSetRGB(0x86, 0x8E, 0x96)};
  // It's "Themeable/Blue/10" but leo/color.h doesn't have it.
  mixer[kColorSearchConversionBannerTypeBackground] = {
      SkColorSetRGB(0xEA, 0xF1, 0xFF)};
  mixer[kColorSearchConversionBannerTypeDescText] = {
      SkColorSetRGB(0x2E, 0x30, 0x39)};
  mixer[kColorSearchConversionBannerTypeBackgroundBorder] = {
      SkColorSetRGB(0xE2, 0xE3, 0xF8)};
  mixer[kColorSearchConversionBannerTypeBackgroundBorderHovered] = {
      SkColorSetRGB(0x83, 0x89, 0xE0)};
  mixer[kColorSearchConversionBannerTypeBackgroundGradientFrom] = {
      SkColorSetARGB(104, 0xFF, 0xFF, 0xFF)};
  mixer[kColorSearchConversionBannerTypeBackgroundGradientTo] = {
      SkColorSetRGB(0xEF, 0xEF, 0xFB)};

  mixer[kColorDialogDontAskAgainButton] = {SkColorSetRGB(0x86, 0x8E, 0x96)};
  mixer[kColorDialogDontAskAgainButtonHovered] = {
      SkColorSetRGB(0x49, 0x50, 0x57)};
  mixer[kColorSidebarAddBubbleHeaderText] = {SkColorSetRGB(0x17, 0x17, 0x1F)};
  mixer[kColorSidebarAddBubbleItemTextBackgroundHovered] = {
      SkColorSetRGB(0x4C, 0x54, 0xD2)};
  mixer[kColorSidebarAddBubbleItemTextHovered] = {
      SkColorSetRGB(0xF0, 0xF2, 0xFF)};
  mixer[kColorSidebarAddBubbleItemTextNormal] = {
      SkColorSetRGB(0x21, 0x25, 0x29)};
  mixer[kColorSidebarArrowBackgroundHovered] = {kColorToolbarInkDropHover};
  mixer[kColorSidebarSeparator] = {SkColorSetRGB(0xE6, 0xE8, 0xF5)};

  mixer[kColorSidebarButtonBase] = {kColorToolbarButtonIcon};
  if (!HasCustomToolbarColor(key)) {
    mixer[kColorSidebarButtonPressed] = {kColorToolbarButtonActivated};
  }

  mixer[kColorSidebarAddButtonDisabled] = {PickColorContrastingToToolbar(
      key, mixer, SkColorSetARGB(0x66, 0x49, 0x50, 0x57),
      SkColorSetARGB(0x66, 0xC2, 0xC4, 0xCF))};

  mixer[kColorSidebarArrowDisabled] = {PickColorContrastingToToolbar(
      key, mixer, SkColorSetARGB(0x8A, 0x49, 0x50, 0x57),
      SkColorSetARGB(0x8A, 0xAE, 0xB1, 0xC2))};
  mixer[kColorSidebarArrowNormal] = {kColorSidebarButtonBase};
  mixer[kColorSidebarItemDragIndicator] = {
      PickColorContrastingToToolbar(key, mixer, SkColorSetRGB(0x21, 0x25, 0x29),
                                    SkColorSetRGB(0xC2, 0xC4, 0xCF))};
  mixer[kColorWebDiscoveryInfoBarBackground] = {
      SkColorSetRGB(0xFF, 0xFF, 0xFF)};
  mixer[kColorWebDiscoveryInfoBarMessage] = {SkColorSetRGB(0x1D, 0x1F, 0x25)};
  mixer[kColorWebDiscoveryInfoBarLink] = {SkColorSetRGB(0x4C, 0x54, 0xD2)};
  mixer[kColorWebDiscoveryInfoBarNoThanks] = {SkColorSetRGB(0x6B, 0x70, 0x84)};
  mixer[kColorWebDiscoveryInfoBarClose] = {SkColorSetRGB(0x6B, 0x70, 0x84)};

  // Color for download button when all completed and button needs user
  // interaction.
  mixer[kColorBraveDownloadToolbarButtonActive] = {
      SkColorSetRGB(0x5F, 0x5C, 0xF1)};

  mixer[kColorLocationBarHoveredShadow] = {
      SkColorSetA(SK_ColorBLACK, 0.07 * 255)};

  // Colors for HelpBubble. IDs are defined in
  // chrome/browser/ui/color/chrome_color_id.h
  mixer[kColorFeaturePromoBubbleBackground] = {SK_ColorWHITE};
  mixer[kColorFeaturePromoBubbleForeground] = {SkColorSetRGB(0x42, 0x45, 0x52)};
  mixer[kColorFeaturePromoBubbleCloseButtonInkDrop] = {
      kColorToolbarInkDropHover};

  mixer[kColorTabGroupBackgroundAlpha] = {
      SkColorSetA(SK_ColorBLACK, 0.15 * 255)};

  mixer[kColorBraveAppMenuAccentColor] = {SkColorSetRGB(0xDF, 0xE1, 0xFF)};
}

void AddBraveDarkThemeColorMixer(ui::ColorProvider* provider,
                                 const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorForTest] = {kDarkColorForTest};

  mixer[kColorBookmarkBarInstructionsText] = {
      PickColorContrastingToToolbar(key, mixer, SkColorSetRGB(0x49, 0x50, 0x57),
                                    SkColorSetRGB(0xFF, 0xFF, 0xFF))};
  mixer[kColorMenuItemSubText] = {SkColorSetRGB(0x84, 0x88, 0x9C)};
  mixer[kColorSearchConversionBannerTypeDescText] = {
      SkColorSetRGB(0xE2, 0xE3, 0xE7)};
  mixer[kColorSearchConversionBannerTypeBackgroundBorder] = {
      SkColorSetRGB(0x1F, 0x25, 0x7A)};
  mixer[kColorSearchConversionBannerTypeBackgroundBorderHovered] = {
      SkColorSetRGB(0x5F, 0x67, 0xD7)};
  mixer[kColorSearchConversionBannerTypeBackgroundGradientFrom] = {
      SkColorSetARGB(104, 0x17, 0x19, 0x1E)};
  mixer[kColorSearchConversionBannerTypeBackgroundGradientTo] = {
      SkColorSetARGB(104, 0x1F, 0x25, 0x7A)};
  mixer[kColorDialogDontAskAgainButton] = {SkColorSetRGB(0x84, 0x88, 0x9C)};
  mixer[kColorDialogDontAskAgainButtonHovered] = {
      SkColorSetRGB(0xC2, 0xC4, 0xCF)};
  mixer[kColorSidebarAddBubbleHeaderText] = {SkColorSetRGB(0xF0, 0xF0, 0xFF)};
  mixer[kColorSidebarAddBubbleItemTextBackgroundHovered] = {
      SkColorSetRGB(0x4C, 0x54, 0xD2)};
  mixer[kColorSidebarAddBubbleItemTextHovered] = {
      SkColorSetRGB(0xF0, 0xF0, 0xFF)};
  mixer[kColorSidebarAddBubbleItemTextNormal] = {
      SkColorSetRGB(0xF0, 0xF0, 0xFF)};
  mixer[kColorSidebarArrowBackgroundHovered] = {kColorToolbarInkDropHover};
  mixer[kColorSidebarSeparator] = {SkColorSetRGB(0x5E, 0x61, 0x75)};

  mixer[kColorSidebarButtonBase] = {kColorToolbarButtonIcon};
  if (!HasCustomToolbarColor(key)) {
    mixer[kColorSidebarButtonPressed] = {kColorToolbarButtonActivated};
  }
  mixer[kColorSidebarAddButtonDisabled] = {PickColorContrastingToToolbar(
      key, mixer, SkColorSetARGB(0x66, 0x49, 0x50, 0x57),
      SkColorSetARGB(0x66, 0xC2, 0xC4, 0xCF))};
  mixer[kColorSidebarArrowDisabled] = {PickColorContrastingToToolbar(
      key, mixer, SkColorSetARGB(0x8A, 0x49, 0x50, 0x57),
      SkColorSetARGB(0x8A, 0xAE, 0xB1, 0xC2))};
  mixer[kColorSidebarArrowNormal] = {kColorSidebarButtonBase};
  mixer[kColorSidebarItemDragIndicator] = {
      PickColorContrastingToToolbar(key, mixer, SkColorSetRGB(0x21, 0x25, 0x29),
                                    SkColorSetRGB(0xC2, 0xC4, 0xCF))};
  mixer[kColorWebDiscoveryInfoBarBackground] = {
      SkColorSetRGB(0x1A, 0x1C, 0x22)};
  mixer[kColorWebDiscoveryInfoBarMessage] = {SkColorSetRGB(0xFF, 0xFF, 0xFF)};
  mixer[kColorWebDiscoveryInfoBarLink] = {SkColorSetRGB(0xA6, 0xAB, 0xEC)};
  mixer[kColorWebDiscoveryInfoBarNoThanks] = {
      SkColorSetARGB(0xBF, 0xEC, 0xEF, 0xF2)};
  mixer[kColorWebDiscoveryInfoBarClose] = {
      SkColorSetARGB(0xBF, 0x8C, 0x90, 0xA1)};

  mixer[kColorBraveDownloadToolbarButtonActive] = {
      SkColorSetRGB(0x87, 0x84, 0xF4)};

  mixer[kColorLocationBarHoveredShadow] = {
      SkColorSetA(SK_ColorBLACK, 0.4 * 255)};

  // Colors for HelpBubble. IDs are defined in
  // chrome/browser/ui/color/chrome_color_id.h
  mixer[kColorFeaturePromoBubbleBackground] = {SkColorSetRGB(0x12, 0x13, 0x16)};
  mixer[kColorFeaturePromoBubbleForeground] = {SkColorSetRGB(0xC6, 0xC8, 0xD0)};
  mixer[kColorFeaturePromoBubbleCloseButtonInkDrop] = {
      kColorToolbarInkDropHover};

  mixer[kColorTabGroupBackgroundAlpha] = {
      SkColorSetA(SK_ColorBLACK, 0.25 * 255)};

  mixer[kColorBraveAppMenuAccentColor] = {SkColorSetRGB(0x37, 0x2C, 0xBF)};
}

// Handling dark or light theme on normal profile.
void AddBraveThemeColorMixer(ui::ColorProvider* provider,
                             const ui::ColorProviderKey& key) {
  AddBraveColorMixerForAllThemes(provider, key);

  key.color_mode == ui::ColorProviderKey::ColorMode::kDark
      ? AddBraveDarkThemeColorMixer(provider, key)
      : AddBraveLightThemeColorMixer(provider, key);
#if BUILDFLAG(ENABLE_BRAVE_VPN)
  AddBraveVpnColorMixer(provider, key);
#endif
#if BUILDFLAG(ENABLE_SPEEDREADER)
  AddBraveSpeedreaderColorMixer(provider, key);
#endif

  auto& mixer = provider->AddMixer();
  mixer[kColorIconBase] = {nala::kColorIconDefault};
  mixer[kColorBookmarkBarInstructionsLink] = {nala::kColorTextInteractive};
  mixer[kColorSearchConversionBannerTypeBackground] = {nala::kColorBlue10};
  mixer[kColorSidebarPanelHeaderSeparator] = {nala::kColorDividerSubtle};
  mixer[kColorSearchConversionCloseButton] = {nala::kColorIconDefault};
  mixer[kColorSidebarPanelHeaderBackground] = {nala::kColorContainerBackground};
  mixer[kColorSidebarPanelHeaderTitle] = {nala::kColorTextPrimary};
  mixer[kColorSidebarPanelHeaderButton] = {nala::kColorIconDefault};
  mixer[kColorSidebarPanelHeaderButtonHovered] = {nala::kColorNeutral60};
  mixer[kColorSidebarAddBubbleBackground] = {nala::kColorContainerBackground};
  mixer[kColorDownloadShelfButtonText] = {nala::kColorTextPrimary};

  if (!HasCustomToolbarColor(key)) {
    mixer[kColorToolbarButtonActivated] = {nala::kColorIconInteractive};
  }

#if BUILDFLAG(ENABLE_BRAVE_WAYBACK_MACHINE)
  mixer[kColorWaybackMachineURLLoaded] = {
      nala::kColorSystemfeedbackSuccessIcon};
  mixer[kColorWaybackMachineURLNotAvailable] = {
      nala::kColorSystemfeedbackErrorIcon};
#endif

  mixer[kColorBraveExtensionMenuIcon] = {nala::kColorIconInteractive};

#if BUILDFLAG(ENABLE_PLAYLIST_WEBUI)
  if (base::FeatureList::IsEnabled(playlist::features::kPlaylist)) {
    playlist::AddThemeColorMixer(provider, key);
  }
#endif
}

void AddBravePrivateThemeColorMixer(ui::ColorProvider* provider,
                                    const ui::ColorProviderKey& key) {
  AddBraveDarkThemeColorMixer(provider, key);

  // Add private theme specific brave colors here.
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorForTest] = {kPrivateColorForTest};

  mixer[kColorToolbarButtonActivated] = {SkColorSetRGB(0x7C, 0x91, 0xFF)};
  mixer[kColorSidebarButtonPressed] = {kColorToolbarButtonActivated};

  // |key.color_mode| always dark as we use dark native theme for
  // private/tor/guest profile. See BraveBrowserFrame::GetNativeTheme().
  // Exceptionally, below side panel header colors should be brave theme
  // specific because side panel header colors should be aligned with
  // side panel contents.
  const bool is_dark = dark_mode::GetActiveBraveDarkModeType() ==
                       dark_mode::BraveDarkModeType::BRAVE_DARK_MODE_TYPE_DARK;
  // These colors should be nala colors, but we don't necessarily match the
  // theme in the browser, so we hardcode light/dark values.
  // kColorDividerSubtle
  mixer[kColorSidebarPanelHeaderSeparator] = {
      is_dark ? nala::kColorPrimitiveNeutral20
              : nala::kColorPrimitiveNeutral90};
  // kColorContainerBackground
  mixer[kColorSidebarPanelHeaderBackground] = {
      is_dark ? nala::kColorPrimitiveNeutral5
              : nala::kColorPrimitiveNeutral100};
  mixer[kColorSidebarPanelHeaderTitle] = {is_dark
                                              ? nala::kColorPrimitiveNeutral90
                                              : nala::kColorPrimitiveNeutral10};
  // kColorIconDefault
  mixer[kColorSidebarPanelHeaderButton] = {
      is_dark ? nala::kColorPrimitiveNeutral90
              : nala::kColorPrimitiveNeutral10};
  // kColorNeutral60
  mixer[kColorSidebarPanelHeaderButtonHovered] = {
      is_dark ? nala::kColorPrimitiveNeutral80
              : nala::kColorPrimitiveNeutral25};
}

void AddBraveTorThemeColorMixer(ui::ColorProvider* provider,
                                const ui::ColorProviderKey& key) {
  AddBravePrivateThemeColorMixer(provider, key);
  AddChromeDarkThemeColorMixer(provider, key);

  // Add tor theme specific brave colors here.
}

void AddPrivateThemeColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderKey& key) {
  AddBravePrivateThemeColorMixer(provider, key);
  AddChromeDarkThemeColorMixer(provider, key);

  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorBookmarkBarForeground] = {SkColorSetRGB(0xFF, 0xFF, 0xFF)};
  mixer[kColorLocationBarFocusRing] = {SkColorSetRGB(0xC6, 0xB3, 0xFF)};
  mixer[kColorNewTabButtonBackgroundFrameActive] = {ui::kColorFrameActive};
  mixer[kColorNewTabButtonBackgroundFrameInactive] = {ui::kColorFrameInactive};
  mixer[kColorNewTabPageBackground] = {kPrivateFrame};
  mixer[kColorTabBackgroundActiveFrameActive] = {
      nala::kColorPrimitivePrivateWindow20};
  mixer[kColorTabBackgroundActiveFrameInactive] = {
      kColorTabBackgroundActiveFrameActive};
  mixer[kColorTabBackgroundInactiveFrameActive] = {ui::kColorFrameActive};
  mixer[kColorTabBackgroundInactiveFrameInactive] = {ui::kColorFrameInactive};

  // TODO(simonhong): Get color from leo when it's available.
  mixer[kColorTabForegroundActiveFrameActive] = {
      SkColorSetRGB(0xF5, 0xF3, 0xFF)};
  mixer[kColorTabForegroundActiveFrameInactive] = {
      SkColorSetRGB(0xCC, 0xBE, 0xFE)};
  mixer[kColorTabForegroundInactiveFrameActive] = {
      SkColorSetRGB(0xCC, 0xBE, 0xFE)};
  mixer[kColorToolbar] = {nala::kColorPrimitivePrivateWindow10};
  mixer[kColorToolbarButtonIcon] = {nala::kColorPrimitivePrivateWindow70};
  mixer[kColorToolbarButtonIconInactive] = {
      ui::SetAlpha(kColorToolbarButtonIcon, kBraveDisabledControlAlpha)};
  mixer[ui::kColorFrameActive] = {kPrivateFrame};
  mixer[ui::kColorFrameInactive] = {kPrivateFrame};
}

void AddTorThemeColorMixer(ui::ColorProvider* provider,
                           const ui::ColorProviderKey& key) {
  AddBraveTorThemeColorMixer(provider, key);

  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorLocationBarFocusRing] = {SkColorSetRGB(0xCF, 0xAB, 0xE2)};
  mixer[kColorNewTabButtonBackgroundFrameActive] = {ui::kColorFrameActive};
  mixer[kColorNewTabButtonBackgroundFrameInactive] = {ui::kColorFrameInactive};
  mixer[kColorNewTabPageBackground] = {kPrivateTorFrame};
  mixer[kColorTabBackgroundActiveFrameActive] = {
      nala::kColorPrimitiveTorWindow20};
  mixer[kColorTabBackgroundActiveFrameInactive] = {
      kColorTabBackgroundActiveFrameActive};
  mixer[kColorTabBackgroundInactiveFrameActive] = {ui::kColorFrameActive};
  mixer[kColorTabBackgroundInactiveFrameInactive] = {ui::kColorFrameInactive};

  // TODO(simonhong): Get color from leo when it's available.
  mixer[kColorTabForegroundActiveFrameActive] = {
      SkColorSetRGB(0xFA, 0xF3, 0xFF)};
  mixer[kColorTabForegroundActiveFrameInactive] = {
      SkColorSetRGB(0xE3, 0xB3, 0xFF)};
  mixer[kColorTabForegroundInactiveFrameActive] = {
      SkColorSetRGB(0xE3, 0xB3, 0xFF)};
  mixer[kColorToolbar] = {nala::kColorPrimitiveTorWindow10};
  mixer[kColorToolbarButtonIcon] = {nala::kColorPrimitiveTorWindow70};
  mixer[kColorToolbarButtonIconInactive] = {
      ui::SetAlpha(kColorToolbarButtonIcon, kBraveDisabledControlAlpha)};
  mixer[ui::kColorFrameActive] = {kPrivateTorFrame};
  mixer[ui::kColorFrameInactive] = {kPrivateTorFrame};
  mixer[kColorTabDividerFrameInactive] = {SkColorSetRGB(0x5A, 0x53, 0x66)};
  mixer[kColorTabDividerFrameActive] = {SkColorSetRGB(0x5A, 0x53, 0x66)};
}

void AddBraveOmniboxPrivateThemeColorMixer(ui::ColorProvider* provider,
                                           const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorToolbarBackgroundSubtleEmphasis] = {kPrivateFrame};
  mixer[kColorToolbarBackgroundSubtleEmphasisHovered] = {
      kColorToolbarBackgroundSubtleEmphasis};

  mixer[kColorOmniboxResultsBackground] = {kPrivateFrame};
  if (IsHighContrast()) {
    mixer[kColorOmniboxResultsBackground] = {
        color_utils::HSLShift(kPrivateFrame, {-1, -1, 0.45})};
  }

  AddOmniboxHoverSelect(mixer);

  mixer[kColorOmniboxIconBackground] = {SK_ColorTRANSPARENT};
}

void AddBraveOmniboxColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  if (!key.custom_theme) {
    mixer[kColorToolbarContentAreaSeparator] = {nala::kColorDividerSubtle};
  }
  mixer[kColorBraveOmniboxResultViewSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorOmniboxResultsUrl] = {nala::kColorTextInteractive};
  // We use a shadow for the Omnibox hover effect, rather than a color change.
  mixer[kColorLocationBarBackgroundHovered] = {kColorLocationBarBackground};
  mixer[kColorOmniboxIconHover] = {
      ui::SetAlpha(kColorOmniboxText, std::ceil(0.10f * 255.0f))};

  // We don't use bg color for location icon view.
  mixer[kColorOmniboxIconBackground] = {SK_ColorTRANSPARENT};
}

void AddBravifiedTabStripColorMixer(ui::ColorProvider* provider,
                                    const ui::ColorProviderKey& key) {
  if (key.custom_theme) {
    return;
  }

  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorNewTabButtonFocusRing] = {ui::kColorFocusableBorderFocused};
  mixer[kColorTabBackgroundActiveFrameActive] = {kColorToolbar};
  mixer[kColorTabBackgroundActiveFrameInactive] = {
      kColorTabBackgroundActiveFrameActive};
  mixer[kColorTabDividerFrameActive] =
      ui::AlphaBlend({nala::kColorDividerStrong},
                     {kColorTabBackgroundInactiveFrameActive}, 0.75 * 0xff);
  mixer[kColorTabDividerFrameInactive] = {kColorTabDividerFrameActive};

  if (key.color_mode == ui::ColorProviderKey::ColorMode::kDark) {
    mixer[kColorTabBackgroundActiveFrameActive] = {
        nala::kColorDesktopbrowserTabbarActiveTabHorizontal};
    mixer[kColorTabBackgroundInactiveHoverFrameActive] = {
        nala::kColorDesktopbrowserTabbarHoverTabHorizontal};
  }
}
