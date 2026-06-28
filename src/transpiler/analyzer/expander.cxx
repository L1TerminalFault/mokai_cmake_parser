#include <cctype>

#include "expander.hxx"

namespace transpiler::analyzer {

Expander::Expander(ScopeStack &scope, DiagnosticReporter &reporter)
    : scope_(scope), reporter_(reporter) {}

// -----------------------------------------------------------------------
// expandArg — resolve all parts of an Argument into one string
// -----------------------------------------------------------------------
std::string Expander::expandArg(const Argument &arg) const {
  std::string result;
  for (const auto &part : arg.parts) {
    switch (part.kind) {
    case ArgPart::Kind::Literal:
      result += part.value;
      break;
    case ArgPart::Kind::VarRef: {
      // The var name itself may contain refs: ${${OUTER}}
      std::string name = scope_.expand(part.value);
      auto entry = scope_.lookup(name);
      if (entry)
        result += entry->value;
      // else: undefined var → empty string (CMake behaviour)
      break;
    }
    case ArgPart::Kind::EnvVarRef: {
      auto val = scope_.lookupEnv(part.value);
      if (val)
        result += *val;
      break;
    }
    case ArgPart::Kind::CacheVarRef: {
      auto entry = scope_.lookupCache(part.value);
      if (entry)
        result += entry->value;
      break;
    }
    case ArgPart::Kind::GenExpr:
      // Generator expressions are runtime — keep as $<...>
      result += "$<" + part.value + ">";
      break;
    }
  }
  return result;
}

std::vector<std::string>
Expander::expandArgs(const std::vector<Argument> &args) const {
  std::vector<std::string> out;
  out.reserve(args.size());
  for (const auto &a : args)
    out.push_back(expandArg(a));
  return out;
}

// -----------------------------------------------------------------------
// splitList — "a;b;c" → ["a","b","c"]
// CMake semicolons inside quoted strings are already stripped by the lexer;
// this operates on already-expanded list strings.
// -----------------------------------------------------------------------
std::vector<std::string> Expander::splitList(const std::string &listStr) {
  std::vector<std::string> items;
  std::string current;

  for (size_t i = 0; i < listStr.size(); ++i) {
    if (listStr[i] == '\\' && i + 1 < listStr.size() && listStr[i + 1] == ';') {
      current += ';';
      ++i; // skip escaped semicolon
    } else if (listStr[i] == ';') {
      items.push_back(std::move(current));
      current.clear();
    } else {
      current += listStr[i];
    }
  }
  if (!current.empty() || !listStr.empty())
    items.push_back(std::move(current));

  // Remove empty strings that result from splitting "" (empty list)
  if (items.size() == 1 && items[0].empty())
    items.clear();

  return items;
}

std::string Expander::joinList(const std::vector<std::string> &items) {
  std::string result;
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0)
      result += ';';
    result += items[i];
  }
  return result;
}

// -----------------------------------------------------------------------
// Callable registry
// -----------------------------------------------------------------------
void Expander::registerCallable(const std::string &name, CallableDef def) {
  std::string lower;
  for (char c : name)
    lower += (char)std::tolower((unsigned char)c);
  callables_[lower] = std::move(def);
}

bool Expander::isCallable(const std::string &name) const {
  std::string lower;
  for (char c : name)
    lower += (char)std::tolower((unsigned char)c);
  return callables_.count(lower) > 0;
}

const CallableDef *Expander::getCallable(const std::string &name) const {
  std::string lower;
  for (char c : name)
    lower += (char)std::tolower((unsigned char)c);
  auto it = callables_.find(lower);
  if (it != callables_.end())
    return &it->second;
  return nullptr;
}

// -----------------------------------------------------------------------
// evalBool — CMake boolean evaluation rules
//
// CMake truthy:  "1", "ON", "YES", "TRUE", "Y", non-zero numbers,
//                non-empty strings that aren't in the falsy list
// CMake falsy:   "0", "OFF", "NO", "FALSE", "N", "IGNORE", "NOTFOUND",
//                "", strings ending in "-NOTFOUND"
// -----------------------------------------------------------------------
Expander::TriBool Expander::evalBool(const std::string &value) const {
  std::string upper;
  for (char c : value)
    upper += (char)std::toupper((unsigned char)c);

  // Explicit false values
  static const std::vector<std::string> falsy = {
      "0", "OFF", "NO", "FALSE", "N", "IGNORE", "NOTFOUND", ""};
  for (const auto &f : falsy)
    if (upper == f)
      return TriBool::False;

  // Ends with -NOTFOUND
  if (upper.size() >= 9 && upper.substr(upper.size() - 9) == "-NOTFOUND")
    return TriBool::False;

  // Explicit true values
  static const std::vector<std::string> truthy = {"1", "ON", "YES", "TRUE",
                                                  "Y"};
  for (const auto &t : truthy)
    if (upper == t)
      return TriBool::True;

  // Non-zero number
  bool isNum = !value.empty();
  for (char c : value)
    if (!std::isdigit((unsigned char)c)) {
      isNum = false;
      break;
    }
  if (isNum && value != "0")
    return TriBool::True;

  // Non-empty string that isn't a known false value — CMake treats as true
  if (!value.empty())
    return TriBool::True;

  return TriBool::Unknown;
}

} // namespace transpiler::analyzer
