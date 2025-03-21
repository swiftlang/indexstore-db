//===--- MemoryBuffer.h - Memory Buffer Interface ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines the MemoryBuffer interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_MEMORYBUFFER_H
#define LLVM_SUPPORT_MEMORYBUFFER_H

#include <IndexStoreDB_LLVMSupport/llvm-c_Types.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_ArrayRef.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_StringRef.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_Twine.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_CBindingWrapping.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_ErrorOr.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_FileSystem.h>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace llvm {

class MemoryBufferRef;

/// This interface provides simple read-only access to a block of memory, and
/// provides simple methods for reading files and standard input into a memory
/// buffer.  In addition to basic access to the characters in the file, this
/// interface guarantees you can read one character past the end of the file,
/// and that this character will read as '\0'.
///
/// The '\0' guarantee is needed to support an optimization -- it's intended to
/// be more efficient for clients which are reading all the data to stop
/// reading when they encounter a '\0' than to continually check the file
/// position to see if it has reached the end of the file.
class MemoryBuffer {
  const char *BufferStart; // Start of the buffer.
  const char *BufferEnd;   // End of the buffer.

protected:
  MemoryBuffer() = default;

  void init(const char *BufStart, const char *BufEnd,
            bool RequiresNullTerminator);

  static constexpr sys::fs::mapped_file_region::mapmode Mapmode =
      sys::fs::mapped_file_region::readonly;

public:
  MemoryBuffer(const MemoryBuffer &) = delete;
  MemoryBuffer &operator=(const MemoryBuffer &) = delete;
  virtual ~MemoryBuffer();

  const char *getBufferStart() const { return BufferStart; }
  const char *getBufferEnd() const   { return BufferEnd; }
  size_t getBufferSize() const { return BufferEnd-BufferStart; }

  StringRef getBuffer() const {
    return StringRef(BufferStart, getBufferSize());
  }

  /// Return an identifier for this buffer, typically the filename it was read
  /// from.
  virtual StringRef getBufferIdentifier() const { return "Unknown buffer"; }

  /// Open the specified file as a MemoryBuffer, returning a new MemoryBuffer
  /// if successful, otherwise returning null. If FileSize is specified, this
  /// means that the client knows that the file exists and that it has the
  /// specified size.
  ///
  /// \param IsVolatile Set to true to indicate that the contents of the file
  /// can change outside the user's control, e.g. when libclang tries to parse
  /// while the user is editing/updating the file or if the file is on an NFS.
  static ErrorOr<std::unique_ptr<MemoryBuffer>>
  getFile(const Twine &Filename, int64_t FileSize = -1,
          bool RequiresNullTerminator = true, bool IsVolatile = false);

  /// Read all of the specified file into a MemoryBuffer as a stream
  /// (i.e. until EOF reached). This is useful for special files that
  /// look like a regular file but have 0 size (e.g. /proc/cpuinfo on Linux).
  static ErrorOr<std::unique_ptr<MemoryBuffer>>
  getFileAsStream(const Twine &Filename);

  /// Given an already-open file descriptor, map some slice of it into a
  /// MemoryBuffer. The slice is specified by an \p Offset and \p MapSize.
  /// Since this is in the middle of a file, the buffer is not null terminated.
  static ErrorOr<std::unique_ptr<MemoryBuffer>>
  getOpenFileSlice(int FD, const Twine &Filename, uint64_t MapSize,
                   int64_t Offset, bool IsVolatile = false);

  /// Given an already-open file descriptor, read the file and return a
  /// MemoryBuffer.
  ///
  /// \param IsVolatile Set to true to indicate that the contents of the file
  /// can change outside the user's control, e.g. when libclang tries to parse
  /// while the user is editing/updating the file or if the file is on an NFS.
  static ErrorOr<std::unique_ptr<MemoryBuffer>>
  getOpenFile(int FD, const Twine &Filename, uint64_t FileSize,
              bool RequiresNullTerminator = true, bool IsVolatile = false);

  /// Open the specified memory range as a MemoryBuffer. Note that InputData
  /// must be null terminated if RequiresNullTerminator is true.
  static std::unique_ptr<MemoryBuffer>
  getMemBuffer(StringRef InputData, StringRef BufferName = "",
               bool RequiresNullTerminator = true);

  static std::unique_ptr<MemoryBuffer>
  getMemBuffer(MemoryBufferRef Ref, bool RequiresNullTerminator = true);

  /// Open the specified memory range as a MemoryBuffer, copying the contents
  /// and taking ownership of it. InputData does not have to be null terminated.
  static std::unique_ptr<MemoryBuffer>
  getMemBufferCopy(StringRef InputData, const Twine &BufferName = "");

  /// Read all of stdin into a file buffer, and return it.
  static ErrorOr<std::unique_ptr<MemoryBuffer>> getSTDIN();

  /// Open the specified file as a MemoryBuffer, or open stdin if the Filename
  /// is "-".
  static ErrorOr<std::unique_ptr<MemoryBuffer>>
  getFileOrSTDIN(const Twine &Filename, int64_t FileSize = -1,
                 bool RequiresNullTerminator = true);

  /// Map a subrange of the specified file as a MemoryBuffer.
  static ErrorOr<std::unique_ptr<MemoryBuffer>>
  getFileSlice(const Twine &Filename, uint64_t MapSize, uint64_t Offset,
               bool IsVolatile = false);

  //===--------------------------------------------------------------------===//
  // Provided for performance analysis.
  //===--------------------------------------------------------------------===//

  /// The kind of memory backing used to support the MemoryBuffer.
  enum BufferKind {
    MemoryBuffer_Malloc,
    MemoryBuffer_MMap
  };

  /// Return information on the memory mechanism used to support the
  /// MemoryBuffer.
  virtual BufferKind getBufferKind() const = 0;

  MemoryBufferRef getMemBufferRef() const;
};

/// This class is an extension of MemoryBuffer, which allows copy-on-write
/// access to the underlying contents.  It only supports creation methods that
/// are guaranteed to produce a writable buffer.  For example, mapping a file
/// read-only is not supported.
class WritableMemoryBuffer : public MemoryBuffer {
protected:
  WritableMemoryBuffer() = default;

  static constexpr sys::fs::mapped_file_region::mapmode Mapmode =
      sys::fs::mapped_file_region::priv;

public:
  using MemoryBuffer::getBuffer;
  using MemoryBuffer::getBufferEnd;
  using MemoryBuffer::getBufferStart;

  // const_cast is well-defined here, because the underlying buffer is
  // guaranteed to have been initialized with a mutable buffer.
  char *getBufferStart() {
    return const_cast<char *>(MemoryBuffer::getBufferStart());
  }
  char *getBufferEnd() {
    return const_cast<char *>(MemoryBuffer::getBufferEnd());
  }
  MutableArrayRef<char> getBuffer() {
    return {getBufferStart(), getBufferEnd()};
  }

  static ErrorOr<std::unique_ptr<WritableMemoryBuffer>>
  getFile(const Twine &Filename, int64_t FileSize = -1,
          bool IsVolatile = false);

  /// Map a subrange of the specified file as a WritableMemoryBuffer.
  static ErrorOr<std::unique_ptr<WritableMemoryBuffer>>
  getFileSlice(const Twine &Filename, uint64_t MapSize, uint64_t Offset,
               bool IsVolatile = false);

  /// Allocate a new MemoryBuffer of the specified size that is not initialized.
  /// Note that the caller should initialize the memory allocated by this
  /// method. The memory is owned by the MemoryBuffer object.
  static std::unique_ptr<WritableMemoryBuffer>
  getNewUninitMemBuffer(size_t Size, const Twine &BufferName = "");

  /// Allocate a new zero-initialized MemoryBuffer of the specified size. Note
  /// that the caller need not initialize the memory allocated by this method.
  /// The memory is owned by the MemoryBuffer object.
  static std::unique_ptr<WritableMemoryBuffer>
  getNewMemBuffer(size_t Size, const Twine &BufferName = "");

private:
  // Hide these base class factory function so one can't write
  //   WritableMemoryBuffer::getXXX()
  // and be surprised that he got a read-only Buffer.
  using MemoryBuffer::getFileAsStream;
  using MemoryBuffer::getFileOrSTDIN;
  using MemoryBuffer::getMemBuffer;
  using MemoryBuffer::getMemBufferCopy;
  using MemoryBuffer::getOpenFile;
  using MemoryBuffer::getOpenFileSlice;
  using MemoryBuffer::getSTDIN;
};

/// This class is an extension of MemoryBuffer, which allows write access to
/// the underlying contents and committing those changes to the original source.
/// It only supports creation methods that are guaranteed to produce a writable
/// buffer.  For example, mapping a file read-only is not supported.
class WriteThroughMemoryBuffer : public MemoryBuffer {
protected:
  WriteThroughMemoryBuffer() = default;

  static constexpr sys::fs::mapped_file_region::mapmode Mapmode =
      sys::fs::mapped_file_region::readwrite;

public:
  using MemoryBuffer::getBuffer;
  using MemoryBuffer::getBufferEnd;
  using MemoryBuffer::getBufferStart;

  // const_cast is well-defined here, because the underlying buffer is
  // guaranteed to have been initialized with a mutable buffer.
  char *getBufferStart() {
    return const_cast<char *>(MemoryBuffer::getBufferStart());
  }
  char *getBufferEnd() {
    return const_cast<char *>(MemoryBuffer::getBufferEnd());
  }
  MutableArrayRef<char> getBuffer() {
    return {getBufferStart(), getBufferEnd()};
  }

  static ErrorOr<std::unique_ptr<WriteThroughMemoryBuffer>>
  getFile(const Twine &Filename, int64_t FileSize = -1);

  /// Map a subrange of the specified file as a ReadWriteMemoryBuffer.
  static ErrorOr<std::unique_ptr<WriteThroughMemoryBuffer>>
  getFileSlice(const Twine &Filename, uint64_t MapSize, uint64_t Offset);

private:
  // Hide these base class factory function so one can't write
  //   WritableMemoryBuffer::getXXX()
  // and be surprised that he got a read-only Buffer.
  using MemoryBuffer::getFileAsStream;
  using MemoryBuffer::getFileOrSTDIN;
  using MemoryBuffer::getMemBuffer;
  using MemoryBuffer::getMemBufferCopy;
  using MemoryBuffer::getOpenFile;
  using MemoryBuffer::getOpenFileSlice;
  using MemoryBuffer::getSTDIN;
};

class MemoryBufferRef {
  StringRef Buffer;
  StringRef Identifier;

public:
  MemoryBufferRef() = default;
  MemoryBufferRef(const MemoryBuffer& Buffer)
      : Buffer(Buffer.getBuffer()), Identifier(Buffer.getBufferIdentifier()) {}
  MemoryBufferRef(StringRef Buffer, StringRef Identifier)
      : Buffer(Buffer), Identifier(Identifier) {}

  StringRef getBuffer() const { return Buffer; }

  StringRef getBufferIdentifier() const { return Identifier; }

  const char *getBufferStart() const { return Buffer.begin(); }
  const char *getBufferEnd() const { return Buffer.end(); }
  size_t getBufferSize() const { return Buffer.size(); }
};

// Create wrappers for C Binding types (see CBindingWrapping.h).
DEFINE_SIMPLE_CONVERSION_FUNCTIONS(MemoryBuffer, LLVMMemoryBufferRef)

} // end namespace llvm

#endif // LLVM_SUPPORT_MEMORYBUFFER_H
