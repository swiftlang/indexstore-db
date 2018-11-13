//===--- PatternMatching.cpp ----------------------------------------------===//
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

#include "IndexStoreDB/Support/PatternMatching.h"
#include "llvm/ADT/StringRef.h"

using namespace IndexStoreDB;

static bool matchesPatternSubstring(StringRef Input,
                                    StringRef Pattern,
                                    bool AnchorStart,
                                    bool AnchorEnd,
                                    bool IgnoreCase) {
  if (AnchorStart && AnchorEnd) {
    if (IgnoreCase)
      return Input.equals_lower(Pattern);
    else
      return Input.equals(Pattern);
  }
  if (AnchorStart) {
    if (IgnoreCase)
      return Input.startswith_lower(Pattern);
    else
      return Input.startswith(Pattern);
  }
  if (AnchorEnd) {
    if (IgnoreCase)
      return Input.endswith_lower(Pattern);
    else
      return Input.endswith(Pattern);
  }
  if (IgnoreCase) {
    size_t N = Pattern.size();
    if (N > Input.size())
      return false;

    for (size_t i = 0, e = Input.size() - N + 1; i != e; ++i)
      if (Input.substr(i, N).equals_lower(Pattern))
        return true;
    return false;
  } else {
    return Input.find(Pattern) != StringRef::npos;
  }
}

static char ascii_tolower(char x) {
  if (x >= 'A' && x <= 'Z')
    return x - 'A' + 'a';
  return x;
}

static bool matchesPatternSubsequence(StringRef Input,
                                      StringRef Pattern,
                                      bool AnchorStart,
                                      bool AnchorEnd,
                                      bool IgnoreCase) {
  if (Input.empty() || Pattern.empty())
    return false;

  auto equals = [&](char c1, char c2)->bool {
    if (IgnoreCase)
      return ascii_tolower(c1) == ascii_tolower(c2);
    else
      return c1 == c2;
  };

  if (Input.size() < Pattern.size())
    return false;

  if (AnchorStart) {
    if (!equals(Input[0], Pattern[0]))
      return false;
  }

  while (!Input.empty() && !Pattern.empty()) {
    if (equals(Input[0], Pattern[0])) {
      Pattern = Pattern.substr(1);
    }
    Input = Input.substr(1);
  }

  if (!Pattern.empty())
    return false;
  if (AnchorEnd && !Input.empty())
    return false;

  return true;
}

bool IndexStoreDB::matchesPattern(StringRef Input,
                               StringRef Pattern,
                               bool AnchorStart,
                               bool AnchorEnd,
                               bool Subsequence,
                               bool IgnoreCase) {
  if (Subsequence)
    return matchesPatternSubsequence(Input, Pattern, AnchorStart, AnchorEnd, IgnoreCase);
  else
    return matchesPatternSubstring(Input, Pattern, AnchorStart, AnchorEnd, IgnoreCase);
}
