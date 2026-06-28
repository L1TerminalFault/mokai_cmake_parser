#include <cctype>
#include <fstream>

#include "analyzer.hxx"

using namespace transpiler::analyzer;

namespace transpiler::analyzer {

// -----------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------
Analyzer::Analyzer(DiagnosticReporter &reporter, const std::string &sourceDir,
                   const std::string &binaryDir, FileLoader loader)
    : reporter_(reporter), sourceDir_(sourceDir), binaryDir_(binaryDir),
      scope_(), expander_(scope_, reporter), condMapper_(scope_, reporter) {
  scope_.seedBuiltins(sourceDir, binaryDir, "");

  // Default loader: read from real filesystem
  if (!loader) {
    loader = [](const std::string &path) -> std::string {
      std::ifstream f(path);
      if (!f)
        throw std::runtime_error("cannot open: " + path);
      return {std::istreambuf_iterator<char>(f),
              std::istreambuf_iterator<char>()};
    };
  }

  (void)loader; // stored in TreeMerger when we wire it up
}

// -----------------------------------------------------------------------
// analyze() — entry point
// -----------------------------------------------------------------------
AnalyzerResult Analyzer::analyze(std::unique_ptr<FileNode> root) {
  if (!root)
    return result_;

  // Tree merger runs first to inline include()/add_subdirectory()
  // For now we use a real-filesystem loader
  FileLoader loader = [](const std::string &path) -> std::string {
    std::ifstream f(path);
    if (!f)
      throw std::runtime_error("cannot open: " + path);
    return {std::istreambuf_iterator<char>(f),
            std::istreambuf_iterator<char>()};
  };

  TreeMerger merger(reporter_, scope_, std::move(loader));
  merger.merge(*root, sourceDir_);

  // Walk the merged AST
  root->accept(*this);

  return std::move(result_);
}

// -----------------------------------------------------------------------
// IVisitor implementation
// -----------------------------------------------------------------------
void Analyzer::visit(FileNode &node) { walkNodes(node.children); }

void Analyzer::visit(CommandNode &node) {
  const std::string &name = node.name;

  // Dispatch to specific handlers
  if (name == "project")
    handleProject(node);
  else if (name == "set")
    handleSet(node);
  else if (name == "option")
    handleOption(node);
  else if (name == "add_executable")
    handleAddExecutable(node);
  else if (name == "add_library")
    handleAddLibrary(node);
  else if (name == "target_sources")
    handleTargetSources(node);
  else if (name == "target_include_directories")
    handleTargetIncludeDirs(node);
  else if (name == "target_link_libraries")
    handleTargetLinkLibs(node);
  else if (name == "target_compile_options")
    handleTargetCompileOpts(node);
  else if (name == "target_compile_definitions")
    handleTargetCompileDefs(node);
  else if (name == "set_target_properties")
    handleSetTargetProps(node);
  else if (name == "find_package")
    handleFindPackage(node);
  else if (name == "include_directories")
    handleIncludeDirectories(node);
  else if (name == "add_compile_options")
    handleAddCompileOptions(node);
  else if (name == "cmake_minimum_required")
    handleCmakeMinReq(node);
  else if (name == "set_property")
    handleSetProperty(node);
  // else: unknown command — silently ignore (many cmake commands don't map to
  // mokai)
}

void Analyzer::visit(IfBlockNode &node) {
  for (auto &branch : node.branches) {
    // Map the condition
    std::optional<std::string> cond;
    if (!branch.condition.empty()) {
      cond =
          condMapper_.map(branch.condition, branch.loc.file, branch.loc.line);
    }

    // Save and set current condition context
    std::string savedCond = currentCondition_;
    if (cond) {
      if (currentCondition_.empty())
        currentCondition_ = *cond;
      else
        currentCondition_ = "(" + currentCondition_ + ") && (" + *cond + ")";
    } else if (!branch.condition.empty()) {
      // Unknown condition — expand args to a comment string
      currentCondition_ = "/* unmapped cmake condition */";
    }
    // else: else-branch — condition is empty, keep outer condition

    walkNodes(branch.body);

    // Restore
    currentCondition_ = savedCond;
  }
}

void Analyzer::visit(ForeachNode &node) {
  // Expand the item list
  std::vector<std::string> items;

  if (node.mode == "RANGE") {
    // foreach(i RANGE [start] stop [step])
    auto expanded = expander_.expandArgs(node.items);
    int start = 0, stop = 0, step = 1;
    if (expanded.size() == 1) {
      stop = std::stoi(expanded[0]);
    } else if (expanded.size() == 2) {
      start = std::stoi(expanded[0]);
      stop = std::stoi(expanded[1]);
    } else if (expanded.size() >= 3) {
      start = std::stoi(expanded[0]);
      stop = std::stoi(expanded[1]);
      step = std::stoi(expanded[2]);
    }
    for (int v = start; v <= stop; v += step)
      items.push_back(std::to_string(v));
  } else {
    // Plain list or IN LISTS/ITEMS
    for (const auto &arg : node.items) {
      std::string val = expander_.expandArg(arg);
      if (node.mode == "LISTS") {
        // val is a variable name — look up its value as a list
        auto entry = scope_.lookup(val);
        if (entry) {
          auto listItems = Expander::splitList(entry->value);
          for (auto &li : listItems)
            items.push_back(li);
        }
      } else {
        // ITEMS or plain — split in case it's already a list
        auto split = Expander::splitList(val);
        for (auto &s : split)
          items.push_back(s);
      }
    }
  }

  // Execute the body once per item
  for (const auto &item : items) {
    scope_.set(node.loopVar, VarEntry{item, VarOrigin::Normal, "", false});
    walkNodes(node.body);
  }
  // Unset loop variable after loop (CMake keeps it set to last value,
  // but for our static analysis it's cleaner to unset)
  scope_.unset(node.loopVar);
}

void Analyzer::visit(WhileNode &node) {
  // Static analysis: we don't actually loop. Walk the body once
  // to catch any target definitions, but ignore the condition.
  reporter_.warning(
      "", 0, 0,
      "while() loop found — body will be analysed once (static approximation)");
  walkNodes(node.body);
}

void Analyzer::visit(FunctionDefNode &node) {
  // Register the function definition for later expansion
  CallableDef def;
  def.kind = CallableDef::Kind::Function;
  def.name = node.name;
  def.params = node.params;
  for (auto &child : node.body)
    def.bodyRefs.push_back(child.get());
  expander_.registerCallable(node.name, std::move(def));
}

void Analyzer::visit(MacroDefNode &node) {
  CallableDef def;
  def.kind = CallableDef::Kind::Macro;
  def.name = node.name;
  def.params = node.params;
  for (auto &child : node.body)
    def.bodyRefs.push_back(child.get());
  expander_.registerCallable(node.name, std::move(def));
}

// -----------------------------------------------------------------------
// Command handlers
// -----------------------------------------------------------------------

void Analyzer::handleProject(const CommandNode &cmd) {
  if (cmd.args.empty())
    return;
  result_.project.name = expand(cmd.args[0]);
  scope_.set("PROJECT_NAME",
             {result_.project.name, VarOrigin::Builtin, "", false});
  scope_.set("CMAKE_PROJECT_NAME",
             {result_.project.name, VarOrigin::Builtin, "", false});

  // Scan for VERSION and LANGUAGES keywords
  for (size_t i = 1; i < cmd.args.size(); ++i) {
    std::string val = expand(cmd.args[i]);
    std::string up;
    for (char c : val)
      up += (char)std::toupper((unsigned char)c);
    if (up == "VERSION" && i + 1 < cmd.args.size()) {
      result_.project.version = expand(cmd.args[++i]);
    } else if (up == "LANGUAGES") {
      while (++i < cmd.args.size()) {
        std::string lang = expand(cmd.args[i]);
        std::string lu;
        for (char c : lang)
          lu += (char)std::toupper((unsigned char)c);
        if (lu == "VERSION" || lu == "DESCRIPTION" || lu == "HOMEPAGE_URL")
          break;
        result_.project.languages.push_back(lang);
      }
    }
  }
}

void Analyzer::handleSet(const CommandNode &cmd) {
  if (cmd.args.empty())
    return;
  std::string varName = expand(cmd.args[0]);
  if (varName.empty())
    return;

  // Detect CACHE type
  // set(VAR val [CACHE type docstring [FORCE]])
  bool isCache = false;
  bool isBool = false;
  std::string docstring;
  std::vector<std::string> values;

  for (size_t i = 1; i < cmd.args.size(); ++i) {
    std::string val = expand(cmd.args[i]);
    std::string up;
    for (char c : val)
      up += (char)std::toupper((unsigned char)c);

    if (up == "CACHE") {
      isCache = true;
      if (i + 1 < cmd.args.size()) {
        std::string type = expand(cmd.args[++i]);
        std::string tu;
        for (char c : type)
          tu += (char)std::toupper((unsigned char)c);
        isBool = (tu == "BOOL");
      }
      if (i + 1 < cmd.args.size())
        docstring = expand(cmd.args[++i]);
      // Skip FORCE if present
      if (i + 1 < cmd.args.size()) {
        std::string next = expand(cmd.args[i + 1]);
        std::string nu;
        for (char c : next)
          nu += (char)std::toupper((unsigned char)c);
        if (nu == "FORCE")
          ++i;
      }
      continue;
    }
    if (up == "PARENT_SCOPE") {
      // We'll handle this after collecting values
      std::string joined = Expander::joinList(values);
      scope_.setParent(varName, {joined, VarOrigin::ParentScope, "", false});
      return;
    }
    values.push_back(val);
  }

  std::string joined = Expander::joinList(values);
  VarEntry entry{joined, isCache ? VarOrigin::Cache : VarOrigin::Normal,
                 docstring, isBool};

  if (isCache) {
    scope_.setCache(varName, entry);
    // Map bool cache vars to mokai options
    if (isBool) {
      ResolvedOption opt;
      opt.name = varName;
      opt.defaultValue = (joined == "ON" || joined == "TRUE" ||
                          joined == "YES" || joined == "1");
      opt.docstring = docstring;
      result_.options.push_back(opt);
    }
  } else {
    scope_.set(varName, entry);
  }

  // Detect CMAKE_CXX_STANDARD
  if (varName == "CMAKE_CXX_STANDARD")
    result_.project.cppStandard = normalizeCppStd(joined);
}

void Analyzer::handleOption(const CommandNode &cmd) {
  // option(VAR "description" [initial_value])
  if (cmd.args.empty())
    return;
  std::string name = expand(cmd.args[0]);
  std::string doc = cmd.args.size() > 1 ? expand(cmd.args[1]) : "";
  std::string def = cmd.args.size() > 2 ? expand(cmd.args[2]) : "OFF";
  std::string dup;
  for (char c : def)
    dup += (char)std::toupper((unsigned char)c);

  bool defaultVal =
      (dup == "ON" || dup == "TRUE" || dup == "YES" || dup == "1");

  scope_.setCache(name, {def, VarOrigin::Cache, doc, true});

  // Add to options list
  ResolvedOption opt{name, defaultVal, doc};
  // Avoid duplicates
  for (auto &existing : result_.options)
    if (existing.name == name) {
      existing = opt;
      return;
    }
  result_.options.push_back(std::move(opt));
}

void Analyzer::handleAddExecutable(const CommandNode &cmd) {
  if (cmd.args.empty())
    return;
  std::string name = expand(cmd.args[0]);
  auto &target = getOrCreateTarget(name, cmd.loc);
  target.type = "executable";

  // Collect sources — skip keywords like WIN32, MACOSX_BUNDLE, IMPORTED, ALIAS
  static const std::vector<std::string> skipKws = {
      "WIN32", "MACOSX_BUNDLE", "IMPORTED", "ALIAS", "EXCLUDE_FROM_ALL"};
  for (size_t i = 1; i < cmd.args.size(); ++i) {
    std::string val = expand(cmd.args[i]);
    std::string up;
    for (char c : val)
      up += (char)std::toupper((unsigned char)c);
    bool skip = false;
    for (auto &kw : skipKws)
      if (up == kw) {
        skip = true;
        break;
      }
    if (!skip && !val.empty())
      target.sources.push_back(val);
  }
}

void Analyzer::handleAddLibrary(const CommandNode &cmd) {
  if (cmd.args.empty())
    return;
  std::string name = expand(cmd.args[0]);
  auto &target = getOrCreateTarget(name, cmd.loc);

  // Detect type: STATIC, SHARED, MODULE, INTERFACE, OBJECT, ALIAS, IMPORTED
  std::string type = "static_library"; // default
  std::vector<std::string> sources;

  static const std::vector<std::string> typeKws = {
      "STATIC", "SHARED", "MODULE",   "INTERFACE",
      "OBJECT", "ALIAS",  "IMPORTED", "EXCLUDE_FROM_ALL"};

  for (size_t i = 1; i < cmd.args.size(); ++i) {
    std::string val = expand(cmd.args[i]);
    std::string up;
    for (char c : val)
      up += (char)std::toupper((unsigned char)c);

    if (up == "STATIC") {
      type = "static_library";
      continue;
    }
    if (up == "SHARED") {
      type = "shared_library";
      continue;
    }
    if (up == "MODULE") {
      type = "shared_library";
      continue;
    }
    if (up == "INTERFACE") {
      type = "static_library";
      continue;
    } // approx
    bool isKw = false;
    for (auto &kw : typeKws)
      if (up == kw) {
        isKw = true;
        break;
      }
    if (!isKw && !val.empty())
      sources.push_back(val);
  }
  target.type = type;
  for (auto &s : sources)
    target.sources.push_back(s);
}

void Analyzer::handleTargetSources(const CommandNode &cmd) {
  if (cmd.args.empty())
    return;
  std::string name = expand(cmd.args[0]);
  auto &target = getOrCreateTarget(name, cmd.loc);

  // Skip visibility keywords: PUBLIC, PRIVATE, INTERFACE
  for (size_t i = 1; i < cmd.args.size(); ++i) {
    std::string val = expand(cmd.args[i]);
    std::string up;
    for (char c : val)
      up += (char)std::toupper((unsigned char)c);
    if (up == "PUBLIC" || up == "PRIVATE" || up == "INTERFACE")
      continue;
    if (!val.empty()) {
      if (currentCondition_.empty())
        target.sources.push_back(val);
      else
        target.sourcesIf.push_back({currentCondition_, {val}});
    }
  }
}

void Analyzer::handleTargetIncludeDirs(const CommandNode &cmd) {
  if (cmd.args.empty())
    return;
  std::string name = expand(cmd.args[0]);
  auto &target = getOrCreateTarget(name, cmd.loc);

  for (size_t i = 1; i < cmd.args.size(); ++i) {
    std::string val = expand(cmd.args[i]);
    std::string up;
    for (char c : val)
      up += (char)std::toupper((unsigned char)c);
    if (up == "PUBLIC" || up == "PRIVATE" || up == "INTERFACE" ||
        up == "BEFORE" || up == "SYSTEM")
      continue;
    if (!val.empty())
      target.includeDirs.push_back(val);
  }
}

void Analyzer::handleTargetLinkLibs(const CommandNode &cmd) {
  if (cmd.args.empty())
    return;
  std::string name = expand(cmd.args[0]);
  auto &target = getOrCreateTarget(name, cmd.loc);

  static const std::vector<std::string> skip = {
      "PUBLIC", "PRIVATE", "INTERFACE", "GENERAL", "OPTIMIZED", "DEBUG"};

  for (size_t i = 1; i < cmd.args.size(); ++i) {
    std::string val = expand(cmd.args[i]);
    std::string up;
    for (char c : val)
      up += (char)std::toupper((unsigned char)c);
    bool isKw = false;
    for (auto &kw : skip)
      if (up == kw) {
        isKw = true;
        break;
      }
    if (isKw || val.empty())
      continue;

    if (currentCondition_.empty())
      target.dependsOn.push_back(val);
    else
      target.dependsIf.push_back({currentCondition_, {val}});
  }
}

void Analyzer::handleTargetCompileOpts(const CommandNode &cmd) {
  if (cmd.args.empty())
    return;
  std::string name = expand(cmd.args[0]);
  auto &target = getOrCreateTarget(name, cmd.loc);

  for (size_t i = 1; i < cmd.args.size(); ++i) {
    std::string val = expand(cmd.args[i]);
    std::string up;
    for (char c : val)
      up += (char)std::toupper((unsigned char)c);
    if (up == "PUBLIC" || up == "PRIVATE" || up == "INTERFACE")
      continue;
    if (!val.empty()) {
      if (currentCondition_.empty())
        target.flags.push_back(val);
      else
        target.flagsIf.push_back({currentCondition_, {val}});
    }
  }
}

void Analyzer::handleTargetCompileDefs(const CommandNode &cmd) {
  if (cmd.args.empty())
    return;
  std::string name = expand(cmd.args[0]);
  auto &target = getOrCreateTarget(name, cmd.loc);

  for (size_t i = 1; i < cmd.args.size(); ++i) {
    std::string val = expand(cmd.args[i]);
    std::string up;
    for (char c : val)
      up += (char)std::toupper((unsigned char)c);
    if (up == "PUBLIC" || up == "PRIVATE" || up == "INTERFACE")
      continue;
    if (!val.empty()) {
      if (currentCondition_.empty())
        target.defines.push_back(val);
      else
        target.definesIf.push_back({currentCondition_, {val}});
    }
  }
}

void Analyzer::handleSetTargetProps(const CommandNode &cmd) {
  // set_target_properties(target1 target2 ... PROPERTIES prop1 val1 ...)
  // We only care about CXX_STANDARD for now
  std::vector<std::string> targetNames;
  size_t i = 0;
  for (; i < cmd.args.size(); ++i) {
    std::string val = expand(cmd.args[i]);
    std::string up;
    for (char c : val)
      up += (char)std::toupper((unsigned char)c);
    if (up == "PROPERTIES") {
      ++i;
      break;
    }
    targetNames.push_back(val);
  }
  for (; i + 1 < cmd.args.size(); i += 2) {
    std::string prop = expand(cmd.args[i]);
    std::string val = expand(cmd.args[i + 1]);
    std::string up;
    for (char c : prop)
      up += (char)std::toupper((unsigned char)c);
    if (up == "CXX_STANDARD")
      result_.project.cppStandard = normalizeCppStd(val);
  }
}

void Analyzer::handleFindPackage(const CommandNode &cmd) {
  // find_package(PackageName ...) — record as a dependency
  if (cmd.args.empty())
    return;
  std::string pkgName = expand(cmd.args[0]);
  if (!pkgName.empty())
    result_.globalDependencies.push_back(pkgName);
}

void Analyzer::handleIncludeDirectories(const CommandNode &cmd) {
  for (size_t i = 0; i < cmd.args.size(); ++i) {
    std::string val = expand(cmd.args[i]);
    std::string up;
    for (char c : val)
      up += (char)std::toupper((unsigned char)c);
    if (up == "AFTER" || up == "BEFORE" || up == "SYSTEM")
      continue;
    if (!val.empty())
      result_.globalIncludeDirs.push_back(val);
  }
}

void Analyzer::handleAddCompileOptions(const CommandNode &cmd) {
  // Global compile options — apply to all targets (simplified)
  // In mokai we don't have a global flags field, so we store them as a note
  // and the emitter can decide what to do.
  // For now, skip.
  (void)cmd;
}

void Analyzer::handleCmakeMinReq(const CommandNode &cmd) {
  // cmake_minimum_required(VERSION x.y) — no mokai equivalent, skip
  (void)cmd;
}

void Analyzer::handleSetProperty(const CommandNode &cmd) {
  // set_property(TARGET tgt PROPERTY prop val) — handle CXX_STANDARD
  (void)cmd; // TODO: parse and handle relevant properties
}

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------
void Analyzer::walkNodes(NodeList &nodes) {
  for (auto &node : nodes)
    node->accept(*this);
}

ResolvedTarget &Analyzer::getOrCreateTarget(const std::string &name,
                                            const SourceLocation &loc) {
  for (auto &t : result_.targets)
    if (t.name == name)
      return t;
  ResolvedTarget t;
  t.name = name;
  t.loc = loc;
  result_.targets.push_back(std::move(t));
  return result_.targets.back();
}

std::string Analyzer::expand(const Argument &arg) const {
  return expander_.expandArg(arg);
}

std::vector<std::string>
Analyzer::expandList(const std::vector<Argument> &args) const {
  std::vector<std::string> out;
  for (const auto &a : args) {
    auto val = expander_.expandArg(a);
    auto items = Expander::splitList(val);
    for (auto &item : items)
      if (!item.empty())
        out.push_back(item);
  }
  return out;
}

std::string Analyzer::mapTargetType(const std::string &cmakeType) {
  std::string up;
  for (char c : cmakeType)
    up += (char)std::toupper((unsigned char)c);
  if (up == "STATIC")
    return "static_library";
  if (up == "SHARED")
    return "shared_library";
  if (up == "MODULE")
    return "shared_library";
  return "executable";
}

std::string Analyzer::normalizeCppStd(const std::string &val) {
  // Accept: "17", "20", "c++17", "c++20", etc.
  std::string out;
  for (char c : val)
    if (std::isdigit((unsigned char)c))
      out += c;
  if (out.empty())
    return "c++23";
  return "c++" + out;
}

} // namespace transpiler::analyzer
