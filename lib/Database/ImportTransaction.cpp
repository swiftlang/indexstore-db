//===--- ImportTransaction.cpp --------------------------------------------===//
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

#include "ImportTransactionImpl.h"
#include "DatabaseImpl.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Path.h"

using namespace IndexStoreDB;
using namespace IndexStoreDB::db;

ImportTransaction::Implementation::Implementation(DatabaseRef dbase)
  : DBase(std::move(dbase)) {
  Txn = lmdb::txn::begin(DBase->impl().getDBEnv());
}

IDCode ImportTransaction::Implementation::getUnitCode(StringRef unitName) {
  return makeIDCodeFromString(unitName);
}

IDCode ImportTransaction::Implementation::addProviderName(StringRef name, bool *wasInserted) {
  IDCode code = makeIDCodeFromString(name);
  lmdb::val key{&code, sizeof(code)};
  lmdb::val val{name.data(), name.size()};
  bool inserted = DBase->impl().getDBISymbolProviderNameByCode().put(Txn, key, val, MDB_NOOVERWRITE);
  if (wasInserted)
    *wasInserted = inserted;
  return code;
}

void ImportTransaction::Implementation::setProviderContainsTestSymbols(IDCode provider) {
  lmdb::val key{&provider, sizeof(provider)};
  lmdb::val val{nullptr, 0};
  DBase->impl().getDBISymbolProvidersWithTestSymbols().put(Txn, key, val, MDB_NOOVERWRITE);
}

bool ImportTransaction::Implementation::providerContainsTestSymbols(IDCode provider) {
  return DBase->impl().getDBISymbolProvidersWithTestSymbols().get(Txn, provider);
}

IDCode ImportTransaction::Implementation::addSymbolInfo(IDCode provider, StringRef USR, StringRef symbolName,
                                                        SymbolInfo symInfo,
                                                        SymbolRoleSet roles, SymbolRoleSet relatedRoles) {
  auto &db = DBase->impl();
  auto &dbiProvidersByUSR = db.getDBISymbolProvidersByUSR();

  IDCode usrCode = makeIDCodeFromString(USR);
  auto cursor = lmdb::cursor::open(Txn, dbiProvidersByUSR);

  ProviderForUSRData entry{provider, roles.toRaw(), relatedRoles.toRaw()};
  lmdb::val key{&usrCode, sizeof(usrCode)};
  lmdb::val value{&entry, sizeof(entry)};
  // Don't dirty the page if it's not updating.
  bool added = cursor.put(key, value, MDB_NODUPDATA);
  if (!added) {
    // Update roles if necessary.
    lmdb::val existingKey;
    lmdb::val existingValue;
    cursor.get(existingKey, existingValue, MDB_GET_CURRENT);
    const auto &existingData = *(ProviderForUSRData*)existingValue.data();
    if (existingData.Roles != entry.Roles || existingData.RelatedRoles != entry.RelatedRoles)
      cursor.put(key, value, MDB_CURRENT);
  }

  if (roles & (SymbolRoleSet(SymbolRole::Declaration)|SymbolRole::Definition)) {
    if (!symbolName.empty() && symInfo.includeInGlobalNameSearch()) {
      if (symbolName.size() > db.getMaxKeySize())
        symbolName = symbolName.substr(0, db.getMaxKeySize());
      db.getDBIUSRsBySymbolName().put(Txn, symbolName, usrCode, MDB_NODUPDATA);
    }

    auto globalKind = getGlobalSymbolKind(symInfo.Kind);
    if (globalKind.hasValue()) {
      db.getDBIUSRsByGlobalSymbolKind().put(Txn, globalKind.getValue(), usrCode, MDB_NODUPDATA);
    }
    if (symInfo.Properties.contains(SymbolProperty::UnitTest) &&
        roles.contains(SymbolRole::Definition)) {
      Optional<GlobalSymbolKind> unitTestGlobalKind;
      if (symInfo.isClassLikeOrExtension())
        unitTestGlobalKind = GlobalSymbolKind::TestClassOrExtension;
      else if (symInfo.Kind == SymbolKind::InstanceMethod)
        unitTestGlobalKind = GlobalSymbolKind::TestMethod;

      if (unitTestGlobalKind.hasValue()) {
        db.getDBIUSRsByGlobalSymbolKind().put(Txn, unitTestGlobalKind.getValue(), usrCode, MDB_NODUPDATA);
      }
    }
  }

  return usrCode;
}

IDCode ImportTransaction::Implementation::addFilePath(CanonicalFilePathRef canonFilePath) {
  return addFilePath(canonFilePath.getPath());
}

IDCode ImportTransaction::Implementation::addFilePath(StringRef filePath) {
  auto &db = DBase->impl();
  auto &dbiDirNames = db.getDBIDirNameByCode();
  auto &dbiFilenames = db.getDBIFilenameByCode();

  IDCode filePathCode = makeIDCodeFromString(filePath);
  IDCode dirCode;
  StringRef dirName = llvm::sys::path::parent_path(filePath);
  if (!dirName.empty()) {
    dirCode = makeIDCodeFromString(dirName);
    lmdb::val key{&dirCode, sizeof(dirCode)};
    lmdb::val val{dirName.data(), dirName.size()};
    dbiDirNames.put(Txn, key, val, MDB_NOOVERWRITE);
  }

  llvm::SmallString<64> dirCodeAndFilename;
  dirCodeAndFilename.resize(sizeof(IDCode));
  memcpy(dirCodeAndFilename.data(), &dirCode, sizeof(dirCode));
  dirCodeAndFilename += llvm::sys::path::filename(filePath);
  lmdb::val key{&filePathCode, sizeof(filePathCode)};
  lmdb::val val{dirCodeAndFilename.data(), dirCodeAndFilename.size()};
  dbiFilenames.put(Txn, key, val, MDB_NOOVERWRITE);

  if (!dirName.empty()) {
    auto &dbiPathsByDir = db.getDBIFilePathCodesByDir();
    lmdb::val key{&dirCode, sizeof(dirCode)};
    lmdb::val value{&filePathCode, sizeof(filePathCode)};
    dbiPathsByDir.put(Txn, key, value, MDB_NODUPDATA);
  }

  return filePathCode;
}


IDCode ImportTransaction::Implementation::addUnitFileIdentifier(StringRef unitFile) {
  return addFilePath(unitFile);
}

IDCode ImportTransaction::Implementation::addDirectory(CanonicalFilePathRef directory) {
  StringRef dirName = directory.getPath();
  IDCode dirCode = makeIDCodeFromString(dirName);
  lmdb::val key{&dirCode, sizeof(dirCode)};
  lmdb::val val{dirName.data(), dirName.size()};
  DBase->impl().getDBIDirNameByCode().put(Txn, key, val, MDB_NOOVERWRITE);
  return dirCode;
}

IDCode ImportTransaction::Implementation::addTargetName(StringRef target) {
  auto &db = DBase->impl();
  auto &targetNames = db.getDBITargetNameByCode();
  IDCode targetCode = makeIDCodeFromString(target);
  lmdb::val key{&targetCode, sizeof(targetCode)};
  lmdb::val val{target.data(), target.size()};
  targetNames.put(Txn, key, val, MDB_NOOVERWRITE);
  return targetCode;
}

IDCode ImportTransaction::Implementation::addModuleName(StringRef moduleName) {
  if (moduleName.empty()) {
    return IDCode{};
  }

  auto &db = DBase->impl();
  auto &moduleNames = db.getDBIModuleNameByCode();
  IDCode moduleCode = makeIDCodeFromString(moduleName);
  lmdb::val key{&moduleCode, sizeof(moduleCode)};
  lmdb::val val{moduleName.data(), moduleName.size()};
  moduleNames.put(Txn, key, val, MDB_NOOVERWRITE);
  return moduleCode;
}

void ImportTransaction::Implementation::addFileAssociationForProvider(IDCode provider, IDCode file, IDCode unit,
                                                                      llvm::sys::TimePoint<> modTime, IDCode module, bool isSystem) {
  auto &db = DBase->impl();
  auto &dbiFilesByProvider = db.getDBITimestampedFilesByProvider();

  auto cursor = lmdb::cursor::open(Txn, dbiFilesByProvider);

  uint64_t nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(modTime.time_since_epoch()).count();
  TimestampedFileForProviderData entry{file, unit, module, nanos, isSystem};
  lmdb::val key{&provider, sizeof(provider)};
  lmdb::val value{&entry, sizeof(entry)};
  bool added = cursor.put(key, value, MDB_NODUPDATA);
  if (!added) {
    // Update timestamp if more recent.
    lmdb::val existingKey;
    lmdb::val existingValue;
    cursor.get(existingKey, existingValue, MDB_GET_CURRENT);
    const auto &existingData = *(TimestampedFileForProviderData*)existingValue.data();
    llvm::sys::TimePoint<> existingModTime = llvm::sys::TimePoint<>(std::chrono::nanoseconds(existingData.NanoTime));
    if (modTime > existingModTime)
      cursor.put(key, value, MDB_CURRENT);
  }
}

bool ImportTransaction::Implementation::removeFileAssociationFromProvider(IDCode provider, IDCode file, IDCode unit) {
  auto &db = DBase->impl();
  auto &dbiFilesByProvider = db.getDBITimestampedFilesByProvider();
  auto cursor = lmdb::cursor::open(Txn, dbiFilesByProvider);

  TimestampedFileForProviderData entry{file, unit, IDCode(), 0, false};
  lmdb::val key{&provider, sizeof(provider)};
  lmdb::val value{&entry, sizeof(entry)};
  bool found = cursor.get(key, value, MDB_GET_BOTH_RANGE);
  if (!found)
    return true;

  unsigned count = cursor.count();
  lmdb::val existingKey;
  lmdb::val existingValue;
  cursor.get(existingKey, existingValue, MDB_GET_CURRENT);
  const auto &existingData = *(TimestampedFileForProviderData*)existingValue.data();
  if (existingData.FileCode == file && existingData.UnitCode == unit) {
    cursor.del();
    --count;
  }
  return count == 0;
}

UnitInfo ImportTransaction::Implementation::getUnitInfo(IDCode unitCode) {
  auto &db = DBase->impl();
  return db.getUnitInfo(unitCode, Txn);
}

void ImportTransaction::Implementation::addUnitInfo(const UnitInfo &info) {
  auto &db = DBase->impl();
  auto &dbiUnitInfoByCode = db.getDBIUnitInfoByCode();
  auto cursor = lmdb::cursor::open(Txn, dbiUnitInfoByCode);

  assert(static_cast<uint16_t>(info.UnitName.size()) == info.UnitName.size());
  assert(static_cast<uint32_t>(info.FileDepends.size()) == info.FileDepends.size());
  assert(static_cast<uint32_t>(info.UnitDepends.size()) == info.UnitDepends.size());
  assert(static_cast<uint32_t>(info.ProviderDepends.size()) == info.ProviderDepends.size());
  auto nanoTime = std::chrono::duration_cast<std::chrono::nanoseconds>(info.ModTime.time_since_epoch()).count();
  UnitInfoData infoData{ info.MainFileCode, info.OutFileCode, info.SysrootCode,
    info.TargetCode,
    nanoTime,
    static_cast<uint16_t>(info.UnitName.size()),
    uint8_t(info.SymProviderKind),
    info.HasMainFile, info.HasSysroot, info.IsSystem, info.HasTestSymbols,
    static_cast<uint32_t>(info.FileDepends.size()),
    static_cast<uint32_t>(info.UnitDepends.size()),
    static_cast<uint32_t>(info.ProviderDepends.size()),
  };

  size_t bufSize =
    sizeof(UnitInfoData) +
    sizeof(IDCode)*info.FileDepends.size() +
    sizeof(IDCode)*info.UnitDepends.size() +
    sizeof(UnitInfo::Provider)*info.ProviderDepends.size() +
    info.UnitName.size();

  // Pad bufSize out to a multiple of our minimum alignment.  This ensures that
  // when we read this data in `getUnitInfo` it is safe to return pointers to
  // the File/Unit/Provider dependency arrays directly.  Note: we actually need
  // size(key + data) to match this size, but in this case our key is already
  // an IDCode.
  static_assert((alignof(UnitInfoData) >= alignof(IDCode)) &&
                (sizeof(UnitInfoData) % alignof(IDCode) == 0),
                "misaligned IDCode");
  bufSize = llvm::alignTo(bufSize, alignof(UnitInfoData));

  lmdb::val key{&info.UnitCode, sizeof(info.UnitCode)};
  lmdb::val val{nullptr, bufSize};
  cursor.put(key, val, MDB_RESERVE);

  char *ptr = val.data();
  memcpy(ptr, &infoData, sizeof(infoData));
  ptr += sizeof(infoData);
  memcpy(ptr, info.FileDepends.data(), sizeof(IDCode)*info.FileDepends.size());
  ptr += sizeof(IDCode)*info.FileDepends.size();
  memcpy(ptr, info.UnitDepends.data(), sizeof(IDCode)*info.UnitDepends.size());
  ptr += sizeof(IDCode)*info.UnitDepends.size();
  memcpy(ptr, info.ProviderDepends.data(), sizeof(UnitInfo::Provider)*info.ProviderDepends.size());
  ptr += sizeof(UnitInfo::Provider)*info.ProviderDepends.size();
  memcpy(ptr, info.UnitName.data(), info.UnitName.size());
}

IDCode ImportTransaction::Implementation::addUnitFileDependency(IDCode unitCode, CanonicalFilePathRef filePathDep) {
  auto &db = DBase->impl();
  auto &dbiUnitByFileDependency = db.getDBIUnitByFileDependency();

  IDCode fileCode = addFilePath(filePathDep);
  lmdb::val key{&fileCode, sizeof(fileCode)};
  lmdb::val value{&unitCode, sizeof(unitCode)};
  dbiUnitByFileDependency.put(Txn, key, value, MDB_NODUPDATA);

  return fileCode;
}

IDCode ImportTransaction::Implementation::addUnitUnitDependency(IDCode unitCode, StringRef unitNameDep) {
  auto &db = DBase->impl();
  auto &dbiUnitByUnitDependency = db.getDBIUnitByUnitDependency();

  IDCode unitDepCode = makeIDCodeFromString(unitNameDep);
  lmdb::val key{&unitDepCode, sizeof(unitDepCode)};
  lmdb::val value{&unitCode, sizeof(unitCode)};
  dbiUnitByUnitDependency.put(Txn, key, value, MDB_NODUPDATA);

  return unitDepCode;
}

void ImportTransaction::Implementation::removeUnitFileDependency(IDCode unitCode, IDCode pathCode) {
  auto &db = DBase->impl();
  lmdb::val key{&pathCode, sizeof(pathCode)};
  lmdb::val value{&unitCode, sizeof(unitCode)};
  db.getDBIUnitByFileDependency().del(Txn, key, value);
}

void ImportTransaction::Implementation::removeUnitUnitDependency(IDCode unitCode, IDCode unitDepCode) {
  auto &db = DBase->impl();
  lmdb::val key{&unitDepCode, sizeof(unitDepCode)};
  lmdb::val value{&unitCode, sizeof(unitCode)};
  db.getDBIUnitByUnitDependency().del(Txn, key, value);
}

void ImportTransaction::Implementation::removeUnitData(IDCode unitCode) {
  std::vector<IDCode> FileDepends;
  std::vector<IDCode> UnitDepends;
  std::vector<UnitInfo::Provider> ProviderDepends;
  auto dbUnit = getUnitInfo(unitCode);
  if (dbUnit.isInvalid())
    return; // Does not exist.

  FileDepends.insert(FileDepends.end(), dbUnit.FileDepends.begin(), dbUnit.FileDepends.end());
  UnitDepends.insert(UnitDepends.end(), dbUnit.UnitDepends.begin(), dbUnit.UnitDepends.end());
  ProviderDepends.insert(ProviderDepends.end(), dbUnit.ProviderDepends.begin(), dbUnit.ProviderDepends.end());

  auto &db = DBase->impl();
  lmdb::val key{&unitCode, sizeof(unitCode)};
  db.getDBIUnitInfoByCode().del(Txn, key);

  for (auto code : FileDepends)
    removeUnitFileDependency(unitCode, code);
  for (auto code : UnitDepends)
    removeUnitUnitDependency(unitCode, code);
  for (auto &prov : ProviderDepends) {
    removeUnitFileDependency(unitCode, prov.FileCode);
    removeFileAssociationFromProvider(prov.ProviderCode, prov.FileCode, unitCode);
  }
}

void ImportTransaction::Implementation::removeUnitData(StringRef unitName) {
  return removeUnitData(makeIDCodeFromString(unitName));
}

void ImportTransaction::Implementation::commit() {
  Txn.commit();
}


ImportTransaction::ImportTransaction(DatabaseRef dbase)
  : Impl(new Implementation(std::move(dbase))) {}

ImportTransaction::~ImportTransaction() {}

IDCode ImportTransaction::getUnitCode(StringRef unitName) {
  return Impl->getUnitCode(unitName);
}

IDCode ImportTransaction::addProviderName(StringRef name, bool *wasInserted) {
  return Impl->addProviderName(name, wasInserted);
}

void ImportTransaction::setProviderContainsTestSymbols(IDCode provider) {
  return Impl->setProviderContainsTestSymbols(provider);
}

bool ImportTransaction::providerContainsTestSymbols(IDCode provider) {
  return Impl->providerContainsTestSymbols(provider);
}

IDCode ImportTransaction::addSymbolInfo(IDCode provider, StringRef USR, StringRef symbolName,
                                        SymbolInfo symInfo,
                                        SymbolRoleSet roles, SymbolRoleSet relatedRoles) {
  return Impl->addSymbolInfo(provider, USR, symbolName, symInfo, roles, relatedRoles);
}

IDCode ImportTransaction::addFilePath(CanonicalFilePathRef filePath) {
  return Impl->addFilePath(filePath);
}

IDCode ImportTransaction::addUnitFileIdentifier(StringRef unitFile) {
  return Impl->addUnitFileIdentifier(unitFile);
}

void ImportTransaction::removeUnitData(IDCode unitCode) {
  return Impl->removeUnitData(unitCode);
}

void ImportTransaction::removeUnitData(StringRef unitName) {
  return Impl->removeUnitData(unitName);
}

void ImportTransaction::commit() {
  return Impl->commit();
}

UnitDataImport::UnitDataImport(ImportTransaction &import, StringRef unitName, llvm::sys::TimePoint<> modTime)
: Import(import), UnitName(unitName), ModTime(modTime), IsSystem(false) {

  auto dbUnit = import._impl()->getUnitInfo(makeIDCodeFromString(unitName));
  UnitCode = dbUnit.UnitCode;
  IsUpToDate = false;
  IsMissing = dbUnit.isInvalid();
  if (IsMissing)
    return; // Does not already exist.

  IsSystem = dbUnit.IsSystem;
  HasTestSymbols = dbUnit.HasTestSymbols;
  SymProviderKind = dbUnit.SymProviderKind;
  PrevMainFileCode = dbUnit.MainFileCode;
  PrevOutFileCode = dbUnit.OutFileCode;
  PrevTargetCode = dbUnit.TargetCode;
  PrevSysrootCode = dbUnit.SysrootCode;

  if (dbUnit.ModTime == modTime) {
    IsUpToDate = true;
    return;
  }

  // The following keep track of previous entries so we can see if we need to add
  // the dependencies or not in the database.
  // The dependencies that are still present get removed from the sets, and what
  // remains gets removed from the database at the commit.
  PrevCombinedFileDepends.insert(dbUnit.FileDepends.begin(), dbUnit.FileDepends.end());
  PrevUnitDepends.insert(dbUnit.UnitDepends.begin(), dbUnit.UnitDepends.end());
  PrevProviderDepends.insert(dbUnit.ProviderDepends.begin(), dbUnit.ProviderDepends.end());
  for (auto prov : dbUnit.ProviderDepends) {
    PrevCombinedFileDepends.insert(prov.FileCode);
  }
}

UnitDataImport::~UnitDataImport() {
}

void UnitDataImport::setMainFile(CanonicalFilePathRef mainFile) {
  assert(!IsUpToDate);
  MainFile = mainFile;
}

void UnitDataImport::setOutFile(StringRef outFile) {
  assert(!IsUpToDate);
  OutFile = outFile;
}

void UnitDataImport::setSysroot(CanonicalFilePathRef sysroot) {
  assert(!IsUpToDate);
  Sysroot = sysroot;
}

void UnitDataImport::setIsSystemUnit(bool isSystem) {
  assert(!IsUpToDate);
  IsSystem = isSystem;
}

void UnitDataImport::setSymbolProviderKind(SymbolProviderKind K) {
  assert(!IsUpToDate);
  SymProviderKind = K;
}

void UnitDataImport::setTarget(StringRef T) {
  assert(!IsUpToDate);
  Target = T;
}

IDCode UnitDataImport::addFileDependency(CanonicalFilePathRef filePathDep) {
  assert(!IsUpToDate);
  IDCode pathCode = makeIDCodeFromString(filePathDep.getPath());
  FileDepends.push_back(pathCode);
  auto it = PrevCombinedFileDepends.find(pathCode);
  if (it == PrevCombinedFileDepends.end()) {
    Import._impl()->addUnitFileDependency(UnitCode, filePathDep);
  } else {
    PrevCombinedFileDepends.erase(it);
  }
  return pathCode;
}

IDCode UnitDataImport::addUnitDependency(StringRef unitNameDep) {
  assert(!IsUpToDate);
  IDCode unitDepCode = Import.getUnitCode(unitNameDep);
  UnitDepends.push_back(unitDepCode);
  auto it = PrevUnitDepends.find(unitDepCode);
  if (it == PrevUnitDepends.end()) {
    Import._impl()->addUnitUnitDependency(UnitCode, unitNameDep);
  } else {
    PrevUnitDepends.erase(it);
  }
  return unitDepCode;
}

IDCode UnitDataImport::addProviderDependency(StringRef providerName, CanonicalFilePathRef filePathDep, StringRef moduleName, bool isSystem, bool *isNewProvider) {
  assert(!IsUpToDate);
  IDCode providerCode = makeIDCodeFromString(providerName);
  IDCode pathCode = makeIDCodeFromString(filePathDep.getPath());
  IDCode moduleNameCode = Import._impl()->addModuleName(moduleName);
  UnitInfo::Provider prov{providerCode, pathCode};
  ProviderDepends.push_back(prov);
  {
    auto it = PrevProviderDepends.find(prov);
    if (it == PrevProviderDepends.end()) {
      IDCode providerCode2 = Import._impl()->addProviderName(providerName, isNewProvider);
      (void)providerCode2;
      assert(providerCode == providerCode2);
    } else {
      if (isNewProvider) {
        *isNewProvider = false;
      }
      PrevProviderDepends.erase(it);
    }
    // Even if the provider was associated with this unit before we still need to
    // re-associate it in order to update its mod-time.
    Import._impl()->addFileAssociationForProvider(providerCode, pathCode, UnitCode, ModTime, moduleNameCode, isSystem);
  }
  {
    auto it = PrevCombinedFileDepends.find(pathCode);
    if (it == PrevCombinedFileDepends.end()) {
      Import._impl()->addUnitFileDependency(UnitCode, filePathDep);
    } else {
      PrevCombinedFileDepends.erase(it);
    }
  }
  return providerCode;
}

void UnitDataImport::commit() {
  assert(!IsUpToDate);

  ImportTransaction::Implementation &import = *Import._impl();
  bool hasMainFile = false;
  IDCode mainFileCode;
  if (!MainFile.empty()) {
    hasMainFile = true;
    mainFileCode = makeIDCodeFromString(MainFile.getPath());
    if (mainFileCode != PrevMainFileCode)
      import.addFilePath(MainFile);
  }
  IDCode outFileCode;
  if (!OutFile.empty()) {
    outFileCode = makeIDCodeFromString(OutFile);
    if (outFileCode != PrevOutFileCode)
      import.addUnitFileIdentifier(OutFile);
  }
  bool hasSysroot = false;
  IDCode sysrootCode;
  if (!Sysroot.empty()) {
    hasSysroot = true;
    sysrootCode = makeIDCodeFromString(Sysroot.getPath());
    if (sysrootCode != PrevSysrootCode)
      import.addDirectory(Sysroot);
  }
  IDCode targetCode;
  if (!Target.empty()) {
    targetCode = makeIDCodeFromString(Target);
    if (targetCode != PrevTargetCode)
      import.addTargetName(Target);
  }

  // Update the `HasTestSymbols` value.
  HasTestSymbols = false;
  for (const UnitInfo::Provider &prov : ProviderDepends) {
    if (import.providerContainsTestSymbols(prov.ProviderCode)) {
      HasTestSymbols = true;
      break;
    }
  }

  UnitInfo info{
    UnitName,
    UnitCode,
    ModTime,
    outFileCode,
    mainFileCode,
    sysrootCode,
    targetCode,
    hasMainFile,
    hasSysroot,
    IsSystem.getValue(),
    HasTestSymbols.getValue(),
    SymProviderKind.getValue(),
    FileDepends,
    UnitDepends,
    ProviderDepends,
  };
  import.addUnitInfo(info);

  for (auto code : PrevCombinedFileDepends)
    import.removeUnitFileDependency(UnitCode, code);
  for (auto code : PrevUnitDepends)
    import.removeUnitUnitDependency(UnitCode, code);
  for (auto &prov : PrevProviderDepends)
    import.removeFileAssociationFromProvider(prov.ProviderCode, prov.FileCode, UnitCode);
}
