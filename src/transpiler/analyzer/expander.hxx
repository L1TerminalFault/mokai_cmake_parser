#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "ast/node.hxx"
#include "diagnostics/diagnostic.hxx"
#include "scope.hxx"

using namespace transpiler::diagnostics;
using namespace transpiler::ast;

namespace transpiler::analyzer {

// -----------------------------------------------------------------------
// MacroDef / FunctionDef — stored definition ready for expansion.
// Copies the body NodeList so we can re-expand on each call.
// -----------------------------------------------------------------------
struct CallableDef {
  enum class Kind { Macro, Function };
  Kind kind;
  std::string name;
  std::vector<std::string> params;
  // Raw body nodes — deep-copied from the definition site.
  // We store the original FileNode children; the expander re-visits them.
  std::vector<ASTNode *> bodyRefs; // non-owning, points into the original AST
};

// -----------------------------------------------------------------------
// Expander — resolves Argument values and expands macro/function calls.
//
// The Analyzer calls into the Expander for:
//   1. expandArg()   — turn an Argument into a concrete string using ScopeStack
//   2. expandList()  — split a CMake list string "a;b;c" into items
//   3. registerCallable() — store a function/macro definition
//   4. isCallable()  — check if a command name is a user-defined callable
//
// NOTE: Full function call expansion (calling into a FunctionDefNode body)
// is handled by the Analyzer's visitor loop, not here. The Expander only
// provides the resolution primitives.
// -----------------------------------------------------------------------
class Expander {
public:
  explicit Expander(ScopeStack &scope, DiagnosticReporter &reporter);

  // Expand a single Argument into a string, resolving all its parts.
  std::string expandArg(const Argument &arg) const;

  // Expand all args in a list and return resolved strings.
  std::vector<std::string> expandArgs(const std::vector<Argument> &args) const;

  // Split a CMake list string "a;b;c" into ["a","b","c"].
  // Escaped semicolons (\;) are treated as literal semicolons.
  static std::vector<std::string> splitList(const std::string &listStr);

  // Join a list of strings into a CMake list string "a;b;c".
  static std::string joinList(const std::vector<std::string> &items);

  // Register a user-defined macro or function.
  void registerCallable(const std::string &name, CallableDef def);

  // Check if a name is a registered callable.
  bool isCallable(const std::string &name) const;

  // Retrieve a callable definition (returns nullptr if not found).
  const CallableDef *getCallable(const std::string &name) const;

  // Evaluate a CMake boolean expression (for if() conditions).
  // Returns true/false/unknown as a tri-state.
  enum class TriBool { True, False, Unknown };
  TriBool evalBool(const std::string &value) const;

private:
  ScopeStack &scope_;
  DiagnosticReporter &reporter_;
  std::unordered_map<std::string, CallableDef> callables_;
};

} // namespace transpiler::analyzer
