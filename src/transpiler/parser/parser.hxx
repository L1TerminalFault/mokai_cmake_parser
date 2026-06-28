#pragma once

#include <memory>
#include <vector>

#include "ast/node.hxx"
#include "diagnostics/diagnostic.hxx"
#include "lexer/token.hxx"

using namespace transpiler::ast;
using namespace transpiler::diagnostics;
using namespace transpiler::lexer;

namespace transpiler::parser {

// -----------------------------------------------------------------------
// Parser — converts a flat token stream into an AST.
//
// Design:
//   - Recursive descent, single-pass.
//   - Consumes tokens produced by Lexer::tokenize().
//   - Control-flow commands (if/foreach/while/function/macro) are
//     collapsed into tree nodes; everything else is a CommandNode.
//   - On parse errors we emit a diagnostic and attempt to synchronise
//     to the next newline so we keep parsing and collect all errors.
// -----------------------------------------------------------------------
class Parser {
public:
  Parser(const std::vector<Token> &tokens, const std::string &filepath,
         DiagnosticReporter &reporter);

  // Returns the root FileNode for this file. Never null.
  std::unique_ptr<FileNode> parse();

private:
  // ---- token navigation ----
  const Token &peek(int offset = 0) const;
  const Token &advance();
  bool isAtEnd() const;
  bool check(TokenKind k, int offset = 0) const;
  bool match(TokenKind k); // advance if kind matches
  void skipNewlines();     // eat any number of Newline tokens
  void
  expectNewlineOrEof(); // consume newline after command, emit diag if missing
  void synchronize();   // error recovery: skip to next newline

  // ---- top-level parsing ----
  NodePtr parseStatement();                // dispatches based on next token
  NodePtr parseCommand(const Token &name); // plain command invocation
  NodePtr parseIfBlock();
  NodePtr parseForeachBlock();
  NodePtr parseWhileBlock();
  NodePtr parseFunctionDef();
  NodePtr parseMacroDef();

  // Parse a body of statements until a terminator keyword is found.
  // `terminators` is a list of lower-cased command names that end the block
  // (e.g. {"endif"} or {"elseif","else","endif"}).
  // Does NOT consume the terminator token.
  NodeList parseBody(const std::vector<std::string> &terminators);

  // ---- argument parsing ----
  // Parse all arguments until RParen.
  std::vector<Argument> parseArgList();
  // Parse one argument (may be a composite of multiple tokens if unquoted).
  Argument parseArgument();
  // Build an Argument from a single already-consumed token.
  Argument tokenToArgument(const Token &tok);
  // Decompose a raw string (from unquoted or quoted token) into ArgParts,
  // handling embedded ${VAR} and $<genexpr> sequences.
  std::vector<ArgPart> decomposeParts(const std::string &text,
                                      ArgPart::Kind varKind,
                                      const SourceLocation &loc);

  // ---- helpers ----
  // True if the current token looks like it could start an argument
  bool isArgumentStart() const;
  // Lower-case a string (for keyword comparisons)
  static std::string toLower(const std::string &s);

  // ---- state ----
  const std::vector<Token> &tokens_;
  const std::string &filepath_;
  DiagnosticReporter &reporter_;
  size_t pos_;
};

} // namespace transpiler::parser
