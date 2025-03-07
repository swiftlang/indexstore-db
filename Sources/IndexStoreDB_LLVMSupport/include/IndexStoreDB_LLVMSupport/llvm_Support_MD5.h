/* -*- C++ -*-
 * This code is derived from (original license follows):
 *
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 * See md5.c for more information.
 */

#ifndef LLVM_SUPPORT_MD5_H
#define LLVM_SUPPORT_MD5_H

#include <IndexStoreDB_LLVMSupport/llvm_ADT_SmallString.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_StringRef.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_Endian.h>
#include <array>
#include <cstdint>

namespace llvm {

template <typename T> class ArrayRef;

class MD5 {
  // Any 32-bit or wider unsigned integer data type will do.
  typedef uint32_t MD5_u32plus;

  MD5_u32plus a = 0x67452301;
  MD5_u32plus b = 0xefcdab89;
  MD5_u32plus c = 0x98badcfe;
  MD5_u32plus d = 0x10325476;
  MD5_u32plus hi = 0;
  MD5_u32plus lo = 0;
  uint8_t buffer[64];
  MD5_u32plus block[16];

public:
  struct MD5Result {
    std::array<uint8_t, 16> Bytes;

    operator std::array<uint8_t, 16>() const { return Bytes; }

    const uint8_t &operator[](size_t I) const { return Bytes[I]; }
    uint8_t &operator[](size_t I) { return Bytes[I]; }

    SmallString<32> digest() const;

    uint64_t low() const {
      // Our MD5 implementation returns the result in little endian, so the low
      // word is first.
      using namespace support;
      return endian::read<uint64_t, little, unaligned>(Bytes.data());
    }

    uint64_t high() const {
      using namespace support;
      return endian::read<uint64_t, little, unaligned>(Bytes.data() + 8);
    }
    std::pair<uint64_t, uint64_t> words() const {
      using namespace support;
      return std::make_pair(high(), low());
    }
  };

  MD5();

  /// Updates the hash for the byte stream provided.
  void update(ArrayRef<uint8_t> Data);

  /// Updates the hash for the StringRef provided.
  void update(StringRef Str);

  /// Finishes off the hash and puts the result in result.
  void final(MD5Result &Result);

  /// Translates the bytes in \p Res to a hex string that is
  /// deposited into \p Str. The result will be of length 32.
  static void stringifyResult(MD5Result &Result, SmallString<32> &Str);

  /// Computes the hash for a given bytes.
  static std::array<uint8_t, 16> hash(ArrayRef<uint8_t> Data);

private:
  const uint8_t *body(ArrayRef<uint8_t> Data);
};

inline bool operator==(const MD5::MD5Result &LHS, const MD5::MD5Result &RHS) {
  return LHS.Bytes == RHS.Bytes;
}

/// Helper to compute and return lower 64 bits of the given string's MD5 hash.
inline uint64_t MD5Hash(StringRef Str) {
  using namespace support;

  MD5 Hash;
  Hash.update(Str);
  MD5::MD5Result Result;
  Hash.final(Result);
  // Return the least significant word.
  return Result.low();
}

} // end namespace llvm

#endif // LLVM_SUPPORT_MD5_H
