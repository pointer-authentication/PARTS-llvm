; ------------------------------------------------------------------------
; Author: Carlos Chinea Perez <carlos.chinea.perez@huawei.com>
; Author: Hans Liljestrand <hans.liljestrand@pm.me>
; Copyright (C) 2019 Huawei Technologies Oy (Finland) Co. Ltd
; Copyright (C) 2019 Secure Systems Group, Aalto University <ssg.aalto.fi>
;
; This file is distributed under the University of Illinois Open Source
; License. See LICENSE.TXT for details.
; ------------------------------------------------------------------------
; RUN: llc -mtriple=aarch64-none-linux-gnu -mattr=v8.3a -parts-dpi < %s | FileCheck %s
;

;CHECK:        pacda   [[PTR:x[0-9]+]], sp
;CHECK:        stp     [[PTR]], {{x[0-9]+}}, [sp]            // 8-byte Folded Spill
;CHECK:        nop
;CHECK:        ldp     [[PTR_RELOAD:x[0-9]+]], {{x[0-9]+}}, [sp]            // 8-byte Folded Reload
;CHECK:        autda   [[PTR_RELOAD]], sp

define i64 @Str64Ldr64(i64* nocapture %P, i64 %v, i64 %n) {
entry:
  call void @llvm.parts.data.pointer.argument.p0i64(i64* %P)
  %0 = bitcast i64* %P to i64*
  call void @llvm.parts.data.pointer.argument.p0i64(i64* %0)
  call void asm sideeffect "nop", "~{x0},~{x1},~{x2},~{x3},~{x4},~{x5},~{x6},~{x7},~{x8},~{x9},~{x10},~{x11},~{x12},~{x13},~{x14},~{x15},~{x16},~{x17},~{x18},~{x19},~{x20},~{x21},~{x22},~{x23},~{x24},~{x25},~{x26},~{x27},~{x28},~{x29},~{x30},~{x31},~{sp},~{lr},~{fp}"()
  %arrayidx0 = getelementptr inbounds i64, i64* %P, i64 1
  call void @llvm.parts.data.pointer.argument.p0i64(i64* %arrayidx0)
  store i64 %v, i64* %arrayidx0
  %arrayidx1 = getelementptr inbounds i64, i64* %0, i64 1
  call void @llvm.parts.data.pointer.argument.p0i64(i64* %arrayidx1)
  %1 = load i64, i64* %arrayidx1
  ret i64 %1
}

declare void @llvm.parts.data.pointer.argument.p0i64(i64*)
