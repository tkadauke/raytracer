#pragma once

// Shield the macOS SDK's global Rect symbol while importing Objective-C
// frameworks so project headers keep their Rect<T> template.
#define Rect MacOSRect
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#undef Rect

#include <stdexcept>
#include <string>

namespace render::detail {

inline id<MTLDevice> sharedMetalDevice() {
  static id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  return device;
}

inline bool metalDeviceAvailable() {
  @autoreleasepool {
    return sharedMetalDevice() != nil;
  }
}

inline id<MTLCommandQueue> sharedCommandQueue() {
  static id<MTLCommandQueue> queue = [] {
    id<MTLDevice> device = sharedMetalDevice();
    return device ? [device newCommandQueue] : nil;
  }();
  return queue;
}

inline std::runtime_error metalError(const std::string& context, NSError* error) {
  std::string detail;
  if (error) {
    detail = [[error localizedDescription] UTF8String];
  }
  return std::runtime_error(detail.empty() ? context : context + ": " + detail);
}

} // namespace render::detail
