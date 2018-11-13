//===--- FilePathWatcher.cpp ----------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2018 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "IndexStoreDB/Support/FilePathWatcher.h"
#include "IndexStoreDB/Support/Logging.h"

#if __has_include(<CoreServices/CoreServices.h>)
#import <CoreServices/CoreServices.h>

using namespace IndexStoreDB;
using namespace llvm;

struct FilePathWatcher::Implementation {
  FSEventStreamRef EventStream = nullptr;

  explicit Implementation(FileEventsReceiverTy pathsReceiver);

  void setupFSEventStream(ArrayRef<std::string> paths, FileEventsReceiverTy pathsReceiver,
                          dispatch_queue_t queue);
  void stopFSEventStream();

  ~Implementation() {
    stopFSEventStream();
  };
};

FilePathWatcher::Implementation::Implementation(FileEventsReceiverTy pathsReceiver) {
  std::vector<std::string> pathsToWatch;
  // FIXME: We should do something smarter than watching all of root. In the meantime
  // this matches what DVTFoundation is doing as well for DVTFilePaths.
  pathsToWatch.push_back("/");

  dispatch_queue_attr_t qosAttribute = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_UTILITY, 0);
  dispatch_queue_t queue = dispatch_queue_create("IndexStoreDB.fsevents", qosAttribute);
  setupFSEventStream(pathsToWatch, std::move(pathsReceiver), queue);
  dispatch_release(queue);
}

namespace {
struct EventStreamContextData {
  FilePathWatcher::FileEventsReceiverTy PathsReceiver;

  static void dispose(const void *ctx) {
    delete static_cast<const EventStreamContextData*>(ctx);
  }
};
}

static void eventStreamCallback(
                       ConstFSEventStreamRef stream,
                       void *clientCallBackInfo,
                       size_t numEvents,
                       void *eventPaths,
                       const FSEventStreamEventFlags eventFlags[],
                       const FSEventStreamEventId eventIds[]) {
  auto *ctx = static_cast<const EventStreamContextData*>(clientCallBackInfo);
  auto paths = makeArrayRef((const char **)eventPaths, numEvents);
  std::vector<std::string> strPaths;
  strPaths.reserve(paths.size());
  for (auto path : paths) {
    strPaths.push_back(path);
  }

  ctx->PathsReceiver(std::move(strPaths));
}

void FilePathWatcher::Implementation::setupFSEventStream(ArrayRef<std::string> paths,
                                                         FileEventsReceiverTy pathsReceiver,
                                                         dispatch_queue_t queue) {
  if (paths.empty())
    return;

  CFMutableArrayRef pathsToWatch = CFArrayCreateMutable(nullptr, 0, &kCFTypeArrayCallBacks);
  for (StringRef path : paths) {
    CFStringRef cfPathStr = CFStringCreateWithBytes(nullptr, (const UInt8 *)path.data(), path.size(), kCFStringEncodingUTF8, false);
    CFArrayAppendValue(pathsToWatch, cfPathStr);
    CFRelease(cfPathStr);
  }
  CFAbsoluteTime latency = 1.0; // Latency in seconds.

  EventStreamContextData *ctxData = new EventStreamContextData();
  ctxData->PathsReceiver = pathsReceiver;
  FSEventStreamContext context;
  context.version = 0;
  context.info = ctxData;
  context.retain = nullptr;
  context.release = EventStreamContextData::dispose;
  context.copyDescription = nullptr;

  EventStream = FSEventStreamCreate(nullptr,
                                    eventStreamCallback,
                                    &context,
                                    pathsToWatch,
                                    kFSEventStreamEventIdSinceNow,
                                    latency,
                                    kFSEventStreamCreateFlagNone);
  CFRelease(pathsToWatch);
  if (!EventStream) {
    LOG_WARN_FUNC("FSEventStreamCreate failed");
    return;
  }
  FSEventStreamSetDispatchQueue(EventStream, queue);
  FSEventStreamStart(EventStream);
}

void FilePathWatcher::Implementation::stopFSEventStream() {
  if (!EventStream)
    return;
  FSEventStreamStop(EventStream);
  FSEventStreamInvalidate(EventStream);
  FSEventStreamRelease(EventStream);
  EventStream = nullptr;
}

#else

using namespace IndexStoreDB;
using namespace llvm;

// TODO: implement for platforms without CoreServices.
struct FilePathWatcher::Implementation {
  explicit Implementation(FileEventsReceiverTy pathsReceiver) {}
};

#endif

FilePathWatcher::FilePathWatcher(FileEventsReceiverTy pathsReceiver)
: Impl(*new Implementation(std::move(pathsReceiver))) {

}

FilePathWatcher::~FilePathWatcher() {
  delete &Impl;
}

