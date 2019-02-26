//===----------------------------------------------------------------------===//
//
// Author: Hans Liljestrand <hans.liljestrand@pm.me>
// Copyright (C) 2018 Secure Systems Group, Aalto University <ssg.aalto.fi>
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <iostream>
// LLVM includes
#include "AArch64.h"
#include "AArch64Subtarget.h"
#include "AArch64RegisterInfo.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/CodeGen/RegisterScavenging.h"
// PARTS includes
#include "llvm/PARTS/PartsTypeMetadata.h"
#include "llvm/PARTS/PartsEventCount.h"
#include "llvm/PARTS/Parts.h"
#include "PartsUtils.h"

#define DEBUG_TYPE "AArch64PartsEmulatedTimingPass"

STATISTIC(StatAutia, DEBUG_TYPE ": Num of emulated auti[ab] instructions");
STATISTIC(StatPacia, DEBUG_TYPE ": Num of emulated paci[ab] instructions");
STATISTIC(StatAuthBranch, DEBUG_TYPE " Num of emulated auti[ia] for branch instructions");

using namespace llvm;
using namespace llvm::PARTS;

namespace {
 class AArch64PartsEmulatedTimingPass : public MachineFunctionPass {

 public:
   static char ID;

   AArch64PartsEmulatedTimingPass() : MachineFunctionPass(ID) {}

   StringRef getPassName() const override { return DEBUG_TYPE; }

   bool doInitialization(Module &M) override;
   bool runOnMachineFunction(MachineFunction &) override;

 private:
  const AArch64Subtarget *STI = nullptr;
  const AArch64InstrInfo *TII = nullptr;
  PartsUtils_ptr  partsUtils = nullptr;
  inline bool handleInstruction(MachineFunction &MF, MachineBasicBlock &MBB, MachineBasicBlock::instr_iterator &MIi);
  void doMachineFunctionInit(MachineFunction &MF);
  inline bool isPartsIntrinsic(unsigned Opcode);
  void insertPACInstr(MachineBasicBlock &MBB, MachineInstr *MI, MachineOperand &dstReg, MachineOperand &modReg, const MCInstrDesc &InstrDesc);
//   void replaceBranchByAuthenticatedBranch(MachineBasicBlock &MBB, MachineInstr *MI_indcall, unsigned dst, unsigned mod) override;
  void addNops(MachineBasicBlock &MBB, MachineInstr *MI, unsigned ptrReg, unsigned modReg, const DebugLoc &DL);
  void lowerPARTSPACIA(MachineFunction &MF, MachineBasicBlock &MBB, MachineInstr &MI);
  void lowerPARTSIntrinsicCommon(MachineFunction &MF, MachineBasicBlock &MBB, MachineInstr &MI, const MCInstrDesc &InstrDesc);
 };

} // end anonymous namespace

FunctionPass *llvm::createAArch64PartsEmulatedTimingPass() {
   return new AArch64PartsEmulatedTimingPass();
}

char AArch64PartsEmulatedTimingPass::ID = 0;

void AArch64PartsEmulatedTimingPass::insertPACInstr(MachineBasicBlock &MBB,
                                         MachineInstr *MI,
                                         MachineOperand &dstReg,
                                         MachineOperand &modReg,
                                         const MCInstrDesc &InstrDesc) {
 addNops(MBB, MI, dstReg.getReg(), modReg.getReg(), MI->getDebugLoc());
}

#if 0
void AArch64PartsEmulatedTimingPass::replaceBranchByAuthenticatedBranch(MachineBasicBlock &MBB,
                                                                                MachineInstr *MI_indcall,
                                                                                unsigned dst,
                                                                                unsigned mod) {
 // FIXME: This might break if the pointer is reused elsewhere!!!
 addNops(MBB, MI_indcall, dst, mod, MI_indcall->getDebugLoc());
}

#endif
void AArch64PartsEmulatedTimingPass::addNops(MachineBasicBlock &MBB, MachineInstr *MI, unsigned ptrReg, unsigned modReg, const DebugLoc &DL) {
  BuildMI(MBB, MI, DL, TII->get(AArch64::EORXri), ptrReg).addReg(ptrReg).addImm(17);
  BuildMI(MBB, MI, DL, TII->get(AArch64::EORXri), ptrReg).addReg(ptrReg).addImm(37);
  BuildMI(MBB, MI, DL, TII->get(AArch64::EORXri), ptrReg).addReg(ptrReg).addImm(97);
  BuildMI(MBB, MI, DL, TII->get(AArch64::EORXrs), ptrReg).addReg(ptrReg).addReg(modReg).addImm(0);
}

bool AArch64PartsEmulatedTimingPass::doInitialization(Module &M) {
  return true;
}

void AArch64PartsEmulatedTimingPass::doMachineFunctionInit(MachineFunction &MF) {
  STI = &MF.getSubtarget<AArch64Subtarget>();
  TII = STI->getInstrInfo();
}

bool AArch64PartsEmulatedTimingPass::runOnMachineFunction(MachineFunction &MF) {
  bool found = false;

  doMachineFunctionInit(MF);

  for (auto &MBB : MF)
    for (auto MIi = MBB.instr_begin(), MIie = MBB.instr_end(); MIi != MIie; ++MIi)
      found = handleInstruction(MF, MBB, MIi) || found;

  return found;
}

/**
 *
 * @param MF
 * @param MBB
 * @param MIi
 * @return  return true when changing something, otherwise false
 */
inline bool AArch64PartsEmulatedTimingPass::handleInstruction(MachineFunction &MF,
                                                   MachineBasicBlock &MBB,
                                                   MachineBasicBlock::instr_iterator &MIi) {
  const auto MIOpcode = MIi->getOpcode();

  if (!isPartsIntrinsic(MIOpcode))
    return false;
  auto &MI = *MIi--;

  switch (MIOpcode) {
    default:
      llvm_unreachable("Unhandled PARTS intrinsic!!");
    case AArch64::PACIA:
      lowerPARTSPACIA(MF, MBB, MI);
      break;
#if 0
    case AArch64::PARTS_AUTIA:
      lowerPARTSAUTIA(MF, MBB, MI);
      break;
    case AArch64::PARTS_AUTCALL:
      lowerPARTSAUTCALL(MF, MBB, MI);
      break;
#endif
  }

  MI.removeFromParent(); // Remove pac instruction !
  MBB.dump();
  return true;
}

inline bool AArch64PartsEmulatedTimingPass::isPartsIntrinsic(unsigned Opcode) {
  switch (Opcode) {
    case AArch64::PACIA:
#if 0
    case AArch64::PARTS_AUTIA:
    case AArch64::PARTS_AUTCALL:
#endif
      return true;
  }

  return false;
}

void AArch64PartsEmulatedTimingPass::lowerPARTSPACIA(MachineFunction &MF,
                                                 MachineBasicBlock &MBB,
                                                 MachineInstr &MI) {
  lowerPARTSIntrinsicCommon(MF, MBB, MI, TII->get(AArch64::PACIA));
  ++StatPacia;
}

#if 0

void AArch64PartsEmulatedTimingPass::lowerPARTSAUTCALL(MachineFunction &MF,
                                                   MachineBasicBlock &MBB,
                                                   MachineInstr &MI) {

  MachineInstr *MI_indcall = findIndirectCallMachineInstr(MI.getNextNode());
  if (MI_indcall == nullptr)
    triggerCompilationErrorOrphanAUTCALL(MBB);

  const unsigned mod_orig = MI.getOperand(2).getReg();
  const unsigned dst = MI.getOperand(0).getReg();
  const unsigned mod = getFreeRegister(MBB, MI_indcall->getPrevNode(), MI);

  insertMovInstr(MBB, &MI, mod, mod_orig);

  replaceBranchByAuthenticatedBranch(MBB, MI_indcall, dst, mod);

  ++StatAuthBranch;
}

inline void AArch64PartsEmulatedTimingPass::triggerCompilationErrorOrphanAUTCALL(MachineBasicBlock &MBB) {
  DEBUG(MBB.dump());
  llvm_unreachable("failed to find BLR for AUTCALL");
}

void AArch64PartsEmulatedTimingPass::replaceBranchByAuthenticatedBranch(MachineBasicBlock &MBB,
                                                                    MachineInstr *MI_indcall,
                                                                    unsigned dst,
                                                                    unsigned mod) {
  auto &MCI = getIndirectCallMachineInstruction(MI_indcall);
  insertPACInstr(MBB, MI_indcall, dst, mod, MCI);
  MI_indcall->removeFromParent(); // Remove the replaced BR instruction
}

inline unsigned AArch64PartsEmulatedTimingPass::getFreeRegister(MachineBasicBlock &MBB,
                                                     MachineInstr *MI_from,
                                                     MachineInstr &MI_to) {
  RegScavenger RS;
  auto &RC = AArch64::GPR64commonRegClass;

  RS.enterBasicBlockEnd(MBB);
  RS.backward(MachineBasicBlock::iterator(MI_from));
  const unsigned reg = RS.scavengeRegisterBackwards(RC, MachineBasicBlock::iterator(MI_to), false, 0);
  RS.setRegUsed(reg); // Tell the Register Scavenger that the register is alive. Needed !?

  return reg;
}

inline const MCInstrDesc &AArch64PartsEmulatedTimingPass::getIndirectCallMachineInstruction(MachineInstr *MI_indcall) {

  if (isNormalIndirectCall(MI_indcall))
   return TII->get(AArch64::BLRAA);

  // This is a tail call return, and we need to use BRAA
  // (tail-call: ~optimizatoin where a tail-cal is converted to a direct call so that
  // the tail-called function can return immediately to the current callee, without
  // going through the currently active function.)

 return TII->get(AArch64::BRAA);
}

inline void AArch64PartsEmulatedTimingPass::lowerPARTSAUTIA(MachineFunction &MF,
                                                 MachineBasicBlock &MBB,
                                                 MachineInstr &MI) {
  lowerPARTSIntrinsicCommon(MF, MBB, MI, TII->get(AArch64::AUTIA));
  ++StatAutia;
}

#endif
void AArch64PartsEmulatedTimingPass::lowerPARTSIntrinsicCommon(MachineFunction &MF,
                                                    MachineBasicBlock &MBB,
                                                    MachineInstr &MI,
                                                    const MCInstrDesc &InstrDesc) {
  auto &mod = MI.getOperand(1);
  auto &dst = MI.getOperand(0);

  insertPACInstr(MBB, &MI, dst, mod, InstrDesc);
}

#if 0
inline MachineInstr *AArch64PartsEmulatedTimingPass::findIndirectCallMachineInstr(MachineInstr *MI) {
  while (MI != nullptr && !isIndirectCall(*MI))
    MI = MI->getNextNode();

  return MI;
}

inline bool AArch64PartsEmulatedTimingPass::isIndirectCall(const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
    case AArch64::BLR:        // Normal indirect call
    case AArch64::TCRETURNri: // Indirect tail call
      return true;
  }
  return false;
}

void AArch64PartsEmulatedTimingPass::insertPACInstr(MachineBasicBlock &MBB,
                                         MachineInstr *MI,
                                         unsigned dstReg,
                                         unsigned modReg,
                                         const MCInstrDesc &InstrDesc) {
  auto BMI = BuildMI(MBB, MI, MI->getDebugLoc(), InstrDesc);
  BMI.addUse(dstReg);
  BMI.addUse(modReg);
}

void AArch64PartsEmulatedTimingPass::insertPACInstr(MachineBasicBlock &MBB,
                                         MachineInstr *MI,
                                         MachineOperand &dstReg,
                                         MachineOperand &modReg,
                                         const MCInstrDesc &InstrDesc) {
  auto BMI = BuildMI(MBB, MI, MI->getDebugLoc(), InstrDesc);
  BMI.add(dstReg);
  BMI.add(modReg);
}


inline void AArch64PartsEmulatedTimingPass::insertMovInstr(MachineBasicBlock &MBB,
                                                MachineInstr *MI,
                                                unsigned dstReg,
                                                unsigned srcReg) {
  auto MOVMI = BuildMI(MBB, MI, MI->getDebugLoc(), TII->get(AArch64::ORRXrs), dstReg);
  MOVMI.addUse(AArch64::XZR);
  MOVMI.addUse(srcReg);
  MOVMI.addImm(0);
}

inline bool AArch64PartsEmulatedTimingPass::isNormalIndirectCall(const MachineInstr *MI) const {
  return MI->getOpcode() == AArch64::BLR;
}
#endif
