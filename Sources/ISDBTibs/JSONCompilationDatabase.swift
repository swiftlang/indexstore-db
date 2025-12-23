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

import struct Foundation.URL

/// A JSON clang-compatible compilation database.
///
/// * Note: this only supports the "arguments" form, not "command".  It is primarily suitable for
/// serializing a compilation database. For a more complete version of the compilation database
/// suitable for reading an arbitrary compilation database written by another tool, see
/// SourceKit-LSP's JSONCompilationDatabase.
///
/// Example:
///
/// ```
/// [
///   {
///     "directory": "/src",
///     "file": "/src/file.cpp",
///     "arguments": ["clang++", "file.cpp"]
///   }
/// ]
/// ```
///
/// See https://clang.llvm.org/docs/JSONCompilationDatabase.html
public struct JSONCompilationDatabase: Equatable {

  /// A single compilation database command.
  ///
  /// See https://clang.llvm.org/docs/JSONCompilationDatabase.html
  public struct Command: Equatable, Codable {

    /// The working directory for the compilation.
    public var directory: String

    /// The path of the main file for the compilation, which may be relative to `directory`.
    public var file: String

    /// The compile command as a list of strings, with the program name first.
    public var arguments: [String]

    /// The name of the build output, or nil.
    public var output: String? = nil

    public init(directory: String, file: String, arguments: [String], output: String? = nil) {
      self.directory = directory
      self.file = file
      self.arguments = arguments
      self.output = output
    }
  }

  public var commands: [Command] = []

  public init(commands: [Command] = []) {
    self.commands = commands
  }
}

extension JSONCompilationDatabase.Command {

  /// The `URL` for this file. If `filename` is relative and `directory` is
  /// absolute, returns the concatenation. However, if both paths are relative,
  /// it falls back to `filename`, which is more likely to be the identifier
  /// that a caller will be looking for.
  public var url: URL {
    if file.hasPrefix("/") || !directory.hasPrefix("/") {
      return URL(fileURLWithPath: file)
    } else {
      return URL(fileURLWithPath: directory).appendingPathComponent(file, isDirectory: false)
    }
  }
}

extension JSONCompilationDatabase: Codable {
  public init(from decoder: Decoder) throws {
    self.commands = try [Command](from: decoder)
  }

  public func encode(to encoder: Encoder) throws {
    try self.commands.encode(to: encoder)
  }
}
