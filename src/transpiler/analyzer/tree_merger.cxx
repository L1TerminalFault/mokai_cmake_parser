#include <cctype>
#include <filesystem>

#include "expander.hxx"
#include "lexer/lexer.hxx"
#include "parser/parser.hxx"
#include "tree_merger.hxx"

using namespace transpiler::lexer;
using namespace transpiler::parser;
namespace fs = std::filesystem;

namespace transpiler::analyzer {

TreeMerger::TreeMerger(DiagnosticReporter &reporter, ScopeStack &scope,
                       FileLoader loader)
    : reporter_(reporter), scope_(scope), loader_(std::move(loader)) {}

// -----------------------------------------------------------------------
// merge() — entry point: mutates root in-place
// -----------------------------------------------------------------------
void TreeMerger::merge(FileNode &root, const std::string &baseDir) {
  // Mark the root file as visited
  try {
    if (!baseDir.empty() && fs::exists(baseDir)) {
      visited_.insert(fs::canonical(baseDir).string());
    } else {
      visited_.insert(baseDir);
    }
  } catch (...) {
    visited_.insert(baseDir);
  }

  root.children = mergeNodeList(root.children, baseDir);
}

// -----------------------------------------------------------------------
// mergeNodeList — rebuild a NodeList, inlining include/add_subdirectory
// -----------------------------------------------------------------------
NodeList TreeMerger::mergeNodeList(NodeList &nodes,
                                   const std::string &baseDir) {
  NodeList result;
  result.reserve(nodes.size());

  for (auto &nodePtr : nodes) {
    ASTNode *raw = nodePtr.get();

    // --- CommandNode: check for inclusion commands ---
    if (auto *cmd = dynamic_cast<CommandNode *>(raw)) {
      const std::string &name = cmd->name;

      bool isInclude = (name == "include");
      bool isAddSubdir = (name == "add_subdirectory");

      if (isInclude || isAddSubdir) {
        auto inlined = tryInline(*cmd, baseDir);
        if (inlined) {
          // Inline the children directly into our result
          for (auto &child : *inlined)
            result.push_back(std::move(child));
          continue; // don't keep the original include() command
        }
        // Could not inline — keep the original command as a fallback
        // The emitter will emit a warning about the unresolved include
      }
    }

    // --- Recurse into block nodes so nested includes are also merged ---
    if (auto *ifn = dynamic_cast<IfBlockNode *>(raw)) {
      for (auto &branch : ifn->branches)
        branch.body = mergeNodeList(branch.body, baseDir);
    } else if (auto *fe = dynamic_cast<ForeachNode *>(raw)) {
      fe->body = mergeNodeList(fe->body, baseDir);
    } else if (auto *wh = dynamic_cast<WhileNode *>(raw)) {
      wh->body = mergeNodeList(wh->body, baseDir);
    } else if (auto *fn = dynamic_cast<FunctionDefNode *>(raw)) {
      fn->body = mergeNodeList(fn->body, baseDir);
    } else if (auto *mac = dynamic_cast<MacroDefNode *>(raw)) {
      mac->body = mergeNodeList(mac->body, baseDir);
    }

    result.push_back(std::move(nodePtr));
  }
  return result;
}

// -----------------------------------------------------------------------
// tryInline — resolve path and load the file
// -----------------------------------------------------------------------
std::optional<NodeList> TreeMerger::tryInline(const CommandNode &cmd,
                                              const std::string &baseDir) {
  if (cmd.args.empty())
    return std::nullopt;

  // Expand the first argument to get the path
  Expander exp(scope_, reporter_);
  std::string rawPath = exp.expandArg(cmd.args[0]);
  if (rawPath.empty())
    return std::nullopt;

  // For add_subdirectory: the argument is a directory, not a file.
  // We look for CMakeLists.txt inside it.
  bool isSubdir = (cmd.name == "add_subdirectory");

  std::string resolvedPath = resolvePath(rawPath, baseDir);
  if (resolvedPath.empty())
    return std::nullopt;

  if (isSubdir) {
    // resolvedPath is a directory — append CMakeLists.txt
    resolvedPath = (fs::path(resolvedPath) / "CMakeLists.txt").string();
  }

  // Normalise to absolute path for cycle detection
  std::string absPath;
  try {
    absPath = fs::weakly_canonical(resolvedPath).string();
  } catch (...) {
    absPath = resolvedPath;
  }

  // Cycle detection
  if (visited_.count(absPath)) {
    reporter_.warning(cmd.loc.file, cmd.loc.line, cmd.loc.col,
                      "skipping already-included file: " + absPath);
    return std::nullopt;
  }
  visited_.insert(absPath);

  // Load the file
  auto fileNode = loadFile(absPath);
  if (!fileNode)
    return std::nullopt;

  // Recursively merge the loaded file's children
  std::string newBaseDir = fs::path(absPath).parent_path().string();
  fileNode->children = mergeNodeList(fileNode->children, newBaseDir);

  // Update CMAKE_CURRENT_SOURCE_DIR for the sub-tree
  // (We restore it after — this is a simplified scope model)
  auto prevDir = scope_.lookup("CMAKE_CURRENT_SOURCE_DIR");
  scope_.set("CMAKE_CURRENT_SOURCE_DIR",
             VarEntry{newBaseDir, VarOrigin::Builtin, "", false});

  NodeList result = std::move(fileNode->children);

  // Restore
  if (prevDir)
    scope_.set("CMAKE_CURRENT_SOURCE_DIR", *prevDir);
  else
    scope_.unset("CMAKE_CURRENT_SOURCE_DIR");

  return result;
}

// -----------------------------------------------------------------------
// loadFile — read, lex, parse
// -----------------------------------------------------------------------
std::unique_ptr<FileNode> TreeMerger::loadFile(const std::string &absPath) {
  std::string source;
  try {
    source = loader_(absPath);
  } catch (const std::exception &e) {
    reporter_.warning("", 0, 0,
                      "could not load file '" + absPath + "': " + e.what());
    return nullptr;
  }

  Lexer lx(source, absPath, reporter_);
  const auto &toks = lx.tokenize();

  if (reporter_.hasFatals())
    return nullptr;

  Parser p(toks, absPath, reporter_);
  return p.parse();
}

// -----------------------------------------------------------------------
// resolvePath — expand vars and resolve relative to baseDir
// -----------------------------------------------------------------------
std::string TreeMerger::resolvePath(const std::string &rawPath,
                                    const std::string &baseDir) const {
  // If empty, return empty
  if (rawPath.empty())
    return "";

  // If it's already absolute, use it directly
  try {
    if (fs::path(rawPath).is_absolute())
      return rawPath;
  } catch (...) {
    // continue with relative resolution
  }

  // Relative — join with baseDir
  try {
    auto joined = fs::path(baseDir) / rawPath;
    if (joined.is_absolute()) {
      return fs::weakly_canonical(joined).string();
    }
    return joined.string();
  } catch (...) {
    try {
      return (fs::path(baseDir) / rawPath).string();
    } catch (...) {
      return "";
    }
  }
}

} // namespace transpiler::analyzer
