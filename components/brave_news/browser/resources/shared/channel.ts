// Copyright (c) 2023 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import { getLocale } from "$web-common/locale"

const getChannelStringId = (channelName: string) => 'BRAVE_NEWS_CHANNEL_'
  + channelName.toUpperCase()
    .split(' ')
    .join('_')

export const getTranslatedChannelName = (channelName: string) => {
  if (!channelName) return ''
  try {
    return getLocale(getChannelStringId(channelName))
  } catch (err) {
    console.error(`Couldn't find translation for channel '${channelName}'`)
    return channelName
  }
}
