/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#pragma once

#include "APIUserInitiatedAction.h"
#include "BackgroundProcessResponsivenessTimer.h"
#include "ChildProcessProxy.h"
#include "MessageReceiverMap.h"
#include "PluginInfoStore.h"
#include "ProcessLauncher.h"
#include "ProcessTerminationReason.h"
#include "ProcessThrottler.h"
#include "ProcessThrottlerClient.h"
#include "ResponsivenessTimer.h"
#include "VisibleWebPageCounter.h"
#include "WebConnectionToWebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/LinkHash.h>
#include <WebCore/SessionID.h>
#include <memory>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace API {
class PageConfiguration;
}

namespace WebCore {
class ResourceRequest;
class URL;
struct PluginInfo;
struct SecurityOriginData;
}

namespace WebKit {

class NetworkProcessProxy;
class ObjCObjectGraph;
class PageClient;
class UserMediaCaptureManagerProxy;
class VisitedLinkStore;
class WebBackForwardListItem;
class WebFrameProxy;
class WebPageGroup;
class WebPageProxy;
class WebProcessPool;
class WebUserContentControllerProxy;
class WebsiteDataStore;
enum class WebsiteDataType;
struct WebNavigationDataStore;
struct WebPageCreationParameters;
struct WebsiteData;

class WebProcessProxy : public ChildProcessProxy, public ResponsivenessTimer::Client, private ProcessThrottlerClient {
public:
    typedef HashMap<uint64_t, RefPtr<WebBackForwardListItem>> WebBackForwardListItemMap;
    typedef HashMap<uint64_t, RefPtr<WebFrameProxy>> WebFrameProxyMap;
    typedef HashMap<uint64_t, WebPageProxy*> WebPageProxyMap;
    typedef HashMap<uint64_t, RefPtr<API::UserInitiatedAction>> UserInitiatedActionMap;

    static Ref<WebProcessProxy> create(WebProcessPool&, WebsiteDataStore&);
    ~WebProcessProxy();

    WebConnection* webConnection() const { return m_webConnection.get(); }

    WebProcessPool& processPool() { return m_processPool; }

    // FIXME: WebsiteDataStores should be made per-WebPageProxy throughout WebKit2
    WebsiteDataStore& websiteDataStore() const { return m_websiteDataStore.get(); }

    static WebPageProxy* webPage(uint64_t pageID);
    Ref<WebPageProxy> createWebPage(PageClient&, Ref<API::PageConfiguration>&&);
    void addExistingWebPage(WebPageProxy&, uint64_t pageID);
    void removeWebPage(WebPageProxy&, uint64_t pageID);

    WTF::IteratorRange<WebPageProxyMap::const_iterator::Values> pages() const { return m_pageMap.values(); }
    unsigned pageCount() const { return m_pageMap.size(); }
    unsigned visiblePageCount() const { return m_visiblePageCounter.value(); }

    void addVisitedLinkStore(VisitedLinkStore&);
    void addWebUserContentControllerProxy(WebUserContentControllerProxy&, WebPageCreationParameters&);
    void didDestroyVisitedLinkStore(VisitedLinkStore&);
    void didDestroyWebUserContentControllerProxy(WebUserContentControllerProxy&);

    WebBackForwardListItem* webBackForwardItem(uint64_t itemID) const;
    RefPtr<API::UserInitiatedAction> userInitiatedActivity(uint64_t);

    ResponsivenessTimer& responsivenessTimer() { return m_responsivenessTimer; }
    bool isResponsive() const;

    WebFrameProxy* webFrame(uint64_t) const;
    bool canCreateFrame(uint64_t frameID) const;
    void frameCreated(uint64_t, WebFrameProxy*);
    void disconnectFramesFromPage(WebPageProxy*); // Including main frame.
    size_t frameCountInPage(WebPageProxy*) const; // Including main frame.

    VisibleWebPageToken visiblePageToken() const;

    void updateTextCheckerState();

    void registerNewWebBackForwardListItem(WebBackForwardListItem*);
    void removeBackForwardItem(uint64_t);

    void willAcquireUniversalFileReadSandboxExtension() { m_mayHaveUniversalFileReadSandboxExtension = true; }
    void assumeReadAccessToBaseURL(const String&);
    bool hasAssumedReadAccessToURL(const WebCore::URL&) const;

    bool checkURLReceivedFromWebProcess(const String&);
    bool checkURLReceivedFromWebProcess(const WebCore::URL&);

    static bool fullKeyboardAccessEnabled();

    void didSaveToPageCache();
    void releasePageCache();

    void fetchWebsiteData(WebCore::SessionID, OptionSet<WebsiteDataType>, Function<void(WebsiteData)>&& completionHandler);
    void deleteWebsiteData(WebCore::SessionID, OptionSet<WebsiteDataType>, std::chrono::system_clock::time_point modifiedSince, Function<void()>&& completionHandler);
    void deleteWebsiteDataForOrigins(WebCore::SessionID, OptionSet<WebsiteDataType>, const Vector<WebCore::SecurityOriginData>&, Function<void()>&& completionHandler);
    static void deleteWebsiteDataForTopPrivatelyControlledDomainsInAllPersistentDataStores(OptionSet<WebsiteDataType>, Vector<String>&& topPrivatelyControlledDomains, bool shouldNotifyPages, Function<void(Vector<String>)>&& completionHandler);
    static void topPrivatelyControlledDomainsWithWebiteData(OptionSet<WebsiteDataType> dataTypes, bool shouldNotifyPage, Function<void(HashSet<String>&&)>&& completionHandler);
    static void notifyPageStatisticsAndDataRecordsProcessed();

    void enableSuddenTermination();
    void disableSuddenTermination();
    bool isSuddenTerminationEnabled() { return !m_numberOfTimesSuddenTerminationWasDisabled; }

    void requestTermination(ProcessTerminationReason);

    void stopResponsivenessTimer();

    RefPtr<API::Object> transformHandlesToObjects(API::Object*);
    static RefPtr<API::Object> transformObjectsToHandles(API::Object*);

#if PLATFORM(COCOA)
    RefPtr<ObjCObjectGraph> transformHandlesToObjects(ObjCObjectGraph&);
    static RefPtr<ObjCObjectGraph> transformObjectsToHandles(ObjCObjectGraph&);
#endif

    void windowServerConnectionStateChanged();

    void processReadyToSuspend();
    void didCancelProcessSuspension();

    void setIsHoldingLockedFiles(bool);

    ProcessThrottler& throttler() { return m_throttler; }

    void reinstateNetworkProcessAssertionState(NetworkProcessProxy&);

    void isResponsive(WTF::Function<void(bool isWebProcessResponsive)>&&);
    void didReceiveMainThreadPing();
    void didReceiveBackgroundResponsivenessPing();

    void memoryPressureStatusChanged(bool isUnderMemoryPressure) { m_isUnderMemoryPressure = isUnderMemoryPressure; }
    bool isUnderMemoryPressure() const { return m_isUnderMemoryPressure; }
    void didExceedInactiveMemoryLimitWhileActive();

    void processTerminated();

    void didExceedCPULimit();
    void didExceedActiveMemoryLimit();
    void didExceedInactiveMemoryLimit();

private:
    explicit WebProcessProxy(WebProcessPool&, WebsiteDataStore&);

    // From ChildProcessProxy
    void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void connectionWillOpen(IPC::Connection&) override;
    void processWillShutDown(IPC::Connection&) override;

    // Called when the web process has crashed or we know that it will terminate soon.
    // Will potentially cause the WebProcessProxy object to be freed.
    void shutDown();

    // IPC message handlers.
    void addBackForwardItem(uint64_t itemID, uint64_t pageID, const PageState&);
    void didDestroyFrame(uint64_t);
    void didDestroyUserGestureToken(uint64_t);

    void shouldTerminate(bool& shouldTerminate);

    // Plugins
#if ENABLE(NETSCAPE_PLUGIN_API)
    void getPlugins(bool refresh, Vector<WebCore::PluginInfo>& plugins, Vector<WebCore::PluginInfo>& applicationPlugins);
#endif // ENABLE(NETSCAPE_PLUGIN_API)
#if ENABLE(NETSCAPE_PLUGIN_API)
    void getPluginProcessConnection(uint64_t pluginProcessToken, Ref<Messages::WebProcessProxy::GetPluginProcessConnection::DelayedReply>&&);
#endif
    void getNetworkProcessConnection(Ref<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply>&&);
#if ENABLE(DATABASE_PROCESS)
    void getDatabaseProcessConnection(Ref<Messages::WebProcessProxy::GetDatabaseProcessConnection::DelayedReply>&&);
#endif

    void retainIconForPageURL(const String& pageURL);
    void releaseIconForPageURL(const String& pageURL);
    void releaseRemainingIconsForPageURLs();

    bool platformIsBeingDebugged() const;

    static const HashSet<String>& platformPathsWithAssumedReadAccess();

    void updateBackgroundResponsivenessTimer();

    // IPC::Connection::Client
    friend class WebConnectionToWebProcess;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;

    // ResponsivenessTimer::Client
    void didBecomeUnresponsive() override;
    void didBecomeResponsive() override;
    void willChangeIsResponsive() override;
    void didChangeIsResponsive() override;
    bool mayBecomeUnresponsive() override;

    // ProcessThrottlerClient
    void sendProcessWillSuspendImminently() override;
    void sendPrepareToSuspend() override;
    void sendCancelPrepareToSuspend() override;
    void sendProcessDidResume() override;
    void didSetAssertionState(AssertionState) override;

    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

    // Implemented in generated WebProcessProxyMessageReceiver.cpp
    void didReceiveWebProcessProxyMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncWebProcessProxyMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

    bool canTerminateChildProcess();

    void logDiagnosticMessageForResourceLimitTermination(const String& limitKey);

    ResponsivenessTimer m_responsivenessTimer;
    BackgroundProcessResponsivenessTimer m_backgroundResponsivenessTimer;
    
    RefPtr<WebConnectionToWebProcess> m_webConnection;
    Ref<WebProcessPool> m_processPool;

    bool m_mayHaveUniversalFileReadSandboxExtension; // True if a read extension for "/" was ever granted - we don't track whether WebProcess still has it.
    HashSet<String> m_localPathsWithAssumedReadAccess;

    WebPageProxyMap m_pageMap;
    WebFrameProxyMap m_frameMap;
    WebBackForwardListItemMap m_backForwardListItemMap;
    UserInitiatedActionMap m_userInitiatedActionMap;

    HashSet<VisitedLinkStore*> m_visitedLinkStores;
    HashSet<WebUserContentControllerProxy*> m_webUserContentControllerProxies;

    int m_numberOfTimesSuddenTerminationWasDisabled;
    ProcessThrottler m_throttler;
    ProcessThrottler::BackgroundActivityToken m_tokenForHoldingLockedFiles;
#if PLATFORM(IOS)
    ProcessThrottler::ForegroundActivityToken m_foregroundTokenForNetworkProcess;
    ProcessThrottler::BackgroundActivityToken m_backgroundTokenForNetworkProcess;
#endif

    HashMap<String, uint64_t> m_pageURLRetainCountMap;

    enum class NoOrMaybe { No, Maybe } m_isResponsive;
    Vector<WTF::Function<void(bool webProcessIsResponsive)>> m_isResponsiveCallbacks;

    VisibleWebPageCounter m_visiblePageCounter;

    // FIXME: WebsiteDataStores should be made per-WebPageProxy throughout WebKit2. Get rid of this member.
    Ref<WebsiteDataStore> m_websiteDataStore;

    bool m_isUnderMemoryPressure { false };

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    std::unique_ptr<UserMediaCaptureManagerProxy> m_userMediaCaptureManagerProxy;
#endif
};

} // namespace WebKit
