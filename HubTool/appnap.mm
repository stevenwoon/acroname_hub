#include "appnap.h"
#include <Foundation/NSProcessInfo.h>
//https://developer.apple.com/documentation/foundation/nsactivityoptions?language=objc

class AppNapSuspenderPrivate {
public:
    id<NSObject> activityId;
};

AppNapSuspender::AppNapSuspender() : p(new AppNapSuspenderPrivate) {}

AppNapSuspender::~AppNapSuspender() {
    delete p;
}

void AppNapSuspender::suspend() {
    if (@available(macOS 10.9, *)) {
        p->activityId = [[NSProcessInfo processInfo ]
            //beginActivityWithOptions: NSActivityUserInitiated | NSActivityLatencyCritical reason:@"Good reason"];
                beginActivityWithOptions: NSActivityUserInitiated reason:@"Good reason"];
    } else {
        // Fallback on earlier versions
    }
}

void AppNapSuspender::resume() {
    if (@available(macOS 10.9, *)) {
        [[NSProcessInfo processInfo ] endActivity:p->activityId];
    } else {
        // Fallback on earlier versions
    }
}
