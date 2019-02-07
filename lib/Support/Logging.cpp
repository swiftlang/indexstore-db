//===--- Logging.cpp ------------------------------------------------------===//
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

#include "Logging_impl.h"
#include "IndexStoreDB/Support/Logging.h"
#include "llvm/Config/config.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/Threading.h"

#include <dispatch/dispatch.h>

using namespace IndexStoreDB;

void IndexStoreDB::writeEscaped(StringRef Str, raw_ostream &OS) {
  for (unsigned i = 0, e = Str.size(); i != e; ++i) {
    unsigned char c = Str[i];

    switch (c) {
    case '\\':
      OS << '\\' << '\\';
      break;
    case '\t':
      OS << '\\' << 't';
      break;
    case '\n':
      OS << '\\' << 'n';
      break;
    case '"':
      OS << '\\' << '"';
      break;
    default:
      OS << c;
      break;
    }
  }
}

std::string Logger::LoggerName;
std::atomic<Logger::Level> Logger::LoggingLevel{Logger::Level::None};

void Logger::enableLoggingByEnvVar(const char *EnvVarName,
                                   StringRef LoggerName) {
  Level LogLevel = Level::Warning;
  const char *EnvOpt = ::getenv(EnvVarName);
  if (EnvOpt) {
    unsigned Val;
    bool Err = StringRef(EnvOpt).getAsInteger(10, Val);
    if (!Err) {
      LogLevel = getLogLevelByNum(Val);
      if (Val > 2)
        LogLevel = Logger::Level::InfoLowPrio;
      else if (Val == 2)
        LogLevel = Logger::Level::InfoMediumPrio;
      else if (Val == 1)
        LogLevel = Logger::Level::InfoHighPrio;
    }
  }

  enableLogging(LoggerName, LogLevel);
}

Logger::Level Logger::getLogLevelByNum(unsigned LevelNum) {
  Level LogLevel = Level::Warning;
  if (LevelNum > 2)
    LogLevel = Logger::Level::InfoLowPrio;
  else if (LevelNum == 2)
    LogLevel = Logger::Level::InfoMediumPrio;
  else if (LevelNum == 1)
    LogLevel = Logger::Level::InfoHighPrio;

  return LogLevel;
}

unsigned Logger::getCurrentLogLevelNum() {
  switch (LoggingLevel.load()) {
    case Level::None:
    case Level::Warning:
      return 0;
    case Level::InfoHighPrio:
      return 1;
    case Level::InfoMediumPrio:
      return 2;
    case Level::InfoLowPrio:
      return 3;
  }
}

Logger &Logger::operator<<(const llvm::format_object_base &Fmt) {
  LogOS << Fmt;
  return *this;
}

Logger::Logger(StringRef Name, Level LogLevel)
  : Name(Name), CurrLevel(LogLevel), LogOS(Msg) {

  thread_id = llvm::get_threadid();
  TimeR = llvm::TimeRecord::getCurrentTime();
}

Logger::~Logger() {
  static llvm::TimeRecord sBeginTR = llvm::TimeRecord::getCurrentTime();

  SmallString<64> LogMsg;
  llvm::raw_svector_ostream LogMsgOS(LogMsg);
  raw_ostream &OS = LogMsgOS;

  OS << '[' << int(CurrLevel) << ':' << Name << ':';
  OS << thread_id << ':';
  OS << llvm::format("%7.4f] ", TimeR.getWallTime() - sBeginTR.getWallTime());
  OS << LogOS.str();
  OS.flush();

  Log_impl(LoggerName.c_str(), LogMsg.c_str());
}
