#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace transpiler::analyzer {

// -----------------------------------------------------------------------
// Variable origin — used by the analyzer to decide precedence and
// whether to emit a mokai option, a define, or a plain value.
// -----------------------------------------------------------------------
enum class VarOrigin {
  Normal,      // set(VAR value)
  Cache,       // set(VAR value CACHE ...)  or  option(VAR ...)
  Env,         // $ENV{VAR}
  ParentScope, // set(VAR value PARENT_SCOPE)
  Builtin,     // CMAKE_SOURCE_DIR, CMAKE_CXX_STANDARD, etc.
};

struct VarEntry {
  std::string value;
  VarOrigin origin;
  std::string docstring; // from option() or CACHE string
  bool isBool = false;   // true for option() and CACHE BOOL
};

// -----------------------------------------------------------------------
// Scope — one frame in the variable scope stack.
//
// CMake scope rules:
//   - Each function call pushes a new scope frame.
//   - Macros do NOT push a new scope (they expand in the caller's scope).
//   - set(VAR val PARENT_SCOPE) writes to the parent frame.
//   - include() and add_subdirectory() share the current scope
//     (though add_subdirectory has subtle differences we approximate).
//   - Cache variables are global and live in a separate flat map.
// -----------------------------------------------------------------------
class Scope {
public:
  explicit Scope(Scope *parent = nullptr) : parent_(parent) {}

  // Look up a normal variable — walks up the parent chain.
  std::optional<VarEntry> lookup(const std::string &name) const;

  // Set a variable in this frame.
  void set(const std::string &name, VarEntry entry);

  // Set in parent frame (PARENT_SCOPE semantics).
  void setInParent(const std::string &name, VarEntry entry);

  // Unset a variable in this frame only.
  void unset(const std::string &name);

  // Check if a variable is defined in this frame (not parent).
  bool hasLocal(const std::string &name) const;

  Scope *parent() { return parent_; }
  const Scope *parent() const { return parent_; }

  const std::unordered_map<std::string, VarEntry> &locals() const {
    return vars_;
  }

private:
  Scope *parent_;
  std::unordered_map<std::string, VarEntry> vars_;
};

// -----------------------------------------------------------------------
// ScopeStack — manages the entire scope chain plus the global cache.
//
// The Analyzer holds one ScopeStack for the entire translation unit.
// -----------------------------------------------------------------------
class ScopeStack {
public:
  ScopeStack();

  // Push/pop a function scope frame
  void pushScope();
  void popScope();

  // Variable lookup — checks normal scope chain first, then cache.
  std::optional<VarEntry> lookup(const std::string &name) const;

  // Set in current scope
  void set(const std::string &name, VarEntry entry);

  // Set in parent scope (PARENT_SCOPE)
  void setParent(const std::string &name, VarEntry entry);

  // Unset
  void unset(const std::string &name);

  // Cache variable operations (global, survives scope pops)
  void setCache(const std::string &name, VarEntry entry);
  std::optional<VarEntry> lookupCache(const std::string &name) const;

  // Environment variable (read-only from our perspective — best effort)
  void setEnv(const std::string &name, const std::string &value);
  std::optional<std::string> lookupEnv(const std::string &name) const;

  // Expand a raw string — replaces ${VAR}, $ENV{VAR}, $CACHE{VAR}.
  // Returns the expanded string. Unresolved refs are left as-is with a warning.
  std::string expand(const std::string &raw) const;

  // Expand an Argument's parts into a resolved string.
  // (forward declared — include node.hpp users call this)
  // std::string expandArg(const Argument& arg) const;

  // Seed well-known CMake builtins given a source directory
  void seedBuiltins(const std::string &sourceDir, const std::string &binaryDir,
                    const std::string &projectName);

  // All cache entries — the emitter uses this to find option() calls
  const std::unordered_map<std::string, VarEntry> &allCache() const {
    return cache_;
  }

  int depth() const { return static_cast<int>(frames_.size()); }

private:
  std::vector<std::unique_ptr<Scope>> frames_;       // frames_[0] = global
  std::unordered_map<std::string, VarEntry> cache_;  // CACHE variables
  std::unordered_map<std::string, std::string> env_; // $ENV snapshots
};

} // namespace transpiler::analyzer
