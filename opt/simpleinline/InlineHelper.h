/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#pragma once

#include "DexClass.h"
#include "Transform.h"
#include "Resolver.h"

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * Helper class to inline a set of condidates.
 * Take a set of candidates and a scope and walk all instructions in scope
 * to find and inline all calls to candidate.
 * A resolver is used to map a method reference to a method definition.
 * Not all methods may be inlined both for restriction on the caller or the
 * callee.
 * Perform inlining bottom up.
 */
class MultiMethodInliner {
 public:
  MultiMethodInliner(
      std::vector<DexClass*>& scope,
      DexClasses& primary_dex,
      std::unordered_set<DexMethod*>& candidates,
      std::function<DexMethod*(DexMethod*, MethodSearch)> resolver);

  /**
   * attempt inlining for all candidates.
   */
  void inline_methods();

  /**
   * Return the count of unique inlined methods.
   */
  std::unordered_set<DexMethod*> get_inlined() const {
    return inlined;
  }

 private:
  /**
   * Inline all callees into caller.
   * Recurse in a callee if that has inlinable candidates of its own.
   * Inlining is bottom up.
   */
  void caller_inline(
      DexMethod* caller,
      std::vector<DexMethod*>& callees,
      std::unordered_set<DexMethod*>& visited);

  /**
   * Inline callees in the caller defined by InlineContext if is_inlinable
   * below returns true.
   */
  void inline_callees(
      InlineContext& inline_context, std::vector<DexMethod*>& callees);

  /**
   * Return true if the callee is inlinable into the caller.
   * The predicates below define the constraint for inlining.
   */
  bool is_inlinable(DexMethod* callee, DexMethod* caller);

  /**
   * Return true if the method is related to enum (java.lang.Enum and derived).
   * Cannot inline enum methods because they can be called by code we do
   * not own.
   */
  bool is_enum_method(DexMethod* callee);

  /**
   * Return true if inlining would cause the caller to have ore than 16
   * registers.
   */
  bool over_16regs(DexMethod* caller, DexMethod* callee);

  /**
   * Return true if the callee contains try/catch.
   * Try/catch regions may need to be merged with those of the caller.
   */
  bool has_try_catch(DexMethod* callee);

  /**
   * Return true if the callee contains certain opcodes that are difficult
   * or impossible to inline.
   * Some of the opcodes are defined by the methods below.
   */
  bool cannot_inline_opcodes(DexMethod* callee);

  /**
   * Return true if inlining would require a method called from the callee
   * (candidate) to turn into a virtual method (e.g. private to public).
   */
  bool create_vmethod(DexOpcode* insn);

  /**
   * Return true if a callee contains an invoke super to a different method
   * in the hierarchy.
   * invoke-super can only exist within the class the call lives in.
   */
  bool is_invoke_super(DexOpcode* insn);

  /**
   * Return true if a callee overrides one of the input registers.
   * Writing over an input registers may change the type of the registers
   * in the caller if the method was inlined and break invariants in the caller.
   */
  bool writes_ins_reg(DexOpcode* insn, uint16_t temp_regs);

  /**
   * Return true if the callee contains a call to an unknown virtual method.
   * We cannot determine the visibility of the method invoked and thus
   * we cannot inline as we could cause a verification error if the method
   * was package/protected and we move the call out of context.
   */
  bool unknown_virtual(DexOpcode* insn, DexMethod* context);

  /**
   * Return true if the callee contains a call to an unknown field.
   * We cannot determine the visibility of the field accessed and thus
   * we cannot inline as we could cause a verification error if the field
   * was package/protected and we move the access out of context.
   */
  bool unknown_field(DexOpcode* insn, DexMethod* context);

  /**
   * If caller is in the primary DEX and any opcode in callee refers to a
   * DexMember of some kind make sure all references live in the primary DEX.
   */
  bool refs_not_in_primary(DexMethod* context);

  /**
   * Change the visibility of members accessed in a callee as they are moved
   * to the caller context.
   */
  void change_visibility(DexMethod* callee);

 private:
  /**
   * Resolver function to map a method reference to a method definition.
   */
  std::function<DexMethod*(DexMethod*, MethodSearch)> resolver;

  /**
   * Set of classes in primary DEX.
   */
  std::unordered_set<DexType*> primary;

  /**
   * Inlined methods.
   */
  std::unordered_set<DexMethod*> inlined;

  //
  // Maps from callee to callers and reverse map from caller to callees.
  // Those are used to perform bottom up inlining.
  //
  std::unordered_map<DexMethod*, std::vector<DexMethod*>> callee_caller;
  std::unordered_map<DexMethod*, std::vector<DexMethod*>> caller_callee;

 private:
  /**
   * Info about inling.
   */
  struct InliningInfo {
    size_t calls_inlined{0};
    size_t recursive{0};
    size_t caller_tries{0};
    size_t not_found{0};
    size_t invoke_range{0};
    size_t enum_callee{0};
    size_t more_than_16regs{0};
    size_t try_catch_block{0};
    size_t throws{0};
    size_t array_data{0};
    size_t multi_ret{0};
    size_t need_vmethod{0};
    size_t invoke_super{0};
    size_t write_over_ins{0};
    size_t escaped_virtual{0};
    size_t non_pub_virtual{0};
    size_t escaped_field{0};
    size_t non_pub_field{0};
    size_t non_pub_ctor{0};
    size_t not_in_primary{0};
  };
  InliningInfo info;

 public:
  const InliningInfo& get_info() {
    return info;
  }
};