// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OrbitFunction.h"

#include <OrbitBase/Logging.h>

#include <map>

#include "Capture.h"
#include "ConnectionManager.h"
#include "Core.h"
#include "CoreApp.h"
#include "Log.h"
#include "OrbitModule.h"
#include "OrbitProcess.h"
#include "Params.h"
#include "Pdb.h"
#include "PrintVar.h"
#include "SamplingProfiler.h"
#include "Serialization.h"
#include "TcpServer.h"
#include "Utils.h"

#ifdef _WIN32
#include "SymbolUtils.h"
#endif

Function::Function(std::string_view name, std::string_view pretty_name,
                   uint64_t address, uint64_t load_bias, uint64_t size,
                   std::string_view file, uint32_t line)
    : name_(name),
      pretty_name_(pretty_name),
      address_(address),
      load_bias_(load_bias),
      size_(size),
      file_(file),
      line_(line) {
  ResetStats();
}

bool Function::Hookable() {
  if (Capture::IsLinuxData()) {
    return true;
  } else {
    // Don't allow hooking in asm implemented functions (strcpy, stccat...)
    // TODO: give this better thought.  Here is the theory:
    // Functions that loop back to first 5 bytes of instructions will explode as
    // the IP lands in the middle of the relative jump instruction...
    // Ideally, we would detect such a loop back and not allow hooking.
    if (file_.find(".asm") != std::string::npos) {
      return false;
    }

    CV_call_e conv = static_cast<CV_call_e>(calling_convention_);
    return ((conv == CV_CALL_NEAR_C || conv == CV_CALL_THISCALL) &&
            size_ >= 5) ||
           (GParams.m_AllowUnsafeHooking && size_ == 0);
  }
}

void Function::Select() {
  if (Hookable()) {
    LOG("Selected %s at 0x%" PRIx64 " (address_=0x%" PRIx64
        ", load_bias_= 0x%" PRIx64 ", base_address=0x%" PRIx64 ")",
        pretty_name_, GetVirtualAddress(), address_, load_bias_,
        module_base_address_);
    Capture::GSelectedFunctionsMap[GetVirtualAddress()] = this;
  }
}

void Function::UnSelect() {
  Capture::GSelectedFunctionsMap.erase(GetVirtualAddress());
}

bool Function::IsSelected() const {
  return Capture::GSelectedFunctionsMap.count(GetVirtualAddress()) > 0;
}

void Function::ResetStats() {
  if (stats_ == nullptr) {
    stats_ = std::make_shared<FunctionStats>();
  } else {
    stats_->Reset();
  }
}

void Function::UpdateStats(const Timer& timer) {
  if (stats_ != nullptr) {
    stats_->Update(timer);
  }
}

void Function::FindFile() {
#ifdef _WIN32
  LineInfo lineInfo;
  SymUtils::GetLineInfo(GetVirtualAddress(), lineInfo);
  if (lineInfo.m_File != "") file_ = lineInfo.m_File;
  file_ = ToLower(file_);
  line_ = lineInfo.m_Line;
#endif
}

const char* Function::GetCallingConventionString() {
  static const char* kCallingConventions[] = {
      "NEAR_C",       // CV_CALL_NEAR_C      = 0x00, // near right to left push,
                      // caller pops stack
      "FAR_C",        // CV_CALL_FAR_C       = 0x01, // far right to left push,
                      // caller pops stack
      "NEAR_PASCAL",  // CV_CALL_NEAR_PASCAL = 0x02, // near left to right
                      // push, callee pops stack
      "FAR_PASCAL",   // CV_CALL_FAR_PASCAL  = 0x03, // far left to right push,
                      // callee pops stack
      "NEAR_FAST",    // CV_CALL_NEAR_FAST   = 0x04, // near left to right push
                      // with regs, callee pops stack
      "FAR_FAST",     // CV_CALL_FAR_FAST    = 0x05, // far left to right push
                      // with regs, callee pops stack
      "SKIPPED",   // CV_CALL_SKIPPED     = 0x06, // skipped (unused) call index
      "NEAR_STD",  // CV_CALL_NEAR_STD    = 0x07, // near standard call
      "FAR_STD",   // CV_CALL_FAR_STD     = 0x08, // far standard call
      "NEAR_SYS",  // CV_CALL_NEAR_SYS    = 0x09, // near sys call
      "FAR_SYS",   // CV_CALL_FAR_SYS     = 0x0a, // far sys call
      "THISCALL",  // CV_CALL_THISCALL    = 0x0b, // this call (this passed in
                   // register)
      "MIPSCALL",  // CV_CALL_MIPSCALL    = 0x0c, // Mips call
      "GENERIC",   // CV_CALL_GENERIC     = 0x0d, // Generic call sequence
      "ALPHACALL",  // CV_CALL_ALPHACALL   = 0x0e, // Alpha call
      "PPCCALL",    // CV_CALL_PPCCALL     = 0x0f, // PPC call
      "SHCALL",     // CV_CALL_SHCALL      = 0x10, // Hitachi SuperH call
      "ARMCALL",    // CV_CALL_ARMCALL     = 0x11, // ARM call
      "AM33CALL",   // CV_CALL_AM33CALL    = 0x12, // AM33 call
      "TRICALL",    // CV_CALL_TRICALL     = 0x13, // TriCore Call
      "SH5CALL",    // CV_CALL_SH5CALL     = 0x14, // Hitachi SuperH-5 call
      "M32RCALL",   // CV_CALL_M32RCALL    = 0x15, // M32R Call
      "CLRCALL",    // CV_CALL_CLRCALL     = 0x16, // clr call
      "INLINE",     // CV_CALL_INLINE      = 0x17, // Marker for routines always
                    // inlined and thus lacking a convention
      "NEAR_VECTOR",  // CV_CALL_NEAR_VECTOR = 0x18, // near left to right push
                      // with regs, callee pops stack
      "RESERVED"};    // CV_CALL_RESERVED    = 0x19  // first unused call
                      // enumeration
  if (calling_convention_ < 0 ||
      static_cast<size_t>(calling_convention_) >=
          (sizeof(kCallingConventions) / sizeof(kCallingConventions[0]))) {
    return "UnknownCallConv";
  }

  return kCallingConventions[calling_convention_];
}

const absl::flat_hash_map<const char*, Function::OrbitType>&
Function::GetFunctionNameToOrbitTypeMap() {
  static absl::flat_hash_map<const char*, OrbitType> function_name_to_type_map{
      {"Start(", ORBIT_TIMER_START},
      {"Stop(", ORBIT_TIMER_STOP},
      {"StartAsync(", ORBIT_TIMER_START_ASYNC},
      {"StopAsync(", ORBIT_TIMER_STOP_ASYNC},
      {"TrackInt(", ORBIT_TRACK_INT},
      {"TrackInt64(", ORBIT_TRACK_INT_64},
      {"TrackUint(", ORBIT_TRACK_UINT},
      {"TrackUint64(", ORBIT_TRACK_UINT_64},
      {"TrackFloat(", ORBIT_TRACK_FLOAT},
      {"TrackDouble(", ORBIT_TRACK_DOUBLE},
      {"TrackFloatAsInt(", ORBIT_TRACK_FLOAT_AS_INT},
      {"TrackDoubleAsInt64(", ORBIT_TRACK_DOUBLE_AS_INT_64},
  };
  return function_name_to_type_map;
}

// Detect Orbit API functions by looking for special function names part of the
// orbit_api namespace. On a match, set the corresponding function type.
bool Function::SetOrbitTypeFromName() {
  const std::string& name = PrettyName();
  if (absl::StartsWith(name, "orbit_api::")) {
    for (auto& pair : GetFunctionNameToOrbitTypeMap()) {
      if (absl::StrContains(name, pair.first)) {
        SetOrbitType(pair.second);
        return true;
      }
    }
  }
  return false;
}

ORBIT_SERIALIZE(Function, 4) {
  ORBIT_NVP_VAL(4, name_);
  ORBIT_NVP_VAL(4, pretty_name_);
  ORBIT_NVP_VAL(4, loaded_module_path_);
  ORBIT_NVP_VAL(4, module_base_address_);
  ORBIT_NVP_VAL(4, address_);
  ORBIT_NVP_VAL(4, load_bias_);
  ORBIT_NVP_VAL(4, size_);
  ORBIT_NVP_VAL(4, file_);
  ORBIT_NVP_VAL(4, line_);
  ORBIT_NVP_VAL(4, calling_convention_);
  ORBIT_NVP_VAL(4, stats_);
}

FunctionParam::FunctionParam() {
#ifdef _WIN32
  memset(&m_SymbolInfo, 0, sizeof(m_SymbolInfo));
#endif
}

bool FunctionParam::InRegister(int a_Index) { return a_Index < 4; }

bool FunctionParam::IsFloat() {
  return (m_Type.find("float") != std::string::npos ||
          m_Type.find("double") != std::string::npos);
}

void Function::Print() {
  ORBIT_VIZV(address_);
  ORBIT_VIZV(file_);
  ORBIT_VIZV(line_);
  ORBIT_VIZV(IsSelected());

  if (!params_.empty()) {
    ORBIT_VIZ("\nParams:");
    for (auto& var : params_) {
      ORBIT_VIZV(var.m_Name);
      ORBIT_VIZV(var.m_Address);
      ORBIT_VIZV(var.m_ParamType);
      ORBIT_VIZV(var.m_Type);
    }
  }
}
