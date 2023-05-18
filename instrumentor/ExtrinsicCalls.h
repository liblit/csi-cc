//===------------------------- ExtrinsicCalls.h ---------------------------===//
//
// Utilities for iterating over non-intrinsic call instructions.
//
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023 Peter J. Ohmann and Benjamin R. Liblit
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
//===----------------------------------------------------------------------===//
#ifndef CSI_EXTRINSIC_CALLS_H
#define CSI_EXTRINSIC_CALLS_H

#include "llvm_proxy/Function.h"
#include "llvm_proxy/InstIterator.h"
#include "llvm_proxy/Instructions.h"

#include <cassert>


namespace csi_inst
{
  // utility for iterating over non-intrinsic call instructions
  template <typename InstructionIterator>
  class ExtrinsicCalls
  {
  public:
    ExtrinsicCalls(const InstructionIterator &begin,
                   const InstructionIterator &end);

    class iterator
    {
    public:
      iterator(const InstructionIterator &current, const InstructionIterator &end);

      iterator &operator++();
      operator llvm::CallInst *() const;
      llvm::CallInst &operator*() const;
      bool operator!=(const iterator &) const;
      bool operator==(const iterator &) const;

    private:
      InstructionIterator current;
      const InstructionIterator end;

      bool included() const;
      void advance();
    };

    const iterator &begin() const;
    const iterator &end() const;

  private:
    const iterator begin_;
    const iterator end_;
  };


  ExtrinsicCalls<llvm::BasicBlock::iterator> extrinsicCalls(llvm::BasicBlock &);
  ExtrinsicCalls<llvm::inst_iterator> extrinsicCalls(llvm::Function &);
}


////////////////////////////////////////////////////////////////////////


template <typename InstructionIterator>
csi_inst::ExtrinsicCalls<InstructionIterator>::iterator::iterator(const InstructionIterator &current,
                                                                  const InstructionIterator &end)
  : current(current),
    end(end)
{
  if (!included()) advance();
}


template <typename InstructionIterator> inline
typename csi_inst::ExtrinsicCalls<InstructionIterator>::iterator &
csi_inst::ExtrinsicCalls<InstructionIterator>::iterator::operator++()
{
  advance();
  return *this;
}


template <typename InstructionIterator> inline
csi_inst::ExtrinsicCalls<InstructionIterator>::iterator::operator llvm::CallInst *() const
{
  assert(current != end);
  llvm::Instruction * const instruction = &*current;
  assert(llvm::isa<llvm::CallInst>(instruction));
  return static_cast<llvm::CallInst *>(instruction);
}


template <typename InstructionIterator> inline
llvm::CallInst &
csi_inst::ExtrinsicCalls<InstructionIterator>::iterator::operator*() const
{
  llvm::CallInst * const instruction = *this;
  return *instruction;
}


template <typename InstructionIterator> inline
bool
csi_inst::ExtrinsicCalls<InstructionIterator>::iterator::operator!=(const iterator &other) const
{
  return !(*this == other);
}

template <typename InstructionIterator> inline
bool
csi_inst::ExtrinsicCalls<InstructionIterator>::iterator::operator==(const iterator &other) const
{
  return current == other.current;
}


template <typename InstructionIterator>
bool csi_inst::ExtrinsicCalls<InstructionIterator>::iterator::included() const
{
  if (current == end) return true;
  const llvm::CallInst * const callInst = llvm::dyn_cast<llvm::CallInst>(&*current);
  if (!callInst) return false;
  const llvm::Function * const callee = callInst->getCalledFunction();
  if (callee && callee->isIntrinsic()) return false;
  return true;
}


template <typename InstructionIterator>
void csi_inst::ExtrinsicCalls<InstructionIterator>::iterator::advance()
{
  if (current != end) ++current;
  if (!included()) advance();
}


////////////////////////////////////////////////////////////////////////


template <typename InstructionIterator>
csi_inst::ExtrinsicCalls<InstructionIterator>::ExtrinsicCalls(const InstructionIterator &begin,
                                                              const InstructionIterator &end)
  : begin_(begin, end),
    end_(end, end)
{
}


template <typename InstructionIterator> inline
const typename csi_inst::ExtrinsicCalls<InstructionIterator>::iterator &
csi_inst::ExtrinsicCalls<InstructionIterator>::begin() const
{
  return begin_;
}


template <typename InstructionIterator> inline
const typename csi_inst::ExtrinsicCalls<InstructionIterator>::iterator &
csi_inst::ExtrinsicCalls<InstructionIterator>::end() const
{
  return end_;
}


#endif // !CSI_EXTRINSIC_CALLS_H
