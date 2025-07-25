// Copyright (c) 2022 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at https://mozilla.org/MPL/2.0/.

import * as React from 'react'

import * as S from './style'
import Button from '@brave/leo/react/button'
import { P3APhase, WelcomeBrowserProxyImpl } from '../../api/welcome_browser_proxy'
import { getLocale, formatLocale } from '$web-common/locale'

interface InputCheckboxProps {
  onChange?: (e: React.ChangeEvent<HTMLInputElement>) => void
  children: React.ReactNode
  id?: string
  isChecked: boolean
}

const changeSettingsNote = formatLocale('braveWelcomeChangeSettingsNote', {
  $1: content => <a href="brave://settings/privacy" onClick={() => {
    WelcomeBrowserProxyImpl.getInstance().openSettingsPage()
  }}>
    {content}
  </a>
})

const readPrivacyPolicy = formatLocale('braveWelcomePrivacyPolicyNote', {
  $1: content => <a href='https://brave.com/privacy/browser' target='_blank'>
    {content}
  </a>
})

const diagnosticReportsLabel = formatLocale('braveWelcomeSendReportsLabel', {
  $1: content => <a href='https://support.brave.com/hc/en-us/articles/360017905872-How-do-I-enable-or-disable-automatic-crash-reporting' target='_blank'>
    {content}
  </a>
})

const braveProductUsageDataLabel = formatLocale('braveWelcomeSendInsightsLabel', {
  $1: content => <a href='https://support.brave.com/hc/en-us/articles/9140465918093-What-is-P3A-in-Brave-' target='_blank'>
    {content}
  </a>
})

function InputCheckbox(props: InputCheckboxProps) {
  return (
    <label className="item">
      <input
        type="checkbox"
        checked={props.isChecked}
        onChange={props.onChange}
      />
      <div>
        {props.children}
      </div>
    </label>
  )
}

function HelpImprove() {
  const [isMetricsReportingEnabled, setMetricsReportingEnabled] = React.useState(true)
  const [isP3AEnabled, setP3AEnabled] = React.useState(true)
  const [completeURLPromise] = React.useState(() => {
    return WelcomeBrowserProxyImpl.getInstance().getWelcomeCompleteURL()
  })

  const handleP3AChange = () => {
    setP3AEnabled(!isP3AEnabled)
  }

  const handleMetricsReportingChange = () => {
    setMetricsReportingEnabled(!isMetricsReportingEnabled)
  }

  const handleFinish = () => {
    WelcomeBrowserProxyImpl.getInstance().setP3AEnabled(isP3AEnabled)
    WelcomeBrowserProxyImpl.getInstance().setMetricsReportingEnabled(isMetricsReportingEnabled)
    WelcomeBrowserProxyImpl.getInstance().recordP3A(P3APhase.Finished)
    completeURLPromise.then((url) => {
      window.open(url || 'chrome://newtab', '_self', 'noopener')
    })
  }

  return (
    <S.MainBox>
      <div className="view-header-box">
        <div className="view-details">
          <h1 className="view-title">{getLocale('braveWelcomeHelpImproveBraveTitle')}</h1>
        </div>
      </div>
      <S.Grid>
        <div className="list">
          <InputCheckbox
            id="metrics"
            onChange={handleMetricsReportingChange}
            isChecked={isMetricsReportingEnabled}
          >
            {diagnosticReportsLabel}
          </InputCheckbox>
          <InputCheckbox
            id="p3a"
            onChange={handleP3AChange}
            isChecked={isP3AEnabled}
          >
            {braveProductUsageDataLabel}
          </InputCheckbox>
        </div>
      </S.Grid>
      <S.ActionBox>
        <div className="box-center">
          <Button
            kind="filled"
            onClick={handleFinish}
            size="large"
          >
            {getLocale('braveWelcomeFinishButtonLabel')}
          </Button>
          <S.FootNote>
            {changeSettingsNote}
            <span>
              {readPrivacyPolicy}
            </span>
          </S.FootNote>
        </div>
      </S.ActionBox>
    </S.MainBox>
  )
}

export default HelpImprove
