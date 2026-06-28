#include <cassert>

#include "scope.hxx"

namespace transpiler::analyzer {

// -----------------------------------------------------------------------
// Scope
// -----------------------------------------------------------------------
std::optional<VarEntry> Scope::lookup(const std::string &name) const {
  auto it = vars_.find(name);
  if (it != vars_.end())
    return it->second;
  if (parent_)
    return parent_->lookup(name);
  return std::nullopt;
}

void Scope::set(const std::string &name, VarEntry entry) {
  vars_[name] = std::move(entry);
}

void Scope::setInParent(const std::string &name, VarEntry entry) {
  if (parent_)
    parent_->set(name, std::move(entry));
  // If no parent (already at global), set locally
  else
    vars_[name] = std::move(entry);
}

void Scope::unset(const std::string &name) { vars_.erase(name); }

bool Scope::hasLocal(const std::string &name) const {
  return vars_.count(name) > 0;
}

// -----------------------------------------------------------------------
// ScopeStack
// -----------------------------------------------------------------------
ScopeStack::ScopeStack() {
  // Global frame always exists
  frames_.push_back(std::make_unique<Scope>(nullptr));
}

void ScopeStack::pushScope() {
  Scope *parent = frames_.back().get();
  frames_.push_back(std::make_unique<Scope>(parent));
}

void ScopeStack::popScope() {
  assert(frames_.size() > 1 && "cannot pop global scope");
  frames_.pop_back();
}

std::optional<VarEntry> ScopeStack::lookup(const std::string &name) const {
  // Normal scope chain first
  auto result = frames_.back()->lookup(name);
  if (result)
    return result;
  // Then cache
  return lookupCache(name);
}

void ScopeStack::set(const std::string &name, VarEntry entry) {
  frames_.back()->set(name, std::move(entry));
}

void ScopeStack::setParent(const std::string &name, VarEntry entry) {
  frames_.back()->setInParent(name, std::move(entry));
}

void ScopeStack::unset(const std::string &name) { frames_.back()->unset(name); }

void ScopeStack::setCache(const std::string &name, VarEntry entry) {
  entry.origin = VarOrigin::Cache;
  cache_[name] = std::move(entry);
  // Cache also visible as normal var in current scope
  frames_.back()->set(name, cache_[name]);
}

std::optional<VarEntry> ScopeStack::lookupCache(const std::string &name) const {
  auto it = cache_.find(name);
  if (it != cache_.end())
    return it->second;
  return std::nullopt;
}

void ScopeStack::setEnv(const std::string &name, const std::string &value) {
  env_[name] = value;
}

std::optional<std::string>
ScopeStack::lookupEnv(const std::string &name) const {
  auto it = env_.find(name);
  if (it != env_.end())
    return it->second;
  return std::nullopt;
}

// -----------------------------------------------------------------------
// expand() — resolve ${VAR}, $ENV{VAR}, $CACHE{VAR} in a raw string.
// We do a single left-to-right pass, handling nesting with a stack.
// CMake allows nested refs: ${${OUTER}} — we handle one level of nesting.
// -----------------------------------------------------------------------
std::string ScopeStack::expand(const std::string &raw) const {
  std::string result;
  result.reserve(raw.size());
  size_t i = 0;

  while (i < raw.size()) {
    if (raw[i] != '$' || i + 1 >= raw.size()) {
      result += raw[i++];
      continue;
    }

    // $ENV{VAR}
    if (raw.substr(i + 1, 4) == "ENV{") {
      size_t end = raw.find('}', i + 5);
      if (end != std::string::npos) {
        std::string varName = raw.substr(i + 5, end - (i + 5));
        // Recursively expand the name itself
        varName = expand(varName);
        auto val = lookupEnv(varName);
        if (val)
          result += *val;
        // else leave empty — env var not set
        i = end + 1;
        continue;
      }
    }

    // $CACHE{VAR}
    if (raw.substr(i + 1, 6) == "CACHE{") {
      size_t end = raw.find('}', i + 7);
      if (end != std::string::npos) {
        std::string varName = raw.substr(i + 7, end - (i + 7));
        varName = expand(varName);
        auto val = lookupCache(varName);
        if (val)
          result += val->value;
        i = end + 1;
        continue;
      }
    }

    // ${VAR} — with one level of nesting support
    if (raw[i + 1] == '{') {
      size_t end = raw.find('}', i + 2);
      if (end != std::string::npos) {
        std::string varName = raw.substr(i + 2, end - (i + 2));
        // Expand the name (handles ${${X}})
        varName = expand(varName);
        auto val = lookup(varName);
        if (val)
          result += val->value;
        // else empty string — undefined var expands to ""
        i = end + 1;
        continue;
      }
    }

    // $<genexpr> — keep as-is, we don't evaluate generator expressions
    if (raw[i + 1] == '<') {
      int depth = 1;
      result += raw[i];
      result += raw[i + 1];
      i += 2;
      while (i < raw.size() && depth > 0) {
        if (raw[i] == '<')
          ++depth;
        else if (raw[i] == '>')
          --depth;
        result += raw[i++];
      }
      continue;
    }

    result += raw[i++];
  }
  return result;
}

// -----------------------------------------------------------------------
// seedBuiltins — populate well-known CMAKE_* variables
// -----------------------------------------------------------------------
void ScopeStack::seedBuiltins(const std::string &sourceDir,
                              const std::string &binaryDir,
                              const std::string &projectName) {
  auto makeVar = [](const std::string &val) {
    return VarEntry{val, VarOrigin::Builtin, "", false};
  };

  set("CMAKE_SOURCE_DIR", makeVar(sourceDir));
  set("CMAKE_CURRENT_SOURCE_DIR", makeVar(sourceDir));
  set("CMAKE_BINARY_DIR", makeVar(binaryDir));
  set("CMAKE_CURRENT_BINARY_DIR", makeVar(binaryDir));
  set("PROJECT_SOURCE_DIR", makeVar(sourceDir));
  set("PROJECT_BINARY_DIR", makeVar(binaryDir));
  set("PROJECT_NAME", makeVar(projectName));
  set("CMAKE_PROJECT_NAME", makeVar(projectName));

  // Common platform/compiler detection vars — seeded as empty
  // The condition_mapper will handle these symbolically
  set("CMAKE_SYSTEM_NAME", makeVar(""));
  set("CMAKE_CXX_COMPILER_ID", makeVar(""));
  set("CMAKE_BUILD_TYPE", makeVar(""));

  // True/false constants
  set("TRUE", makeVar("TRUE"));
  set("FALSE", makeVar("FALSE"));
  set("ON", makeVar("ON"));
  set("OFF", makeVar("OFF"));
  set("YES", makeVar("YES"));
  set("NO", makeVar("NO"));
  set("1", makeVar("1"));
  set("0", makeVar("0"));
}

} // namespace transpiler::analyzer
