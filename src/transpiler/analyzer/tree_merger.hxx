#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>

#include "ast/node.hxx"
#include "diagnostics/diagnostic.hxx"
#include "scope.hxx"

using namespace transpiler::ast;
using namespace transpiler::diagnostics;

namespace transpiler::analyzer {

// -----------------------------------------------------------------------
// FileLoader — callable that reads a file and returns its content.
// Injected into TreeMerger so tests can provide in-memory files
// without touching the real filesystem.
// -----------------------------------------------------------------------
using FileLoader = std::function<std::string(const std::string &absolutePath)>;

// -----------------------------------------------------------------------
// TreeMerger — follows include() and add_subdirectory() calls,
// parses the referenced files, and inlines them into the AST.
//
// Strategy:
//   - Walk the FileNode's children looking for include() and
//     add_subdirectory() commands.
//   - When found, resolve the path relative to the current file's
//     directory using base_dir rules.
//   - Parse the referenced file (reusing the same DiagnosticReporter).
//   - Inline the referenced FileNode's children into the current tree.
//   - Track visited paths to prevent infinite include loops.
//
// The result is a single "flat" FileNode whose children represent
// the fully merged cmake tree — what the Analyzer sees.
// -----------------------------------------------------------------------
class TreeMerger {
public:
  TreeMerger(DiagnosticReporter &reporter, ScopeStack &scope,
             FileLoader loader);

  // Process a FileNode in-place: wherever include()/add_subdirectory()
  // appears, replace it with the inlined children of the referenced file.
  // Returns the same node with mutations applied.
  void merge(FileNode &root, const std::string &baseDir);

private:
  // Recursively merge a NodeList: replace inclusion commands with
  // the children of the loaded file.
  NodeList mergeNodeList(NodeList &nodes, const std::string &baseDir);

  // Try to inline an include() or add_subdirectory() command.
  // Returns the loaded children if successful, nullopt otherwise.
  std::optional<NodeList> tryInline(const CommandNode &cmd,
                                    const std::string &baseDir);

  // Load, lex, and parse a cmake file. Returns nullptr on failure.
  std::unique_ptr<FileNode> loadFile(const std::string &absPath);

  // Resolve a path from a cmake command argument relative to baseDir.
  // Expands variables using the current scope before resolving.
  std::string resolvePath(const std::string &rawPath,
                          const std::string &baseDir) const;

  DiagnosticReporter &reporter_;
  ScopeStack &scope_;
  FileLoader loader_;

  // Track absolute paths we've already merged to prevent cycles.
  std::unordered_set<std::string> visited_;
};

} // namespace transpiler::analyzer
