//===-- GenericOptional.cpp ----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//

#include "Generic.h"
#include "LibCxx.h"
#include "LibStdcpp.h"
#include "MsvcStl.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Target.h"
#include "llvm/Support/ErrorExtras.h"

using namespace lldb;
using namespace lldb_private;

bool lldb_private::formatters::GenericOptionalSummaryProvider(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  stream.Printf(" Has Value=%s ",
                valobj.GetNumChildrenIgnoringErrors() == 0 ? "false" : "true");

  return true;
}

// Synthetic Children Provider
namespace {

class GenericOptionalFrontend : public SyntheticChildrenFrontEnd {
public:
  enum class StdLib {
    LibCxx,
    LibStdcpp,
    MsvcStl,
  };

  GenericOptionalFrontend(ValueObject &valobj, StdLib stdlib);

  llvm::Expected<size_t> GetIndexOfChildWithName(ConstString name) override {
    if (name == "$$dereference$$" || name == "Value")
      return 0;
    return llvm::createStringErrorV("type has no child named '{0}'", name);
  }

  llvm::Expected<uint32_t> CalculateNumChildren() override {
    return m_has_value ? 1U : 0U;
  }

  ValueObjectSP GetChildAtIndex(uint32_t idx) override;
  lldb::ChildCacheState Update() override;

private:
  bool m_has_value = false;
  StdLib m_stdlib;
};

/// `std::optional<T&>` (C++26 and polyfills such as Beman Optional26) stores
/// a `T*` instead of an engaged flag plus payload. Null means empty.
static ValueObjectSP GetOptionalRefPointer(ValueObject &backend) {
  static constexpr llvm::StringLiteral kNames[] = {"value_", "_M_val",
                                                   "__value_"};
  for (llvm::StringRef name : kNames) {
    if (ValueObjectSP child = backend.GetChildMemberWithName(name)) {
      if (child->GetCompilerType().IsPointerType())
        return child;
    }
  }
  return {};
}

} // namespace

GenericOptionalFrontend::GenericOptionalFrontend(ValueObject &valobj,
                                                 StdLib stdlib)
    : SyntheticChildrenFrontEnd(valobj), m_stdlib(stdlib) {
  if (auto target_sp = m_backend.GetTargetSP()) {
    Update();
  }
}

lldb::ChildCacheState GenericOptionalFrontend::Update() {
  m_has_value = false;
  ValueObjectSP engaged_sp;

  if (m_stdlib == StdLib::LibCxx)
    engaged_sp = m_backend.GetChildMemberWithName("__engaged_");
  else if (m_stdlib == StdLib::LibStdcpp) {
    if (ValueObjectSP payload = m_backend.GetChildMemberWithName("_M_payload"))
      engaged_sp = payload->GetChildMemberWithName("_M_engaged");
  } else if (m_stdlib == StdLib::MsvcStl)
    engaged_sp = m_backend.GetChildMemberWithName("_Has_value");

  if (engaged_sp) {
    // _M_engaged/__engaged is a bool flag and is true if the optional contains
    // a value. Converting it to unsigned gives us a size of 1 if it contains a
    // value and 0 if not.
    m_has_value = engaged_sp->GetValueAsUnsigned(0) != 0;
    return lldb::ChildCacheState::eRefetch;
  }

  if (ValueObjectSP ptr_sp = GetOptionalRefPointer(m_backend))
    m_has_value = ptr_sp->GetValueAsUnsigned(0) != 0;

  return lldb::ChildCacheState::eRefetch;
}

ValueObjectSP GenericOptionalFrontend::GetChildAtIndex(uint32_t idx) {
  if (!m_has_value || idx != 0)
    return ValueObjectSP();

  if (ValueObjectSP ptr_sp = GetOptionalRefPointer(m_backend)) {
    bool success = false;
    uint64_t addr = ptr_sp->GetValueAsUnsigned(0, &success);
    if (!success || addr == 0)
      return ValueObjectSP();
    CompilerType pointee = ptr_sp->GetCompilerType().GetPointeeType();
    if (!pointee)
      return ValueObjectSP();
    ExecutionContext exe_ctx(m_backend.GetExecutionContextRef());
    return CreateChildValueObjectFromAddress("Value", addr, exe_ctx, pointee);
  }

  ValueObjectSP val_sp;

  if (m_stdlib == StdLib::LibCxx) {
    // __val_ contains the underlying value of an optional if it has one.
    // Currently because it is part of an anonymous union
    // GetChildMemberWithName() does not peer through and find it unless we are
    // at the parent itself. We can obtain the parent through __engaged_.
    ValueObjectSP engaged_sp = m_backend.GetChildMemberWithName("__engaged_");
    if (!engaged_sp)
      return ValueObjectSP();
    ValueObject *parent = engaged_sp->GetParent();
    if (!parent)
      return ValueObjectSP();
    ValueObjectSP first = parent->GetChildAtIndex(0);
    if (!first)
      return ValueObjectSP();
    val_sp = first->GetChildMemberWithName("__val_");
  } else if (m_stdlib == StdLib::LibStdcpp) {
    ValueObjectSP payload = m_backend.GetChildMemberWithName("_M_payload");
    if (!payload)
      return ValueObjectSP();
    val_sp = payload->GetChildMemberWithName("_M_payload");
    if (!val_sp)
      return ValueObjectSP();

    // In some implementations, _M_value contains the underlying value of an
    // optional, and in other versions, it's in the payload member.
    if (ValueObjectSP candidate = val_sp->GetChildMemberWithName("_M_value"))
      val_sp = candidate;
  } else if (m_stdlib == StdLib::MsvcStl) {
    // Same issue as with LibCxx
    ValueObjectSP has_value = m_backend.GetChildMemberWithName("_Has_value");
    if (!has_value)
      return ValueObjectSP();
    ValueObject *parent = has_value->GetParent();
    if (!parent)
      return ValueObjectSP();
    ValueObjectSP first = parent->GetChildAtIndex(0);
    if (!first)
      return ValueObjectSP();
    val_sp = first->GetChildMemberWithName("_Value");
  }

  if (!val_sp)
    return ValueObjectSP();

  CompilerType holder_type = val_sp->GetCompilerType();

  if (!holder_type)
    return ValueObjectSP();

  return val_sp->Clone("Value");
}

SyntheticChildrenFrontEnd *
formatters::LibStdcppOptionalSyntheticFrontEndCreator(
    CXXSyntheticChildren *, lldb::ValueObjectSP valobj_sp) {
  if (valobj_sp)
    return new GenericOptionalFrontend(
        *valobj_sp, GenericOptionalFrontend::StdLib::LibStdcpp);
  return nullptr;
}

SyntheticChildrenFrontEnd *formatters::LibcxxOptionalSyntheticFrontEndCreator(
    CXXSyntheticChildren *, lldb::ValueObjectSP valobj_sp) {
  if (valobj_sp)
    return new GenericOptionalFrontend(*valobj_sp,
                                       GenericOptionalFrontend::StdLib::LibCxx);
  return nullptr;
}

bool formatters::IsMsvcStlOptional(ValueObject &valobj) {
  if (auto valobj_sp = valobj.GetNonSyntheticValue())
    return valobj_sp->GetChildMemberWithName("_Has_value") != nullptr;
  return false;
}

SyntheticChildrenFrontEnd *formatters::MsvcStlOptionalSyntheticFrontEndCreator(
    CXXSyntheticChildren *, lldb::ValueObjectSP valobj_sp) {
  if (valobj_sp)
    return new GenericOptionalFrontend(
        *valobj_sp, GenericOptionalFrontend::StdLib::MsvcStl);
  return nullptr;
}
