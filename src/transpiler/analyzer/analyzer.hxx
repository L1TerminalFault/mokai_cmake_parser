#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ast/node.hxx"
#include "condition_mapper.hxx"
#include "diagnostics/diagnostic.hxx"
#include "expander.hxx"
#include "scope.hxx"
#include "tree_merger.hxx"

using namespace transpiler::diagnostics;
using namespace transpiler::ast;

namespace transpiler::analyzer {

// -----------------------------------------------------------------------
// ResolvedTarget — the analyzer's output for a single CMake target.
// This is an intermediate form between the raw AST and the mokai manifest.
// The emitter consumes these.
// -----------------------------------------------------------------------
struct ConditionalSources {
  std::string condition; // mokai condition string, empty = unconditional
  std::vector<std::string> patterns; // file patterns (may be globs)
};

struct ConditionalFlags {
  std::string condition;
  std::vector<std::string> flags;
};

struct ConditionalDefines {
  std::string condition;
  std::vector<std::string> defines;
};

struct ConditionalDepends {
  std::string condition;
  std::vector<std::string> deps;
};

struct ResolvedTarget {
  std::string name;
  std::string type; // "executable", "static_library", "shared_library"

  std::vector<std::string> sources;
  std::vector<std::string> includeDirs;
  std::vector<std::string> flags;
  std::vector<std::string> defines;
  std::vector<std::string> systemLibs;
  std::vector<std::string> dependsOn;
  std::vector<std::string> properties; // @property_group references

  std::vector<ConditionalSources> sourcesIf;
  std::vector<ConditionalFlags> flagsIf;
  std::vector<ConditionalDefines> definesIf;
  std::vector<ConditionalDepends> dependsIf;

  SourceLocation loc;
};

// -----------------------------------------------------------------------
// ResolvedOption — from option() or CACHE BOOL calls
// -----------------------------------------------------------------------
struct ResolvedOption {
  std::string name;
  bool defaultValue;
  std::string docstring;
};

// -----------------------------------------------------------------------
// ResolvedProject — top-level project() metadata
// -----------------------------------------------------------------------
struct ResolvedProject {
  std::string name;
  std::string version;
  std::string cppStandard; // "c++17", "c++20", etc.
  std::vector<std::string> languages;
};

// -----------------------------------------------------------------------
// AnalyzerResult — everything the emitter needs.
// -----------------------------------------------------------------------
struct AnalyzerResult {
  ResolvedProject project;
  std::vector<ResolvedOption> options;
  std::vector<ResolvedTarget> targets;
  std::vector<std::string> globalIncludeDirs;
  std::vector<std::string> globalDependencies;
};

// -----------------------------------------------------------------------
// Analyzer — walks the merged AST, executes cmake logic, and builds
// an AnalyzerResult.
//
// It implements IVisitor so it can dispatch on node type cleanly.
// -----------------------------------------------------------------------
class Analyzer : public IVisitor {
public:
  Analyzer(DiagnosticReporter &reporter, const std::string &sourceDir,
           const std::string &binaryDir, FileLoader loader = nullptr);

  // Run the full analysis pass. Takes ownership of the root.
  AnalyzerResult analyze(std::unique_ptr<FileNode> root);

private:
  // IVisitor implementation
  void visit(FileNode &) override;
  void visit(CommandNode &) override;
  void visit(IfBlockNode &) override;
  void visit(ForeachNode &) override;
  void visit(WhileNode &) override;
  void visit(FunctionDefNode &) override;
  void visit(MacroDefNode &) override;

  // Command handlers — one per significant CMake command
  void handleProject(const CommandNode &);
  void handleSet(const CommandNode &);
  void handleOption(const CommandNode &);
  void handleAddExecutable(const CommandNode &);
  void handleAddLibrary(const CommandNode &);
  void handleTargetSources(const CommandNode &);
  void handleTargetIncludeDirs(const CommandNode &);
  void handleTargetLinkLibs(const CommandNode &);
  void handleTargetCompileOpts(const CommandNode &);
  void handleTargetCompileDefs(const CommandNode &);
  void handleSetTargetProps(const CommandNode &);
  void handleFindPackage(const CommandNode &);
  void handleIncludeDirectories(const CommandNode &);
  void handleAddCompileOptions(const CommandNode &);
  void handleCmakeMinReq(const CommandNode &);
  void handleSetProperty(const CommandNode &);

  // Walk a NodeList (used by block nodes)
  void walkNodes(NodeList &nodes);

  // Find or create a target by name
  ResolvedTarget &getOrCreateTarget(const std::string &name,
                                    const SourceLocation &loc);

  // Expand an argument to string using current scope
  std::string expand(const Argument &arg) const;

  // Expand all args to string list (also splits CMake lists)
  std::vector<std::string> expandList(const std::vector<Argument> &args) const;

  // Map a CMake target type string to mokai type
  static std::string mapTargetType(const std::string &cmakeType);

  // Normalise a CMake C++ standard value to "c++XX"
  static std::string normalizeCppStd(const std::string &val);

  // State
  DiagnosticReporter &reporter_;
  std::string sourceDir_;
  std::string binaryDir_;

  ScopeStack scope_;
  Expander expander_;
  ConditionMapper condMapper_;

  AnalyzerResult result_;

  // Current condition context (set while walking inside an if-branch)
  std::string currentCondition_;
};

} // namespace transpiler::analyzer
