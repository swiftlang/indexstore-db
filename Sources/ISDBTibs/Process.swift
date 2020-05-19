//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2019 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import Foundation

extension Process {

  enum TibsProcessError: Error {
    case nonZeroExit(TerminationReason, Int32)
    case invalidUTF8Output(Data)
  }

  /// Runs a subprocess and returns its output as a String if it has a zero exit.
  static func tibs_checkNonZeroExit(
    arguments: [String],
    environment: [String: String]? = nil
  ) throws -> String {
    let p = Process()
    let out = Pipe()

    if #available(macOS 10.13, *) {
      p.executableURL = URL(fileURLWithPath: arguments[0], isDirectory: false)
    } else {
      p.launchPath = arguments[0]
    }

    p.arguments = Array(arguments[1...])
    if let environment = environment {
      p.environment = environment
    }
    p.standardOutput = out

    if #available(macOS 10.13, *) {
      try p.run()
    } else {
      p.launch()
    }

    let data = out.fileHandleForReading.readDataToEndOfFile()
    p.waitUntilExit()

    if p.terminationReason != .exit || p.terminationStatus != 0 {
      throw TibsProcessError.nonZeroExit(p.terminationReason, p.terminationStatus)
    }

    guard let str = String(data: data, encoding: .utf8) else {
      throw TibsProcessError.invalidUTF8Output(data)
    }
    return str
  }
}
