// Copyright 2022 Risc0, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "risc0/zkp/circuit/code_regs.h"
#include "risc0/zkp/circuit/types.h"

namespace risc0 {

struct ResultInfoRegs {
  RegU32 result;
  Reg setReg;
  Reg doStore;
  RegU32 pcRaw;

  ResultInfoRegs(BufAlloc& alloc) : result(alloc), setReg(alloc), doStore(alloc), pcRaw(alloc) {}
};

} // namespace risc0
