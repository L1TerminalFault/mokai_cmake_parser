#include <cctype>

#include "condition_mapper.hxx"
#include "expander.hxx"

namespace transpiler::analyzer {

namespace {

std::string toUpper(const std::string &s) {
  std::string u;
  u.reserve(s.size());
  for (char c : s)
    u += (char)std::toupper((unsigned char)c);
  return u;
}
std::string toLower(const std::string &s) {
  std::string l;
  l.reserve(s.size());
  for (char c : s)
    l += (char)std::tolower((unsigned char)c);
  return l;
}

} // namespace

ConditionMapper::ConditionMapper(const ScopeStack &scope,
                                 DiagnosticReporter &reporter)
    : scope_(scope), reporter_(reporter) {}

// -----------------------------------------------------------------------
// map() — entry point
// -----------------------------------------------------------------------
std::optional<std::string>
ConditionMapper::map(const std::vector<Argument> &condArgs,
                     const std::string &filepath, uint32_t line) const {
  if (condArgs.empty())
    return std::nullopt;

  // Expand all args to strings
  Expander exp(const_cast<ScopeStack &>(scope_), reporter_);
  CondTokens toks;
  for (const auto &arg : condArgs) {
    std::string val = exp.expandArg(arg);
    toks.push_back({val, toUpper(val)});
  }

  size_t i = 0;
  auto result = parseExpr(toks, i);
  if (!result) {
    reporter_.warning(
        filepath, line, 0,
        "could not map cmake condition to mokai condition string");
  }
  return result;
}

// -----------------------------------------------------------------------
// Recursive descent over condition tokens
// Precedence: OR < AND < NOT < ATOM
// -----------------------------------------------------------------------
std::optional<std::string> ConditionMapper::parseExpr(const CondTokens &toks,
                                                      size_t &i) const {
  return parseOr(toks, i);
}

std::optional<std::string> ConditionMapper::parseOr(const CondTokens &toks,
                                                    size_t &i) const {
  auto lhs = parseAnd(toks, i);

  while (i < toks.size() && toks[i].upper == "OR") {
    ++i;
    auto rhs = parseAnd(toks, i);
    if (!lhs || !rhs)
      return std::nullopt;
    lhs = "(" + *lhs + " || " + *rhs + ")";
  }
  return lhs;
}

std::optional<std::string> ConditionMapper::parseAnd(const CondTokens &toks,
                                                     size_t &i) const {
  auto lhs = parseNot(toks, i);

  while (i < toks.size() && toks[i].upper == "AND") {
    ++i;
    auto rhs = parseNot(toks, i);
    if (!lhs || !rhs)
      return std::nullopt;
    lhs = *lhs + " && " + *rhs;
  }
  return lhs;
}

std::optional<std::string> ConditionMapper::parseNot(const CondTokens &toks,
                                                     size_t &i) const {
  if (i < toks.size() && toks[i].upper == "NOT") {
    ++i;
    auto inner = parseNot(toks, i); // handle NOT NOT
    if (!inner)
      return std::nullopt;
    // Try to invert simple == to !=
    auto &s = *inner;
    if (s.find(" == ") != std::string::npos) {
      std::string inv = s;
      auto pos = inv.find(" == ");
      inv.replace(pos, 4, " != ");
      return inv;
    }
    if (s.find(" != ") != std::string::npos) {
      std::string inv = s;
      auto pos = inv.find(" != ");
      inv.replace(pos, 4, " == ");
      return inv;
    }
    return "!(" + s + ")";
  }
  return parseAtom(toks, i);
}

std::optional<std::string> ConditionMapper::parseAtom(const CondTokens &toks,
                                                      size_t &i) const {
  if (i >= toks.size())
    return std::nullopt;

  // Parenthesised sub-expression  (CMake doesn't have parens in conditions,
  // but we handle the edge case defensively)
  if (toks[i].text == "(") {
    ++i;
    auto inner = parseExpr(toks, i);
    if (i < toks.size() && toks[i].text == ")")
      ++i;
    return inner;
  }

  const std::string &lhs = toks[i].text;
  const std::string &lhsUpper = toks[i].upper;
  ++i;

  // Check for DEFINED <var> prefix operator
  if (i < toks.size() && toks[i].upper == "DEFINED") {
    ++i;
    std::string varName = (i < toks.size()) ? toks[i].text : "";
    if (i < toks.size())
      ++i;
    if (varName.empty())
      return std::nullopt;
    return "options." + varName + " == true";
  }

  // Check for binary comparison operators
  static const std::vector<std::string> binOps = {"STREQUAL",
                                                  "STRLESS",
                                                  "STRGREATER",
                                                  "STRLESS_EQUAL",
                                                  "STRGREATER_EQUAL",
                                                  "EQUAL",
                                                  "LESS",
                                                  "GREATER",
                                                  "LESS_EQUAL",
                                                  "GREATER_EQUAL",
                                                  "MATCHES",
                                                  "IN_LIST",
                                                  "VERSION_EQUAL",
                                                  "VERSION_LESS",
                                                  "VERSION_GREATER",
                                                  "VERSION_LESS_EQUAL",
                                                  "VERSION_GREATER_EQUAL"};

  if (i < toks.size()) {
    const std::string &opUpper = toks[i].upper;
    for (const auto &op : binOps) {
      if (opUpper == op) {
        ++i;
        std::string rhs = (i < toks.size()) ? toks[i].text : "";
        if (i < toks.size())
          ++i;
        return mapComparison(lhs, lhsUpper, op, rhs);
      }
    }
  }

  // IS_DIRECTORY, EXISTS, IS_ABSOLUTE — filesystem checks, can't map
  if (lhsUpper == "EXISTS" || lhsUpper == "IS_DIRECTORY" ||
      lhsUpper == "IS_ABSOLUTE" || lhsUpper == "IS_SYMLINK") {
    return std::nullopt;
  }

  // Single identifier: map known CMake variables/keywords
  return mapSingleVar(lhs, lhsUpper);
}

// -----------------------------------------------------------------------
// mapSingleVar — CMake predefined condition identifiers
// -----------------------------------------------------------------------
std::optional<std::string>
ConditionMapper::mapSingleVar(const std::string &name,
                              const std::string &upper) const {
  // --- OS predicates ---
  if (upper == "WIN32")
    return "os == windows";
  if (upper == "UNIX")
    return "(os == linux || os == macos)";
  if (upper == "APPLE")
    return "os == macos";
  if (upper == "ANDROID")
    return "os == android";
  if (upper == "IOS")
    return "os == ios";

  // --- Compiler predicates ---
  if (upper == "MSVC" || upper == "MSVC_IDE")
    return "compiler == msvc";
  if (upper == "CMAKE_COMPILER_IS_GNUCXX" || upper == "CMAKE_COMPILER_IS_GNUCC")
    return "compiler == gcc";

  // --- Arch predicates ---
  if (upper == "CMAKE_SIZEOF_VOID_P")
    return std::nullopt; // can't map simply

  // --- Build type ---
  if (upper == "CMAKE_BUILD_TYPE")
    return std::nullopt; // needs rhs

  // --- Boolean literals ---
  if (upper == "TRUE" || upper == "ON" || upper == "YES" || upper == "1")
    return std::nullopt; // always true — usually redundant
  if (upper == "FALSE" || upper == "OFF" || upper == "NO" || upper == "0")
    return std::nullopt; // always false

  // --- Check if it's an option() variable ---
  auto entry = scope_.lookup(name);
  if (entry && entry->isBool) {
    return "options." + name + " == true";
  }

  // --- CMAKE_* variables that map to known mokai fields ---
  if (upper.substr(0, 7) == "CMAKE_C") {
    // CMAKE_CXX_COMPILER_ID, CMAKE_C_COMPILER_ID checked via lookup
    return std::nullopt;
  }

  // Unknown — can't map
  return std::nullopt;
}

// -----------------------------------------------------------------------
// mapComparison — LHS OP RHS
// -----------------------------------------------------------------------
std::optional<std::string> ConditionMapper::mapComparison(
    const std::string &lhs, const std::string &lhsUpper, const std::string &op,
    const std::string &rhs) const {
  std::string rhsLower = toLower(rhs);
  std::string rhsUpper = toUpper(rhs);

  bool isEq = (op == "STREQUAL" || op == "EQUAL");
  // CMAKE_SYSTEM_NAME / CMAKE_HOST_SYSTEM_NAME → os
  if (lhsUpper == "CMAKE_SYSTEM_NAME" || lhsUpper == "CMAKE_HOST_SYSTEM_NAME") {
    static const std::unordered_map<std::string, std::string> osMap = {
        {"windows", "windows"}, {"linux", "linux"}, {"darwin", "macos"},
        {"android", "android"}, {"ios", "ios"},     {"freebsd", "linux"},
    };
    auto it = osMap.find(rhsLower);
    if (it != osMap.end())
      return "os " + std::string(isEq ? "==" : "!=") + " " + it->second;
    return std::nullopt;
  }

  // CMAKE_CXX_COMPILER_ID / CMAKE_C_COMPILER_ID → compiler
  if (lhsUpper == "CMAKE_CXX_COMPILER_ID" ||
      lhsUpper == "CMAKE_C_COMPILER_ID") {
    static const std::unordered_map<std::string, std::string> compMap = {
        {"clang", "clang"}, {"appleclang", "clang"}, {"gnu", "gcc"},
        {"gcc", "gcc"},     {"msvc", "msvc"},        {"intel", "msvc"},
    };
    auto it = compMap.find(rhsLower);
    if (it != compMap.end())
      return "compiler " + std::string(isEq ? "==" : "!=") + " " + it->second;
    return std::nullopt;
  }

  // CMAKE_BUILD_TYPE → config
  if (lhsUpper == "CMAKE_BUILD_TYPE") {
    return "config " + std::string(isEq ? "==" : "!=") + " " + rhsLower;
  }

  // CMAKE_CXX_STANDARD → cpp_version
  if (lhsUpper == "CMAKE_CXX_STANDARD") {
    return "cpp_version " + std::string(isEq ? "==" : "!=") + " c++" + rhs;
  }

  // CMAKE_SIZEOF_VOID_P → arch (8 = x86_64, 4 = x86)
  if (lhsUpper == "CMAKE_SIZEOF_VOID_P" && op == "EQUAL") {
    if (rhs == "8")
      return "arch == x86_64";
    if (rhs == "4")
      return "arch == x86";
    return std::nullopt;
  }

  // MSVC_VERSION comparisons → compiler_version
  if (lhsUpper == "MSVC_VERSION") {
    std::string mokaiOp;
    if (op == "GREATER" || op == "STRGREATER")
      mokaiOp = ">";
    else if (op == "LESS" || op == "STRLESS")
      mokaiOp = "<";
    else if (op == "GREATER_EQUAL")
      mokaiOp = ">=";
    else if (op == "LESS_EQUAL")
      mokaiOp = "<=";
    else if (op == "EQUAL")
      mokaiOp = "==";
    if (!mokaiOp.empty())
      return "compiler_version " + mokaiOp + " " + rhs;
  }

  // options.VAR == value  — for cache bool variables
  auto entry = scope_.lookup(lhs);
  if (entry && entry->isBool) {
    std::string boolVal = (rhsUpper == "TRUE" || rhsUpper == "ON" ||
                           rhsUpper == "YES" || rhsUpper == "1")
                              ? "true"
                              : "false";
    return "options." + lhs + " " + std::string(isEq ? "==" : "!=") + " " +
           boolVal;
  }

  return std::nullopt;
}

} // namespace transpiler::analyzer
