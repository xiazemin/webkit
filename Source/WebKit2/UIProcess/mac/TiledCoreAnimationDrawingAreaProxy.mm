/*
 * Copyright (C) 2011-2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "TiledCoreAnimationDrawingAreaProxy.h"

#if !PLATFORM(IOS)

#import "ColorSpaceData.h"
#import "DrawingAreaMessages.h"
#import "DrawingAreaProxyMessages.h"
#import "LayerTreeContext.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebCore/MachSendRight.h>
#import <WebCore/QuartzCoreSPI.h>
#import <wtf/BlockPtr.h>

using namespace IPC;
using namespace WebCore;

namespace WebKit {

TiledCoreAnimationDrawingAreaProxy::TiledCoreAnimationDrawingAreaProxy(WebPageProxy& webPageProxy)
    : DrawingAreaProxy(DrawingAreaTypeTiledCoreAnimation, webPageProxy)
    , m_isWaitingForDidUpdateGeometry(false)
{
}

TiledCoreAnimationDrawingAreaProxy::~TiledCoreAnimationDrawingAreaProxy()
{
}

void TiledCoreAnimationDrawingAreaProxy::deviceScaleFactorDidChange()
{
    m_webPageProxy.process().send(Messages::DrawingArea::SetDeviceScaleFactor(m_webPageProxy.deviceScaleFactor()), m_webPageProxy.pageID());
}

void TiledCoreAnimationDrawingAreaProxy::sizeDidChange()
{
    if (!m_webPageProxy.isValid())
        return;

    // We only want one UpdateGeometry message in flight at once, so if we've already sent one but
    // haven't yet received the reply we'll just return early here.
    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void TiledCoreAnimationDrawingAreaProxy::waitForPossibleGeometryUpdate(Seconds timeout)
{
#if !HAVE(COREANIMATION_FENCES)
    if (!m_isWaitingForDidUpdateGeometry)
        return;

    if (m_webPageProxy.process().state() != WebProcessProxy::State::Running)
        return;

    m_webPageProxy.process().connection()->waitForAndDispatchImmediately<Messages::DrawingAreaProxy::DidUpdateGeometry>(m_webPageProxy.pageID(), timeout, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
#endif
}

void TiledCoreAnimationDrawingAreaProxy::colorSpaceDidChange()
{
    m_webPageProxy.process().send(Messages::DrawingArea::SetColorSpace(m_webPageProxy.colorSpace()), m_webPageProxy.pageID());
}

void TiledCoreAnimationDrawingAreaProxy::minimumLayoutSizeDidChange()
{
    if (!m_webPageProxy.isValid())
        return;

    // We only want one UpdateGeometry message in flight at once, so if we've already sent one but
    // haven't yet received the reply we'll just return early here.
    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void TiledCoreAnimationDrawingAreaProxy::enterAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext& layerTreeContext)
{
    m_webPageProxy.enterAcceleratedCompositingMode(layerTreeContext);
}

void TiledCoreAnimationDrawingAreaProxy::exitAcceleratedCompositingMode(uint64_t backingStoreStateID, const UpdateInfo&)
{
    // This should never be called.
    ASSERT_NOT_REACHED();
}

void TiledCoreAnimationDrawingAreaProxy::updateAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext& layerTreeContext)
{
    m_webPageProxy.updateAcceleratedCompositingMode(layerTreeContext);
}

void TiledCoreAnimationDrawingAreaProxy::didUpdateGeometry()
{
    ASSERT(m_isWaitingForDidUpdateGeometry);

    m_isWaitingForDidUpdateGeometry = false;

    IntSize minimumLayoutSize = m_webPageProxy.minimumLayoutSize();

    // If the WKView was resized while we were waiting for a DidUpdateGeometry reply from the web process,
    // we need to resend the new size here.
    if (m_lastSentSize != m_size || m_lastSentMinimumLayoutSize != minimumLayoutSize)
        sendUpdateGeometry();
}

void TiledCoreAnimationDrawingAreaProxy::waitForDidUpdateActivityState()
{
    Seconds activityStateUpdateTimeout = Seconds::fromMilliseconds(250);
    m_webPageProxy.process().connection()->waitForAndDispatchImmediately<Messages::WebPageProxy::DidUpdateActivityState>(m_webPageProxy.pageID(), activityStateUpdateTimeout, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
}

void TiledCoreAnimationDrawingAreaProxy::intrinsicContentSizeDidChange(const IntSize& newIntrinsicContentSize)
{
    if (m_webPageProxy.minimumLayoutSize().width() > 0)
        m_webPageProxy.intrinsicContentSizeDidChange(newIntrinsicContentSize);
}

void TiledCoreAnimationDrawingAreaProxy::willSendUpdateGeometry()
{
    m_lastSentMinimumLayoutSize = m_webPageProxy.minimumLayoutSize();
    m_lastSentSize = m_size;
    m_isWaitingForDidUpdateGeometry = true;
}

MachSendRight TiledCoreAnimationDrawingAreaProxy::createFence()
{
#if HAVE(COREANIMATION_FENCES)
    if (!m_webPageProxy.isValid())
        return MachSendRight();

    RetainPtr<CAContext> rootLayerContext = [asLayer(m_webPageProxy.acceleratedCompositingRootLayer()) context];
    if (!rootLayerContext)
        return MachSendRight();

    // Don't fence if we don't have a connection, because the message
    // will likely get dropped on the floor (if the Web process is terminated)
    // or queued up until process launch completes, and there's nothing useful
    // to synchronize in these cases.
    if (!m_webPageProxy.process().connection())
        return MachSendRight();

    // Don't fence if we have incoming synchronous messages, because we may not
    // be able to reply to the message until the fence times out.
    if (m_webPageProxy.process().connection()->hasIncomingSyncMessage())
        return MachSendRight();

    MachSendRight fencePort = MachSendRight::adopt([rootLayerContext createFencePort]);

    // Invalidate the fence if a synchronous message arrives while it's installed,
    // because we won't be able to reply during the fence-wait.
    uint64_t callbackID = m_webPageProxy.process().connection()->installIncomingSyncMessageCallback([rootLayerContext] {
        [rootLayerContext invalidateFences];
    });
    RefPtr<WebPageProxy> retainedPage = &m_webPageProxy;
    [CATransaction addCommitHandler:[callbackID, retainedPage] {
        if (!retainedPage->isValid())
            return;
        if (Connection* connection = retainedPage->process().connection())
            connection->uninstallIncomingSyncMessageCallback(callbackID);
    } forPhase:kCATransactionPhasePostCommit];

    return fencePort;
#else
    return MachSendRight();
#endif
}

void TiledCoreAnimationDrawingAreaProxy::sendUpdateGeometry()
{
    ASSERT(!m_isWaitingForDidUpdateGeometry);

    willSendUpdateGeometry();
    m_webPageProxy.process().send(Messages::DrawingArea::UpdateGeometry(m_size, m_layerPosition, true, createFence()), m_webPageProxy.pageID());
}

void TiledCoreAnimationDrawingAreaProxy::adjustTransientZoom(double scale, FloatPoint origin)
{
    m_webPageProxy.process().send(Messages::DrawingArea::AdjustTransientZoom(scale, origin), m_webPageProxy.pageID());
}

void TiledCoreAnimationDrawingAreaProxy::commitTransientZoom(double scale, FloatPoint origin)
{
    m_webPageProxy.process().send(Messages::DrawingArea::CommitTransientZoom(scale, origin), m_webPageProxy.pageID());
}

void TiledCoreAnimationDrawingAreaProxy::dispatchAfterEnsuringDrawing(WTF::Function<void (CallbackBase::Error)>&& callback)
{
    // This callback is primarily used for testing in RemoteLayerTreeDrawingArea. We could in theory wait for a CA commit here.
    dispatch_async(dispatch_get_main_queue(), BlockPtr<void ()>::fromCallable([callback = WTFMove(callback)] {
        callback(CallbackBase::Error::None);
    }).get());
}

} // namespace WebKit

#endif // !PLATFORM(IOS)
