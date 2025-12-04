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

public import IndexStoreDB_CIndexStoreDB

public enum IterationContinuationBehavior {
  case `continue`
  case stop
}

public struct IndexStoreSequence<Element: ~Escapable> {
  @usableFromInline let iterate: ((Element) -> IterationContinuationBehavior) -> Void

  @usableFromInline
  init(_ iterate: @escaping ((Element) -> IterationContinuationBehavior) -> Void) {
    self.iterate = iterate
  }

  @inlinable
  public func forEach<Error>(_ body: (Element) throws(Error) -> IterationContinuationBehavior) throws(Error) {
    var caughtError: Error? = nil
    self.iterate { (value) in
      do {
        return try body(value)
      } catch let error as Error {
        caughtError = error
        return .stop
      } catch {
        // Should never happen because `body` can only throw errors of the generic argument type `Error`.
        preconditionFailure("Caught error of unexpected type \(type(of: error))")
      }
    }
    if let caughtError {
      throw caughtError
    }
  }

  @inlinable
  public func map<Result, Error>(_ transform: (Element) throws(Error) -> Result) throws(Error) -> [Result] {
    return try self.compactMap { (value) throws(Error) in
      try transform(value)
    }
  }

  @inlinable
  public func compactMap<Result, Error>(_ transform: (Element) throws(Error) -> Result?) throws(Error) -> [Result] {
    var result: [Result] = []
    try self.forEach { (value) throws(Error) in
      if let transformed = try transform(value) {
        result.append(transformed)
      }
      return .continue
    }
    return result
  }
}

/// Allow calling `handleResult` from a C function pointer that takes a `void *` context pointer.
///
/// The `context` and `handleResult` parameters need to be passed to a function that drives the iteration of some data.
/// Every time the `handleResult` `@convention(c)` closure is called from `perform`, this gets translated to a call
/// to a call of the `handleResult` closure, which is a full Swift closure type and may also capture values.
///
/// `perform` must not escape the `context` parameter passed to it.
///
/// The `IterationContinuationBehavior` from `handleResult` is translated to a boolean value for `perform`.
@usableFromInline
func iterateWithClosureAsContextToCFunctionPointer<Result>(
  perform: (
    _ context: UnsafeMutableRawPointer,
    _ handleResult: @convention(c) (_ context: UnsafeMutableRawPointer?, _ result: indexstore_string_ref_t) -> Bool
  ) -> Result,
  handleResult: (indexstore_string_ref_t) -> IterationContinuationBehavior
) -> Result {
  typealias HandleResultType = (indexstore_string_ref_t) -> IterationContinuationBehavior
  return withoutActuallyEscaping(handleResult) { handleResult in
    return withUnsafePointer(to: handleResult) { (handleResultPointer) -> Result in
      perform(UnsafeMutableRawPointer(mutating: handleResultPointer)) { handleResultPointer, result in
        let iterationBehavior = handleResultPointer!.assumingMemoryBound(to: HandleResultType.self).pointee(result)
        switch iterationBehavior {
        case .continue: return true
        case .stop: return false
        }
      }
    }
  }
}

@usableFromInline
func iterateWithClosureAsContextToCFunctionPointer<Result>(
  perform: (
    _ context: UnsafeMutableRawPointer,
    _ handleResult: @convention(c) (_ context: UnsafeMutableRawPointer?, _ result: UnsafeMutableRawPointer?) -> Bool
  ) -> Result,
  handleResult: (UnsafeMutableRawPointer?) -> IterationContinuationBehavior
) -> Result {
  typealias HandleResultType = (UnsafeMutableRawPointer?) -> IterationContinuationBehavior
  return withoutActuallyEscaping(handleResult) { handleResult in
    return withUnsafePointer(to: handleResult) { (handleResultPointer) -> Result in
      perform(UnsafeMutableRawPointer(mutating: handleResultPointer)) { handleResultPointer, result in
        let iterationBehavior = handleResultPointer!.assumingMemoryBound(to: HandleResultType.self).pointee(result)
        switch iterationBehavior {
        case .continue: return true
        case .stop: return false
        }
      }
    }
  }
}
