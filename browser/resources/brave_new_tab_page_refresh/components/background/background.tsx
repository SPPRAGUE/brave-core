/* Copyright (c) 2025 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import * as React from 'react'

import { NewTabPageAdEventType, SponsoredImageBackground } from '../../state/background_state'
import { openLink } from '../common/link'
import { loadImage } from '../../lib/image_loader'

import {
  useBackgroundState,
  useCurrentBackground,
  useBackgroundActions } from '../../context/background_context'

import { style } from './background.style'

export function Background() {
  const actions = useBackgroundActions()
  const currentBackground = useCurrentBackground()

  function renderBackground() {
    if (!currentBackground) {
      return <ColorBackground colorValue='transparent' />
    }

    switch (currentBackground.type) {
      case 'brave':
      case 'custom':
        return <ImageBackground url={currentBackground.imageUrl} />
      case 'sponsored-image':
        return (
          <ImageBackground
            url={currentBackground.imageUrl}
            onLoadError={actions.notifySponsoredImageLoadError}
          />
        )
      case 'sponsored-rich-media':
        return <SponsoredRichMediaBackground background={currentBackground} />
      case 'color':
        return <ColorBackground colorValue={currentBackground.cssValue} />
    }
  }

  return (
    <div data-css-scope={style.scope}>
      {renderBackground()}
    </div>
  )
}

function ColorBackground(props: { colorValue: string }) {
  React.useEffect(() => {
    setBackgroundVariable(props.colorValue)
  }, [props.colorValue])

  return <div className='color-background' />
}

function setBackgroundVariable(value: string) {
  if (value) {
    document.body.style.setProperty('--ntp-background', value)
  } else {
    document.body.style.removeProperty('--ntp-background')
  }
}

function ImageBackground(props: { url: string, onLoadError?: () => void }) {
  // In order to avoid a "flash-of-unloaded-image", load the image in the
  // background and only update the background CSS variable when the image has
  // finished loading.
  React.useEffect(() => {
    loadImage(props.url).then((loaded) => {
      if (loaded) {
        setBackgroundVariable(`url(${CSS.escape(props.url)})`)
      } else if (props.onLoadError) {
        props.onLoadError()
      }
    })
  }, [props.url])

  return <div className='image-background' />
}

function SponsoredRichMediaBackground(
  props: { background: SponsoredImageBackground }
) {
  const actions = useBackgroundActions()
  const sponsoredRichMediaBaseUrl =
    useBackgroundState((s) => s.sponsoredRichMediaBaseUrl)

  return (
    <IframeBackground
      url={props.background.imageUrl}
      expectedOrigin={new URL(sponsoredRichMediaBaseUrl).origin}
      onMessage={(data) => {
        const eventType = getRichMediaEventType(data)
        if (eventType) {
          actions.notifySponsoredRichMediaEvent(eventType)
        }
        if (eventType === NewTabPageAdEventType.kClicked) {
          const url = props.background.logo?.destinationUrl
          if (url) {
            openLink(url)
          }
        }
      }}
    />
  )
}

function getRichMediaEventType(data: any): NewTabPageAdEventType | null {
  if (!data || data.type !== 'richMediaEvent') {
    return null
  }
  const value = String(data.value || '')
  switch (value) {
    case 'click': return NewTabPageAdEventType.kClicked
    case 'interaction': return NewTabPageAdEventType.kInteraction
    case 'mediaPlay': return NewTabPageAdEventType.kMediaPlay
    case 'media25': return NewTabPageAdEventType.kMedia25
    case 'media100': return NewTabPageAdEventType.kMedia100
  }
  return null
}

interface IframeBackgroundProps {
  url: string
  expectedOrigin: string
  onMessage: (data: unknown) => void
}

function IframeBackground(props: IframeBackgroundProps) {
  const iframeRef = React.useRef<HTMLIFrameElement>(null)
  const [contentLoaded, setContentLoaded] = React.useState(false)

  React.useEffect(() => {
    function listener(event: MessageEvent) {
      if (!event.origin || event.origin !== props.expectedOrigin) {
        return
      }
      if (!event.source || event.source !== iframeRef.current?.contentWindow) {
        return
      }
      props.onMessage(event.data)
    }

    window.addEventListener('message', listener)
    return () => { window.removeEventListener('message', listener) }
  }, [props.expectedOrigin, props.onMessage])

  return (
    <iframe
      ref={iframeRef}
      className={contentLoaded ? '' : 'loading'}
      src={props.url}
      sandbox='allow-scripts allow-same-origin'
      allow={allowNoneList([
        'accelerometer',
        'ambient-light-sensor',
        'camera',
        'display-capture',
        'document-domain',
        'fullscreen',
        'geolocation',
        'gyroscope',
        'magnetometer',
        'microphone',
        'midi',
        'payment',
        'publickey-credentials-get',
        'usb'
      ])}
      onLoad={() => setContentLoaded(true)}
    />
  )
}

function allowNoneList(items: string[]) {
  return items.map((key) => `${key} 'none'`).join('; ')
}
