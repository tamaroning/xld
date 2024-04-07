//===- LEB128.cpp - LEB128 utility functions implementation -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements some utility functions for encoding SLEB128 and
// ULEB128 values.
//
//===----------------------------------------------------------------------===//

#include "common/integers.h"

namespace xld {

/// Utility function to get the size of the ULEB128-encoded value.
unsigned get_uleb128_size(uint64_t Value) {
    unsigned Size = 0;
    do {
        Value >>= 7;
        Size += sizeof(int8_t);
    } while (Value);
    return Size;
}

/// Utility function to get the size of the SLEB128-encoded value.
unsigned get_sleb128_size(int64_t Value) {
    unsigned Size = 0;
    int Sign = Value >> (8 * sizeof(Value) - 1);
    bool IsMore;

    do {
        unsigned Byte = Value & 0x7f;
        Value >>= 7;
        IsMore = Value != Sign || ((Byte ^ Sign) & 0x40) != 0;
        Size += sizeof(int8_t);
    } while (IsMore);
    return Size;
}

} // namespace xld
