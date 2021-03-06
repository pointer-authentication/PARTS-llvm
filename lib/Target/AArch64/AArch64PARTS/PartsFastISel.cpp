//===----------------------------------------------------------------------===//
//
// Author: Hans Liljestrand <hans@liljestrand.dev>
// Copyright (c) 2018 Secure Systems Group, Aalto University <ssg.aalto.fi>
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "PartsFastISel.h"
#include "llvm/IR/Metadata.h"
#include "llvm/PARTS/Parts.h"
#include "llvm/PARTS/PartsLog.h"
#include "llvm/PARTS/PartsTypeMetadata.h"

using namespace llvm;

PartsFastISel::PartsFastISel(FunctionLoweringInfo &FuncInfo)
    : FuncInfo(FuncInfo),
      log(PartsLog::getLogger("PartsFastISel"))
{
  DEBUG_PA(log->enable());
}

PartsFastISel_ptr PartsFastISel::get(FunctionLoweringInfo &FuncInfo) {
  return std::make_shared<PartsFastISel>(FuncInfo);
}

void PartsFastISel::addPartsTypeMetadata(MachineInstrBuilder &MIB, MDNode *partsType) {
  assert(partsType != nullptr && "Expected a non-null MDNode pointer!");
  assert(isa<MDNode>(partsType) && "Expected an MDNode");
  assert(PartsTypeMetadata::isPartsTypeMetadataContainer(partsType) && "Not a PartsTypeMetadata MDNode!");
  
  MIB.addMetadata(partsType);
}

void PartsFastISel::addMetadataToStore(MachineInstrBuilder &MIB, MDNode *partsType) {
  if (partsType == nullptr) {
    DEBUG_PA(log->debug(FuncInfo.Fn->getName()) << "\t\t\t*** no metadata when emitting store\n");
    return;
  }
  DEBUG_PA(log->debug(FuncInfo.Fn->getName()) << "\t\t\t*** moving metadata to emitted store\n");

  addPartsTypeMetadata(MIB, partsType);
}


void PartsFastISel::addMetadataToLoad(MachineInstrBuilder &MIB, MDNode *partsType) {
  if (partsType == nullptr) {
    DEBUG_PA(log->debug(FuncInfo.Fn->getName()) << "\t\t\t*** no metadata when emitting LDR\n");
    return;
  }
  DEBUG_PA(log->debug(FuncInfo.Fn->getName()) << "\t\t\t*** moving metadata to emitted LDR\n");

  addPartsTypeMetadata(MIB, partsType);
}

void PartsFastISel::addMetadataToCall(MachineInstrBuilder &MIB, MDNode *partsType) {
  if (partsType == nullptr) {
    DEBUG_PA(log->debug(FuncInfo.Fn->getName()) << "\t\t\t*** no metadata when emitting call\n");
    return;
  }
  DEBUG_PA(log->debug(FuncInfo.Fn->getName()) << "\t\t\t*** moving metadata to emitted call\n");

  addPartsTypeMetadata(MIB, partsType);
}

void PartsFastISel::addMetadataToCall(MachineInstrBuilder &MIB, FastISel::CallLoweringInfo &CLI, unsigned reg)
{
  // prep for pauth instrumentation by transferring type_id info to emitted BLR

  PartsTypeMetadata_ptr partsType = nullptr;

  auto &C = FuncInfo.Fn->getContext();
  const Value *Callee = CLI.Callee;

  if (reg) {
    DEBUG_PA(log->debug(FuncInfo.Fn->getName()) << "\t\t\t*** preparing metadata to emitted branch instruction\n");
    partsType = PartsTypeMetadata::get(Callee->getType());
  } else {
    DEBUG_PA(log->debug(FuncInfo.Fn->getName()) << "\t\t\t*** setting ignore metadata to emitted branch instruction\n");
    partsType = PartsTypeMetadata::getIgnored();
  }

  addPartsTypeMetadata(MIB, partsType->getMDNode(C));
}
