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

public struct Makefile {
  public let outputs: [(target: Substring, deps: [Substring])]

  public init?(contents: String) {
    var makeOutputs: [(target: Substring, deps: [Substring])] = []

    let lines = contents.split(whereSeparator: { $0.isNewline })
    for line in lines {
      var deps: [Substring] = []
      let split = line.split(separator: ":", maxSplits: 1)

      guard let output = split.first, let depStr = split.last else {
        return nil
      }

      var prev: Character = "."
      var it = depStr.startIndex
      var strStart = it

      while it != depStr.endIndex {
        let curr = depStr[it]
        if curr == " " && prev != "\\" {
          let dep = depStr[strStart..<it]
          if !dep.isEmpty {
            deps.append(dep)
          }
          strStart = depStr.index(after: it)
        }
        prev = curr
        it = depStr.index(after: it)
      }

      let dep = depStr[strStart..<it]
      if !dep.isEmpty {
        deps.append(dep)
      }
      makeOutputs.append((target: output, deps: deps))
    }
    outputs = makeOutputs
  }

  public init?(path: URL) {
    guard let contents = try? String(contentsOf: path, encoding: .utf8) else {
      return nil
    }
    self.init(contents: contents)
  }

}
