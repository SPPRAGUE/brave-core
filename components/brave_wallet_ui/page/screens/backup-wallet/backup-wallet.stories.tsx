// Copyright (c) 2022 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at https://mozilla.org/MPL/2.0/.

import * as React from 'react'

// mocks
import { mockedMnemonic } from '../../../stories/mock-data/user-accounts'

// components
import { WalletPageStory } from '../../../stories/wrappers/wallet-page-story-wrapper'
import { BackupWalletRoutes } from './backup-wallet.routes'

export const _BackupWallet = {
  render: () => {
    return (
      <WalletPageStory pageStateOverride={{ mnemonic: mockedMnemonic }}>
        <BackupWalletRoutes />
      </WalletPageStory>
    )
  },
}

export default {
  title: 'Wallet/Desktop/Screens/Backup Wallet',
  component: BackupWalletRoutes,
}
