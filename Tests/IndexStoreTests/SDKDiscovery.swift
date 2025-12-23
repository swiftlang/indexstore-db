import Foundation
import ISDBTibs

private func xcrunMacOSSDKPath() -> String? {
  guard
    var path = try? Process.tibs_checkNonZeroExit(arguments: ["/usr/bin/xcrun", "--show-sdk-path", "--sdk", "macosx"])
  else {
    return nil
  }
  if path.last == "\n" {
    path = String(path.dropLast())
  }
  return path
}

/// The default sdk path to use.
package let defaultSDKPath: String? = {
  #if os(macOS)
  return xcrunMacOSSDKPath()
  #else
  return ProcessInfo.processInfo.environment["SDKROOT"]
  #endif
}()
