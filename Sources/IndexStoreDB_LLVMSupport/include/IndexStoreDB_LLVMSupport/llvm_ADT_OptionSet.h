//===--- OptionSet.h - Sets of boolean options ------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the OptionSet class template.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ADT_OPTIONSET_H
#define LLVM_ADT_OPTIONSET_H

#include <IndexStoreDB_LLVMSupport/llvm_ADT_None.h>

#include <type_traits>
#include <cstdint>

namespace llvm {

/// The class template \c OptionSet captures a set of options stored as the
/// bits in an unsigned integral value.
///
/// Each option corresponds to a particular flag value in the provided
/// enumeration type (\c Flags). The option set provides ways to add options,
/// remove options, intersect sets, etc., providing a thin type-safe layer
/// over the underlying unsigned value.
///
/// \tparam Flags An enumeration type that provides the individual flags
/// for options. Each enumerator should have a power-of-two value, indicating
/// which bit it is associated with.
///
/// \tparam StorageType The unsigned integral type to use to store the flags
/// enabled within this option set. This defaults to the unsigned form of the
/// underlying type of the enumeration.
template<typename Flags,
         typename StorageType = typename std::make_unsigned<
                                  typename std::underlying_type<Flags>::type
                                 >::type>
class OptionSet {
  StorageType Storage;

public:
  /// Create an empty option set.
  OptionSet() : Storage() { }

  /// Create an empty option set.
  OptionSet(llvm::NoneType) : Storage() { }

  /// Create an option set with only the given option set.
  OptionSet(Flags flag) : Storage(static_cast<StorageType>(flag)) { }

  /// Create an option set from raw storage.
  explicit OptionSet(StorageType storage) : Storage(storage) { }

  /// Check whether an option set is non-empty.
  explicit operator bool() const { return Storage != 0; }

  /// Explicitly convert an option set to its underlying storage.
  explicit operator StorageType() const { return Storage; }

  /// Explicitly convert an option set to intptr_t, for use in
  /// llvm::PointerIntPair.
  ///
  /// This member is not present if the underlying type is bigger than
  /// a pointer.
  template <typename T = std::intptr_t>
  explicit operator typename std::enable_if<sizeof(StorageType) <= sizeof(T),
      std::intptr_t>::type () const {
    return static_cast<intptr_t>(Storage);
  }

  /// Retrieve the "raw" representation of this option set.
  StorageType toRaw() const { return Storage; }
  
  bool containsAny(OptionSet set) const {
    return static_cast<bool>(*this & set);
  }
  bool containsAll(OptionSet set) const {
    return (*this & set).toRaw() == set.toRaw();
  }
  
  /// Determine whether this option set contains all of the options in the
  /// given set.
  bool contains(OptionSet set) const {
    return !static_cast<bool>(set - *this);
  }

  /// Produce the union of two option sets.
  friend OptionSet operator|(OptionSet lhs, OptionSet rhs) {
    return OptionSet(lhs.Storage | rhs.Storage);
  }

  /// Produce the union of two option sets.
  friend OptionSet &operator|=(OptionSet &lhs, OptionSet rhs) {
    lhs.Storage |= rhs.Storage;
    return lhs;
 }

  /// Produce the intersection of two option sets.
  friend OptionSet operator&(OptionSet lhs, OptionSet rhs) {
    return OptionSet(lhs.Storage & rhs.Storage);
  }

  /// Produce the intersection of two option sets.
  friend OptionSet &operator&=(OptionSet &lhs, OptionSet rhs) {
    lhs.Storage &= rhs.Storage;
    return lhs;
  }

  /// Produce the difference of two option sets.
  friend OptionSet operator-(OptionSet lhs, OptionSet rhs) {
    return OptionSet(lhs.Storage & ~rhs.Storage);
  }

  /// Produce the intersection of two option sets.
  friend OptionSet &operator-=(OptionSet &lhs, OptionSet rhs) {
    lhs.Storage &= ~rhs.Storage;
    return lhs;
  }

private:
#ifndef _MSC_VER
  // This is crashing MSVC.
  template <typename T>
  static auto _checkResultTypeOperatorOr(T t) -> decltype(t | t) { return T(); }

  static void _checkResultTypeOperatorOr(...) {}

  static_assert(!std::is_same<decltype(_checkResultTypeOperatorOr(Flags())),
                              Flags>::value,
                "operator| should produce an OptionSet");
#endif
};

}

#endif
