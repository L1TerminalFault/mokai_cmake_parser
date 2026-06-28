#pragma once

#include <optional>
#include <string>

#include "ast/node.hxx"
#include "diagnostics/diagnostic.hxx"
#include "scope.hxx"

using namespace transpiler::diagnostics;
using namespace transpiler::ast;

namespace transpiler::analyzer {

// -----------------------------------------------------------------------
// ConditionMapper — translates CMake if() condition argument lists into
// mokai condition strings.
//
// CMake condition syntax is complex:
//   if(WIN32)                          → os == windows
//   if(UNIX)                           → os == linux || os == macos
//   if(APPLE)                          → os == macos
//   if(CMAKE_COMPILER_IS_GNUCXX)       → compiler == gcc
//   if(MSVC)                           → compiler == msvc
//   if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")  → compiler == clang
//   if(options.BUILD_VULKAN_BACKEND)   → options.BUILD_VULKAN_BACKEND == true
//   if(NOT WIN32)                      → os != windows
//   if(WIN32 AND UNIX)                 → os == windows && (os == linux || os ==
//   macos) if(CMAKE_BUILD_TYPE STREQUAL "Debug")       → config == debug
//
// We do best-effort mapping. Unknown conditions are emitted as a comment
// in the TOML output with a warning diagnostic.
//
// Result type: either a resolved mokai condition string, or nullopt
// meaning "could not map — emit as unknown".
// -----------------------------------------------------------------------
class ConditionMapper {
public:
  ConditionMapper(const ScopeStack &scope, DiagnosticReporter &reporter);

  // Main entry point: map a CMake condition arg list to a mokai string.
  // Returns nullopt if the condition can't be mapped statically.
  std::optional<std::string> map(const std::vector<Argument> &condArgs,
                                 const std::string &filepath,
                                 uint32_t line) const;

private:
  // Token stream over the condition args (already expanded)
  struct CondToken {
    std::string text;
    std::string upper; // pre-computed upper-case for keyword checks
  };

  using CondTokens = std::vector<CondToken>;

  // Parse an expression at a given precedence level
  std::optional<std::string> parseExpr(const CondTokens &toks, size_t &i) const;
  std::optional<std::string> parseOr(const CondTokens &toks, size_t &i) const;
  std::optional<std::string> parseAnd(const CondTokens &toks, size_t &i) const;
  std::optional<std::string> parseNot(const CondTokens &toks, size_t &i) const;
  std::optional<std::string> parseAtom(const CondTokens &toks, size_t &i) const;

  // Map a single identifier/keyword to a mokai condition fragment
  std::optional<std::string> mapSingleVar(const std::string &name,
                                          const std::string &upper) const;

  // Map a comparison:  LHS STREQUAL/MATCHES/... RHS
  std::optional<std::string> mapComparison(const std::string &lhs,
                                           const std::string &lhsUpper,
                                           const std::string &op,
                                           const std::string &rhs) const;

  const ScopeStack &scope_;
  DiagnosticReporter &reporter_;
};

} // namespace transpiler::analyzer
