#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lexer/token.hxx"

using namespace transpiler::lexer;

namespace transpiler::ast {

// Forward declarations for visitor
struct FileNode;
struct CommandNode;
struct IfBlockNode;
struct ForeachNode;
struct WhileNode;
struct FunctionDefNode;
struct MacroDefNode;

// -----------------------------------------------------------------------
// IVisitor — implement this to add a new pass without touching nodes.
// -----------------------------------------------------------------------
struct IVisitor {
  virtual ~IVisitor() = default;
  virtual void visit(FileNode &) = 0;
  virtual void visit(CommandNode &) = 0;
  virtual void visit(IfBlockNode &) = 0;
  virtual void visit(ForeachNode &) = 0;
  virtual void visit(WhileNode &) = 0;
  virtual void visit(FunctionDefNode &) = 0;
  virtual void visit(MacroDefNode &) = 0;
};

// -----------------------------------------------------------------------
// ArgPart / Argument
//
// CMake arguments are concatenated composites, e.g.:
//   "${CMAKE_SOURCE_DIR}/src"  -> [VarRef("CMAKE_SOURCE_DIR"), Literal("/src")]
//   "lib$<IF:$<bool>,a,b>"     -> [Literal("lib"), GenExpr("IF:$<bool>,a,b")]
//   "plain_word"               -> [Literal("plain_word")]
// -----------------------------------------------------------------------
struct ArgPart {
  enum class Kind { Literal, VarRef, EnvVarRef, CacheVarRef, GenExpr };
  Kind kind;
  std::string value; // literal text, var name, or gen-expr body
};

struct Argument {
  std::vector<ArgPart> parts;
  SourceLocation loc;
  bool isQuoted = false;

  bool isPlainLiteral() const {
    return parts.size() == 1 && parts[0].kind == ArgPart::Kind::Literal;
  }
  const std::string &literal() const { return parts[0].value; }
};

// -----------------------------------------------------------------------
// Base node
// -----------------------------------------------------------------------
struct ASTNode {
  SourceLocation loc;
  virtual ~ASTNode() = default;
  virtual void accept(IVisitor &v) = 0;
};

using NodePtr = std::unique_ptr<ASTNode>;
using NodeList = std::vector<NodePtr>;

// -----------------------------------------------------------------------
// CommandNode — a single CMake command call.
//   add_executable(my_target src/main.cpp)
// Covers the vast majority of CMake. The analyzer pattern-matches on name.
// -----------------------------------------------------------------------
struct CommandNode : ASTNode {
  std::string name; // lower-cased
  std::vector<Argument> args;
  void accept(IVisitor &v) override { v.visit(*this); }
};

// -----------------------------------------------------------------------
// IfBlockNode — if/elseif/else/endif collapsed into one tree node.
// -----------------------------------------------------------------------
struct IfBranch {
  std::vector<Argument> condition; // empty = else branch
  NodeList body;
  SourceLocation loc;
};

struct IfBlockNode : ASTNode {
  std::vector<IfBranch> branches;
  void accept(IVisitor &v) override { v.visit(*this); }
};

// -----------------------------------------------------------------------
// ForeachNode — foreach/endforeach
// -----------------------------------------------------------------------
struct ForeachNode : ASTNode {
  std::string loopVar;
  std::vector<Argument> items;
  std::string mode; // "LISTS", "ITEMS", "RANGE", "ZIP_LISTS", ""
  NodeList body;
  void accept(IVisitor &v) override { v.visit(*this); }
};

// -----------------------------------------------------------------------
// WhileNode — while/endwhile
// -----------------------------------------------------------------------
struct WhileNode : ASTNode {
  std::vector<Argument> condition;
  NodeList body;
  void accept(IVisitor &v) override { v.visit(*this); }
};

// -----------------------------------------------------------------------
// FunctionDefNode / MacroDefNode — user-defined callables.
// Body stored as-is; expander substitutes on call.
// -----------------------------------------------------------------------
struct FunctionDefNode : ASTNode {
  std::string name;
  std::vector<std::string> params;
  NodeList body;
  void accept(IVisitor &v) override { v.visit(*this); }
};

struct MacroDefNode : ASTNode {
  std::string name;
  std::vector<std::string> params;
  NodeList body;
  void accept(IVisitor &v) override { v.visit(*this); }
};

// -----------------------------------------------------------------------
// FileNode — root of one parsed CMake file.
// tree_merger stitches multiple FileNodes together.
// -----------------------------------------------------------------------
struct FileNode : ASTNode {
  std::string filepath;
  NodeList children;
  void accept(IVisitor &v) override { v.visit(*this); }
};

} // namespace transpiler::ast
