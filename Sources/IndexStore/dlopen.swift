//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2025 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import Foundation

#if os(Windows)
import CRT
import WinSDK
#elseif canImport(Darwin)
import Darwin
#elseif canImport(Glibc)
import Glibc
#elseif canImport(Musl)
import Musl
#elseif canImport(Android)
import Android
#endif

final class DLHandle: Sendable {
  fileprivate struct Handle: @unchecked Sendable {
    #if os(Windows)
    let handle: HMODULE
    #else
    let handle: UnsafeMutableRawPointer
    #endif
  }

  #if canImport(Darwin)
  static let rtldDefault = DLHandle(rawValue: Handle(handle: UnsafeMutableRawPointer(bitPattern: -2)!))
  #endif

  fileprivate let rawValue: Handle

  fileprivate init(rawValue: Handle) {
    self.rawValue = rawValue
  }
}

struct DLOpenFlags: RawRepresentable, OptionSet, Sendable {
  #if !os(Windows)
  static let lazy: DLOpenFlags = DLOpenFlags(rawValue: RTLD_LAZY)
  static let now: DLOpenFlags = DLOpenFlags(rawValue: RTLD_NOW)
  static let local: DLOpenFlags = DLOpenFlags(rawValue: RTLD_LOCAL)
  static let global: DLOpenFlags = DLOpenFlags(rawValue: RTLD_GLOBAL)

  // Platform-specific flags.
  #if os(macOS)
  static let first: DLOpenFlags = DLOpenFlags(rawValue: RTLD_FIRST)
  #else
  static let first: DLOpenFlags = DLOpenFlags(rawValue: 0)
  #endif
  #endif

  var rawValue: Int32

  init(rawValue: Int32) {
    self.rawValue = rawValue
  }
}

enum DLError: Swift.Error {
  case `open`(String)
}

func dlopen(_ path: String?, mode: DLOpenFlags) throws -> DLHandle {
  #if os(Windows)
  guard let handle = path?.withCString(encodedAs: UTF16.self, LoadLibraryW) else {
    throw DLError.open("LoadLibraryW failed: \(GetLastError())")
  }
  #else
  guard let handle = dlopen(path, mode.rawValue) else {
    throw DLError.open(dlerror() ?? "unknown error")
  }
  #endif
  return DLHandle(rawValue: DLHandle.Handle(handle: handle))
}

func dlsym<T>(_ handle: DLHandle, symbol: String) -> T? {
  #if os(Windows)
  guard let ptr = GetProcAddress(handle.rawValue.handle, symbol) else {
    return nil
  }
  #else
  guard let ptr = dlsym(handle.rawValue.handle, symbol) else {
    return nil
  }
  #endif
  return unsafeBitCast(ptr, to: T.self)
}

#if !os(Windows)
func dlerror() -> String? {
  if let err: UnsafeMutablePointer<Int8> = dlerror() {
    return String(cString: err)
  }
  return nil
}
#endif
