/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "NetworkRTCProvider.h"

#if USE(LIBWEBRTC)

#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkRTCSocket.h"
#include "WebRTCResolverMessages.h"
#include "WebRTCSocketMessages.h"
#include <WebCore/LibWebRTCMacros.h>
#include <webrtc/base/asyncpacketsocket.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

static inline std::unique_ptr<rtc::Thread> createThread()
{
    auto thread = rtc::Thread::CreateWithSocketServer();
    auto result = thread->Start();
    ASSERT_UNUSED(result, result);
    // FIXME: Set thread name.
    return thread;
}

NetworkRTCProvider::NetworkRTCProvider(NetworkConnectionToWebProcess& connection)
    : m_connection(&connection)
    , m_rtcMonitor(*this)
    , m_rtcNetworkThread(createThread())
    , m_packetSocketFactory(makeUniqueRef<rtc::BasicPacketSocketFactory>(m_rtcNetworkThread.get()))
{
#if defined(NDEBUG)
    rtc::LogMessage::LogToDebug(rtc::LS_NONE);
#else
    if (WebKit2LogWebRTC.state != WTFLogChannelOn)
        rtc::LogMessage::LogToDebug(rtc::LS_WARNING);
#endif
}

NetworkRTCProvider::~NetworkRTCProvider()
{
    ASSERT(!m_connection);
    ASSERT(!m_sockets.size());
    ASSERT(!m_rtcMonitor.isStarted());

    for (auto identifier : m_resolvers.keys())
        stopResolver(identifier);
}

void NetworkRTCProvider::close()
{
    m_connection = nullptr;
    m_rtcMonitor.stopUpdating();

    callOnRTCNetworkThread([this]() {
        m_sockets.clear();
        callOnMainThread([provider = makeRef(*this)]() {
            if (provider->m_rtcNetworkThread)
                provider->m_rtcNetworkThread->Stop();
        });
    });
}

void NetworkRTCProvider::createSocket(uint64_t identifier, std::unique_ptr<rtc::AsyncPacketSocket>&& socket, LibWebRTCSocketClient::Type type)
{
    if (!socket) {
        sendFromMainThread([identifier](IPC::Connection& connection) {
            connection.send(Messages::WebRTCSocket::SignalClose(1), identifier);
        });
        return;
    }
    addSocket(identifier, std::make_unique<LibWebRTCSocketClient>(identifier, *this, WTFMove(socket), type));
}

void NetworkRTCProvider::createUDPSocket(uint64_t identifier, const RTCNetwork::SocketAddress& address, uint16_t minPort, uint16_t maxPort)
{
    callOnRTCNetworkThread([this, identifier, address = RTCNetwork::isolatedCopy(address.value), minPort, maxPort]() {
        std::unique_ptr<rtc::AsyncPacketSocket> socket(m_packetSocketFactory->CreateUdpSocket(address, minPort, maxPort));
        createSocket(identifier, WTFMove(socket), LibWebRTCSocketClient::Type::UDP);
    });
}

void NetworkRTCProvider::createServerTCPSocket(uint64_t identifier, const RTCNetwork::SocketAddress& address, uint16_t minPort, uint16_t maxPort, int options)
{
    callOnRTCNetworkThread([this, identifier, address = RTCNetwork::isolatedCopy(address.value), minPort, maxPort, options]() {
        std::unique_ptr<rtc::AsyncPacketSocket> socket(m_packetSocketFactory->CreateServerTcpSocket(address, minPort, maxPort, options));
        createSocket(identifier, WTFMove(socket), LibWebRTCSocketClient::Type::ServerTCP);
    });
}

void NetworkRTCProvider::createClientTCPSocket(uint64_t identifier, const RTCNetwork::SocketAddress& localAddress, const RTCNetwork::SocketAddress& remoteAddress, int options)
{
    callOnRTCNetworkThread([this, identifier, localAddress = RTCNetwork::isolatedCopy(localAddress.value), remoteAddress = RTCNetwork::isolatedCopy(remoteAddress.value), options]() {
        std::unique_ptr<rtc::AsyncPacketSocket> socket(m_packetSocketFactory->CreateClientTcpSocket(localAddress, remoteAddress, { }, { }, options));
        createSocket(identifier, WTFMove(socket), LibWebRTCSocketClient::Type::ClientTCP);
    });
}

void NetworkRTCProvider::wrapNewTCPConnection(uint64_t identifier, uint64_t newConnectionSocketIdentifier)
{
    callOnRTCNetworkThread([this, identifier, newConnectionSocketIdentifier]() {
        std::unique_ptr<rtc::AsyncPacketSocket> socket = m_pendingIncomingSockets.take(newConnectionSocketIdentifier);
        addSocket(identifier, std::make_unique<LibWebRTCSocketClient>(identifier, *this, WTFMove(socket), LibWebRTCSocketClient::Type::ServerConnectionTCP));
    });
}

void NetworkRTCProvider::addSocket(uint64_t identifier, std::unique_ptr<LibWebRTCSocketClient>&& socket)
{
    m_sockets.add(identifier, WTFMove(socket));
}

std::unique_ptr<LibWebRTCSocketClient> NetworkRTCProvider::takeSocket(uint64_t identifier)
{
    return m_sockets.take(identifier);
}

void NetworkRTCProvider::newConnection(LibWebRTCSocketClient& serverSocket, std::unique_ptr<rtc::AsyncPacketSocket>&& newSocket)
{
    sendFromMainThread([identifier = serverSocket.identifier(), incomingSocketIdentifier = ++m_incomingSocketIdentifier, remoteAddress = RTCNetwork::isolatedCopy(newSocket->GetRemoteAddress())](IPC::Connection& connection) {
        connection.send(Messages::WebRTCSocket::SignalNewConnection(incomingSocketIdentifier, RTCNetwork::SocketAddress(remoteAddress)), identifier);
    });
    m_pendingIncomingSockets.add(m_incomingSocketIdentifier, WTFMove(newSocket));
}

void NetworkRTCProvider::didReceiveNetworkRTCSocketMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    NetworkRTCSocket(decoder.destinationID(), *this).didReceiveMessage(connection, decoder);
}

void NetworkRTCProvider::createResolver(uint64_t identifier, const String& address)
{
    CFHostRef host = CFHostCreateWithName(kCFAllocatorDefault, address.createCFString().get());
    ASSERT(host);

    auto resolver = std::make_unique<Resolver>(identifier, *this, host);

    CFHostClientContext context = { 0, resolver.get(), nullptr, nullptr, nullptr };
    CFHostSetClient(host, NetworkRTCProvider::resolvedName, &context);
    CFHostScheduleWithRunLoop(host, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    Boolean result = CFHostStartInfoResolution(host, kCFHostAddresses, nullptr);
    ASSERT_UNUSED(result, result);

    m_resolvers.add(identifier, WTFMove(resolver));
}

NetworkRTCProvider::Resolver::~Resolver()
{
    CFHostUnscheduleFromRunLoop(host.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    CFHostSetClient(host.get(), nullptr, nullptr);
}

void NetworkRTCProvider::stopResolver(uint64_t identifier)
{
    auto resolver = m_resolvers.take(identifier);
    if (resolver)
        CFHostCancelInfoResolution(resolver->host.get(), CFHostInfoType::kCFHostAddresses);
}

void NetworkRTCProvider::resolvedName(CFHostRef hostRef, CFHostInfoType typeInfo, const CFStreamError *error, void *info)
{
    ASSERT_UNUSED(typeInfo, !typeInfo);

    if (error->domain) {
        // FIXME: Need to handle failure, but info is not provided in the callback.
        return;
    }

    ASSERT(info);
    auto* resolverInfo = static_cast<Resolver*>(info);
    auto resolver = resolverInfo->rtcProvider.m_resolvers.take(resolverInfo->identifier);
    if (!resolver)
        return;

    Boolean result;
    CFArrayRef resolvedAddresses = (CFArrayRef)CFHostGetAddressing(hostRef, &result);
    ASSERT_UNUSED(result, result);

    size_t count = CFArrayGetCount(resolvedAddresses);
    Vector<RTCNetwork::IPAddress> addresses;
    addresses.reserveInitialCapacity(count);

    for (size_t index = 0; index < count; ++index) {
        CFDataRef data = (CFDataRef)CFArrayGetValueAtIndex(resolvedAddresses, index);
        auto* address = reinterpret_cast<const struct sockaddr_in*>(CFDataGetBytePtr(data));
        addresses.uncheckedAppend(RTCNetwork::IPAddress(rtc::IPAddress(address->sin_addr)));
    }
    ASSERT(resolver->rtcProvider.m_connection);
    resolver->rtcProvider.m_connection->connection().send(Messages::WebRTCResolver::SetResolvedAddress(addresses), resolver->identifier);
}

struct NetworkMessageData : public rtc::MessageData {
    NetworkMessageData(Ref<NetworkRTCProvider>&& rtcProvider, Function<void()>&& callback)
        : rtcProvider(WTFMove(rtcProvider))
        , callback(WTFMove(callback))
    { }
    Ref<NetworkRTCProvider> rtcProvider;
    Function<void()> callback;
};

void NetworkRTCProvider::OnMessage(rtc::Message* message)
{
    ASSERT(message->message_id == 1);
    auto* data = static_cast<NetworkMessageData*>(message->pdata);
    data->callback();
    delete data;
}

void NetworkRTCProvider::callOnRTCNetworkThread(Function<void()>&& callback)
{
    m_rtcNetworkThread->Post(RTC_FROM_HERE, this, 1, new NetworkMessageData(*this, WTFMove(callback)));
}

void NetworkRTCProvider::callSocket(uint64_t identifier, Function<void(LibWebRTCSocketClient&)>&& callback)
{
    callOnRTCNetworkThread([this, identifier, callback = WTFMove(callback)]() {
        if (auto* socket = m_sockets.get(identifier))
            callback(*socket);
    });
}

void NetworkRTCProvider::sendFromMainThread(Function<void(IPC::Connection&)>&& callback)
{
    callOnMainThread([provider = makeRef(*this), callback = WTFMove(callback)]() {
        if (provider->m_connection)
            callback(provider->m_connection->connection());
    });
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
