/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/ai_chat/content/browser/full_screenshotter.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "brave/components/ai_chat/content/browser/pdf_utils.h"
#include "brave/components/ai_chat/core/browser/utils.h"
#include "components/paint_preview/browser/compositor_utils.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/common/recording_map.h"
#include "content/public/browser/render_widget_host_view.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"

namespace ai_chat {

FullScreenshotter::FullScreenshotter()
    : paint_preview::PaintPreviewBaseService(
          /*file_mixin=*/nullptr,  // in-memory captures
          /*policy=*/nullptr,      // all content is deemed amenable
          /*is_off_the_record=*/true),
      paint_preview_compositor_service_(nullptr,
                                        base::OnTaskRunnerDeleter(nullptr)),
      paint_preview_compositor_client_(nullptr,
                                       base::OnTaskRunnerDeleter(nullptr)) {}

FullScreenshotter::~FullScreenshotter() = default;

FullScreenshotter::PendingScreenshots::PendingScreenshots() = default;
FullScreenshotter::PendingScreenshots::~PendingScreenshots() = default;

void FullScreenshotter::CaptureScreenshots(
    const raw_ptr<content::WebContents> web_contents,
    CaptureScreenshotsCallback callback) {
  if (!web_contents) {
    std::move(callback).Run(
        base::unexpected("The given web contents is no longer valid"));
    return;
  }
  if (IsPdf(web_contents)) {
    std::move(callback).Run(base::unexpected("Do not support pdf capturing"));
    return;
  }
  auto* view = web_contents->GetRenderWidgetHostView();
  if (!view || view->GetVisibleViewportSize().IsEmpty()) {
    std::move(callback).Run(
        base::unexpected("No visible render widget host view available"));
    return;
  }
  viewport_bounds_ = view->GetVisibleViewportSize();

  // Start capturing via Paint Preview.
  CaptureParams capture_params;
  capture_params.web_contents = web_contents;
  capture_params.persistence =
      paint_preview::RecordingPersistence::kMemoryBuffer;
  capture_params.capture_links = false;
  CapturePaintPreview(
      capture_params,
      base::BindOnce(&FullScreenshotter::OnScreenshotCaptured,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FullScreenshotter::InitCompositorServiceForTest(
    std::unique_ptr<paint_preview::PaintPreviewCompositorService,
                    base::OnTaskRunnerDeleter> service) {
  paint_preview_compositor_service_ = std::move(service);
  paint_preview_compositor_client_ =
      paint_preview_compositor_service_->CreateCompositor(base::DoNothing());
}

void FullScreenshotter::OnScreenshotCaptured(
    CaptureScreenshotsCallback callback,
    paint_preview::PaintPreviewBaseService::CaptureStatus status,
    std::unique_ptr<paint_preview::CaptureResult> result) {
  if (status != PaintPreviewBaseService::CaptureStatus::kOk ||
      !result->capture_success) {
    std::move(callback).Run(base::unexpected(
        absl::StrFormat("Failed to capture a screenshot (CaptureStatus=%d)",
                        static_cast<int>(status))));
    return;
  }

  if (!paint_preview_compositor_client_) {
    if (!paint_preview_compositor_service_) {
      paint_preview_compositor_service_ = paint_preview::StartCompositorService(
          base::BindOnce(&FullScreenshotter::OnCompositorServiceDisconnected,
                         weak_ptr_factory_.GetWeakPtr()));
    }
    paint_preview_compositor_client_ =
        paint_preview_compositor_service_->CreateCompositor(
            base::BindOnce(&FullScreenshotter::SendCompositeRequest,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           PrepareCompositeRequest(std::move(result))));
  } else {
    SendCompositeRequest(std::move(callback),
                         PrepareCompositeRequest(std::move(result)));
  }
}

paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
FullScreenshotter::PrepareCompositeRequest(
    std::unique_ptr<paint_preview::CaptureResult> capture_result) {
  paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
      begin_composite_request =
          paint_preview::mojom::PaintPreviewBeginCompositeRequest::New();
  std::pair<paint_preview::RecordingMap, paint_preview::PaintPreviewProto>
      map_and_proto = paint_preview::RecordingMapFromCaptureResult(
          std::move(*capture_result));
  begin_composite_request->recording_map = std::move(map_and_proto.first);
  if (begin_composite_request->recording_map.empty()) {
    VLOG(2) << "Captured an empty screenshot";
    return nullptr;
  }
  begin_composite_request->preview =
      mojo_base::ProtoWrapper(std::move(map_and_proto.second));
  return begin_composite_request;
}

void FullScreenshotter::SendCompositeRequest(
    CaptureScreenshotsCallback callback,
    paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
        begin_composite_request) {
  if (!begin_composite_request) {
    std::move(callback).Run(
        base::unexpected("Invalid begin_composite_request"));
    return;
  }

  CHECK(paint_preview_compositor_client_);
  paint_preview_compositor_client_->BeginMainFrameComposite(
      std::move(begin_composite_request),
      base::BindOnce(&FullScreenshotter::OnCompositeFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FullScreenshotter::OnCompositorServiceDisconnected() {
  VLOG(2) << "Compositor service is disconnected";
  paint_preview_compositor_client_.reset();
  paint_preview_compositor_service_.reset();
}

void FullScreenshotter::OnCompositeFinished(
    CaptureScreenshotsCallback callback,
    paint_preview::mojom::PaintPreviewCompositor::BeginCompositeStatus status,
    paint_preview::mojom::PaintPreviewBeginCompositeResponsePtr response) {
  if (status != paint_preview::mojom::PaintPreviewCompositor::
                    BeginCompositeStatus::kSuccess) {
    std::move(callback).Run(base::unexpected("BeginMainFrameComposite failed"));
    return;
  }

  if (!response->frames.contains(response->root_frame_guid)) {
    std::move(callback).Run(base::unexpected("Root frame data not found"));
    return;
  }

  const auto& frame_data = response->frames[response->root_frame_guid];
  const auto& content_size = frame_data->scroll_extents;

  auto pending = std::make_unique<PendingScreenshots>();

  // Calculate number of full viewport screenshots needed
  int total_height = content_size.height();
  int viewport_height = viewport_bounds_.height();
  int num_screenshots = (total_height + viewport_height - 1) /
                        viewport_height;  // Ceiling division

  pending->completed_images.resize(num_screenshots);

  // Queue up screenshot rectangles
  for (int i = 0; i < num_screenshots; ++i) {
    int y = i * viewport_height;
    int height = std::min(viewport_height, total_height - y);
    pending->remaining_rects.emplace(0, y, content_size.width(), height);
  }

  pending->callback = std::move(callback);
  CaptureNextScreenshot(std::move(pending));
}

void FullScreenshotter::CaptureNextScreenshot(
    std::unique_ptr<PendingScreenshots> pending) {
  if (pending->remaining_rects.empty()) {
    // All screenshots captured, return results
    std::move(pending->callback)
        .Run(base::ok(std::move(pending->completed_images)));
    return;
  }

  // Take the next rect to capture
  gfx::Rect capture_rect = pending->remaining_rects.front();
  pending->remaining_rects.pop();

  paint_preview_compositor_client_->BitmapForMainFrame(
      capture_rect, 1,
      base::BindOnce(&FullScreenshotter::OnBitmapReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(pending),
                     pending->completed_images.size() -
                         pending->remaining_rects.size() - 1));
}

void FullScreenshotter::OnBitmapReceived(
    std::unique_ptr<PendingScreenshots> pending,
    size_t index,
    paint_preview::mojom::PaintPreviewCompositor::BitmapStatus status,
    const SkBitmap& bitmap) {
  if (status != paint_preview::mojom::PaintPreviewCompositor::BitmapStatus::
                    kSuccess ||
      bitmap.empty()) {
    std::move(pending->callback)
        .Run(base::unexpected(
            absl::StrFormat("Failed to get bitmap (BitmapStatus=%d)",
                            static_cast<int>(status))));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&FullScreenshotter::EncodeBitmap, ScaleDownBitmap(bitmap)),
      base::BindOnce(&FullScreenshotter::OnBitmapEncoded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(pending),
                     index));
}

// static
base::expected<std::vector<uint8_t>, std::string>
FullScreenshotter::EncodeBitmap(const SkBitmap& bitmap) {
  auto data = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false);
  if (!data) {
    return base::unexpected("Failed to encode the bitmap");
  }
  return base::ok(*data);
}

void FullScreenshotter::OnBitmapEncoded(
    std::unique_ptr<PendingScreenshots> pending,
    size_t index,
    base::expected<std::vector<uint8_t>, std::string> result) {
  if (!result.has_value()) {
    std::move(pending->callback).Run(base::unexpected(result.error()));
    return;
  }
  pending->completed_images[index] = std::move(*result);
  CaptureNextScreenshot(std::move(pending));
}

}  // namespace ai_chat
