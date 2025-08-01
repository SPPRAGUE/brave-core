/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import { ExternalWallet, ExternalWalletProvider } from '../../shared/lib/external_wallet'
import { ProviderPayoutStatus } from '../../shared/lib/provider_payout_status'
import { Optional } from '../../shared/lib/optional'

interface EmbedderInfo {
  isBubble: boolean
  isAutoResizeBubble: boolean
  platform: 'android' | 'desktop'
  animatedBackgroundEnabled: boolean
}

export type EnableRewardsResult =
  'success' |
  'wallet-generation-disabled' |
  'country-already-declared' |
  'unexpected-error'

export interface AvailableCountryInfo {
  countryCodes: string[]
  defaultCountryCode: string
}

export type AdType =
  'new-tab-page' |
  'notification' |
  'search-result'

export interface AdsInfo {
  browserUpgradeRequired: boolean
  isSupportedRegion: boolean
  adsEnabled: Record<AdType, boolean>
  adTypesReceivedThisMonth: Record<AdType, number>
  minEarningsPreviousMonth: number
  nextPaymentDate: number
  notificationAdsPerHour: number
  shouldAllowSubdivisionTargeting: boolean
  currentSubdivision: string
  availableSubdivisions: Array<{ code: string, name: string }>
  autoDetectedSubdivision: string
}

export type AdLikeStatus = 'liked' | 'disliked' | ''

export interface AdsHistoryItem {
  id: string
  type: AdType
  createdAt: number
  name: string
  text: string
  domain: string
  url: string
  likeStatus: AdLikeStatus
  inappropriate: boolean
}

export interface RewardsParameters {
  tipChoices: number[]
  rate: number
  walletProviderRegions: Record<string, { allow: string[], block: string[] }>
  payoutStatus: Record<string, ProviderPayoutStatus>
}

export type ConnectExternalWalletResult =
  'success' |
  'device-limit-reached' |
  'flagged-wallet' |
  'kyc-required' |
  'mismatched-countries' |
  'mismatched-provider-accounts' |
  'provider-unavailable' |
  'region-not-supported' |
  'request-signature-verification-error' |
  'unexpected-error' |
  'uphold-bat-not-allowed' |
  'uphold-insufficient-capabilities' |
  'uphold-transaction-verification-failure'

export type CreatorPlatform =
  '' |
  'twitter' |
  'youtube' |
  'twitch' |
  'reddit' |
  'vimeo' |
  'github'

export interface CreatorSite {
  id: string
  icon: string
  name: string
  url: string
  platform: CreatorPlatform
}

export interface CreatorBanner {
  title: string
  description: string
  background: string
  web3URL: string
}

export interface CreatorInfo {
  site: CreatorSite
  banner: CreatorBanner
  supportedWalletProviders: ExternalWalletProvider[]
}

export interface RecurringContribution {
  site: CreatorSite
  amount: number
  nextContributionDate: number
}

export interface CaptchaInfo {
  url: string
  maxAttemptsExceeded: boolean
}

export type NotificationType =
  'monthly-tip-completed' |
  'external-wallet-disconnected'

export interface Notification {
  type: NotificationType
  id: string
  timeStamp: number
}

export interface ExternalWalletDisconnectedNotification extends Notification {
  type: 'external-wallet-disconnected'
  provider: ExternalWalletProvider
}

export type NotificationActionType =
  'open-link' |
  'reconnect-external-wallet'

export interface NotificationAction {
  type: NotificationActionType
}

export interface OpenLinkNotificationAction extends NotificationAction {
  type: 'open-link'
  url: string
}

export { ExternalWallet, ExternalWalletProvider }

export interface UICardItem {
  title: string
  description: string
  url: string
  thumbnail: string
}

export interface UICardBanner {
  image: string
  url: string
}

export interface UICard {
  name: string
  title: string
  section: string
  order: number
  banner: UICardBanner | null | undefined
  items: UICardItem[]
}

export interface AppState {
  loading: boolean
  openTime: number
  isUnsupportedRegion: boolean
  embedder: EmbedderInfo
  paymentId: string
  countryCode: string
  externalWallet: ExternalWallet | null
  externalWalletProviders: ExternalWalletProvider[]
  balance: Optional<number>
  tosUpdateRequired: boolean
  selfCustodyProviderInvites: ExternalWalletProvider[]
  selfCustodyInviteDismissed: boolean
  adsInfo: AdsInfo | null
  recurringContributions: RecurringContribution[]
  rewardsParameters: RewardsParameters | null
  currentCreator: CreatorInfo | null
  captchaInfo: CaptchaInfo | null
  notifications: Notification[]
  cards: UICard[] | null
}

export function defaultState(): AppState {
  return {
    loading: true,
    openTime: Date.now(),
    isUnsupportedRegion: false,
    embedder: {
      isBubble: false,
      isAutoResizeBubble: false,
      platform: 'desktop',
      animatedBackgroundEnabled: false
    },
    paymentId: '',
    countryCode: '',
    externalWallet: null,
    externalWalletProviders: [],
    balance: new Optional(),
    tosUpdateRequired: false,
    selfCustodyProviderInvites: [],
    selfCustodyInviteDismissed: false,
    adsInfo: null,
    recurringContributions: [],
    rewardsParameters: null,
    currentCreator: null,
    captchaInfo: null,
    notifications: [],
    cards: null
  }
}
