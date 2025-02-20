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

#import "config.h"

#import "PlatformUtilities.h"
#import "Test.h"
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKURLSchemeTask.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKIconLoadingDelegate.h>
#import <WebKit/_WKLinkIconParameters.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED

static bool doneWithIcons;
static bool receivedFaviconDataCallback;
static bool alreadyProvidedIconData;
static RetainPtr<NSData> receivedFaviconData;

@interface IconLoadingDelegate : NSObject <_WKIconLoadingDelegate>
@end

@implementation IconLoadingDelegate {
    RetainPtr<_WKLinkIconParameters> favicon;
    RetainPtr<_WKLinkIconParameters> touch;
    RetainPtr<_WKLinkIconParameters> touchPrecomposed;
}

- (void)webView:(WKWebView *)webView shouldLoadIconWithParameters:(_WKLinkIconParameters *)parameters completionHandler:(void (^)(void (^)(NSData*)))completionHandler
{
    switch (parameters.iconType) {
    case WKLinkIconTypeFavicon:
        favicon = parameters;
        EXPECT_TRUE([[parameters.url absoluteString] isEqual:@"testing:///favicon.ico"]);
        break;
    case WKLinkIconTypeTouchIcon:
        touch = parameters;
        break;
    case WKLinkIconTypeTouchPrecomposedIcon:
        touchPrecomposed = parameters;
    }

    if (favicon && touch && touchPrecomposed)
        doneWithIcons = true;

    if (parameters.iconType == WKLinkIconTypeFavicon) {
        completionHandler([](NSData *iconData) {
            receivedFaviconData = iconData;
            receivedFaviconDataCallback = true;
        });
    } else
        completionHandler(nil);
}

@end

@interface IconLoadingSchemeHandler : NSObject <WKURLSchemeHandler>
- (instancetype)initWithData:(NSData *)data;
- (void)setFaviconData:(NSData *)data;
@end

@implementation IconLoadingSchemeHandler {
    RetainPtr<NSData> mainResourceData;
    RetainPtr<NSData> faviconData;
}

- (instancetype)initWithData:(NSData *)data
{
    self = [super init];
    if (!self)
        return nil;

    mainResourceData = data;

    return self;
}

- (void)setFaviconData:(NSData *)data
{
    faviconData = data;
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    RetainPtr<NSURLResponse> response;
    NSData *data = nil;

    if ([[task.request.URL absoluteString] isEqual:@"testing:///favicon.ico"]) {
        EXPECT_FALSE(alreadyProvidedIconData);
        response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"image/png" expectedContentLength:1 textEncodingName:nil]);
        data = faviconData.get();
        alreadyProvidedIconData = true;
    } else {
        response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:1 textEncodingName:nil]);
        data = mainResourceData.get();
    }

    [task didReceiveResponse:response.get()];
    [task didReceiveData:data];
    [task didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
}

@end

static const char mainBytes[] =
"<head>" \
"<link rel=\"apple-touch-icon\" sizes=\"57x57\" href=\"http://example.com/my-apple-touch-icon.png\">" \
"<link rel=\"apple-touch-icon-precomposed\" sizes=\"57x57\" href=\"http://example.com/my-apple-touch-icon-precomposed.png\">" \
"</head>";

TEST(IconLoading, DefaultFavicon)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<IconLoadingSchemeHandler> handler = adoptNS([[IconLoadingSchemeHandler alloc] initWithData:[NSData dataWithBytesNoCopy:(void*)mainBytes length:sizeof(mainBytes)]]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<IconLoadingDelegate> iconDelegate = adoptNS([[IconLoadingDelegate alloc] init]);

    webView.get()._iconLoadingDelegate = iconDelegate.get();

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:///main"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&doneWithIcons);
}

static const char mainBytes2[] =
"Oh, hello there!";

TEST(IconLoading, AlreadyCachedIcon)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    NSData *mainData = [NSData dataWithBytesNoCopy:(void*)mainBytes2 length:sizeof(mainBytes2)];
    RetainPtr<IconLoadingSchemeHandler> handler = adoptNS([[IconLoadingSchemeHandler alloc] initWithData:mainData]);

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"large-red-square-image" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSData *> iconDataFromDisk = [NSData dataWithContentsOfURL:url];
    [handler.get() setFaviconData:iconDataFromDisk.get()];

    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<IconLoadingDelegate> iconDelegate = adoptNS([[IconLoadingDelegate alloc] init]);

    webView.get()._iconLoadingDelegate = iconDelegate.get();

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:///main"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&receivedFaviconDataCallback);

    EXPECT_TRUE([iconDataFromDisk.get() isEqual:receivedFaviconData.get()]);

    receivedFaviconDataCallback = false;
    receivedFaviconData = nil;

    // Load another main resource that results in the same icon being loaded (which should come from the memory cache).
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:///main2"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&receivedFaviconDataCallback);

    EXPECT_TRUE([iconDataFromDisk.get() isEqual:receivedFaviconData.get()]);
}

#endif
