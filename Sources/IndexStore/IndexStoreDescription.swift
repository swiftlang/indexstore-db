//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2026 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import Foundation

extension IndexStoreUnit: CustomStringConvertible {
  public var description: String {
    var result = """
      Module: \(moduleName.string)
      Has Main File: \(hasMainFile)
      Main File: \(mainFile.string)
      Output File: \(outputFile.string)
      Target: \(target.string)
      Sysroot: \(sysrootPath.string)
      Working Directory: \(workingDirectory.string)
      Is System: \(isSystemUnit)
      Is Module: \(isModuleUnit)
      Is Debug: \(isDebugCompilation)
      Provider Identifier: \(providerIdentifier.string)
      Provider Version: \(providerVersion.string)
      Mod Date: \(modificationDate)

      DEPENDENCIES START
      \(dependencies.map { dep in
        "\(String(describing: dep.kind).capitalized) | \(dep.name.string)"
      }.joined(separator: "\n"))
      DEPENDENCIES END
      """

    return result
  }
}

extension IndexStoreRecord: CustomStringConvertible {
  public var description: String {
    let symbolLines = symbols.map { symbol in
      "\(symbol.kind) | \(symbol.name.string) | USR: \(symbol.usr.string)"
    }

    var occurrencesLines: [String] = []
    occurrences.forEach { occurrence in
      occurrencesLines.append(
        "\(occurrence.position.line):\(occurrence.position.column) | \(occurrence.symbol.kind) | USR: \(occurrence.symbol.usr.string) | Roles: \(occurrence.roles)"
      )

      occurrence.relations.forEach { relation in
        occurrencesLines.append(
          "\tRelation | \(relation.symbol.usr.string) | Roles: \(relation.roles)")
        return .continue
      }
      return .continue
    }

    let allLines =
      ["SYMBOLS START"] + symbolLines + ["SYMBOLS END", "OCCURRENCES START"] + occurrencesLines
      + ["OCCURRENCES END"]
    return allLines.joined(separator: "\n")
  }
}
