#pragma once

#include <cstdint>
// #include <ratio>
#include <string>
#include <vector>

#include "diagnostics/diagnostic.hxx"
#include "token.hxx"

using namespace transpiler::diagnostics;

namespace transpiler::lexer {

// -----------------------------------------------------------------------
// Lexer — converts a CMake source file into a flat token stream.
//
// Design notes:
//   - One Lexer instance per file. The tree_merger instantiates a new one
//     for every include()/add_subdirectory() it follows.
//   - All tokens are materialized upfront into `tokens_`. This makes the
//     parser trivially backtrack-able and the test suite easy to write.
//   - Diagnostics are pushed into the shared DiagnosticReporter rather than
//     thrown, so the lexer keeps going after a bad character and collects
//     all errors in one pass.
// -----------------------------------------------------------------------
class Lexer {
public:
  // `source`   — full file contents (caller owns the string)
  // `filepath` — used only for SourceLocation; does not open any file
  // `reporter` — shared diagnostic sink; must outlive the Lexer
  // source and filepath are owned by value — no dangling reference risk
  Lexer(std::string source, std::string filepath, DiagnosticReporter &reporter);

  // Run the full lex pass and return all tokens including EndOfFile.
  // Safe to call once; subsequent calls return the cached result.
  const std::vector<Token> &tokenize();

private:
  // ---- source navigation ----
  char peek(int offset = 0) const;
  char advance();
  bool isAtEnd() const;
  void
  skipWhitespaceAndContinuations(); // handles line continuations: \<newline>
  SourceLocation here() const;

  // ---- scanners (each leaves `pos_` past the last consumed char) ----
  Token scanIdentifierOrKeyword();
  Token scanQuotedString();
  Token scanBracketString(); // [[ ... ]] and #[[ ... ]]
  Token scanVarRef();        // ${ }, $ENV{ }, $CACHE{ }, $< >
  Token scanComment();
  Token scanNumber();

  // ---- helpers ----
  // Resolve escape sequences inside a quoted string body.
  std::string resolveEscapes(const std::string &raw, const SourceLocation &loc);
  // Look up whether an identifier spelling is a CMake keyword.
  static TokenKind classifyKeyword(const std::string &word);

  // ---- state ----
  std::string source_;
  std::string filepath_;
  DiagnosticReporter &reporter_;

  size_t pos_;    // current byte position in source_
  uint32_t line_; // current 1-based line number
  uint32_t col_;  // current 1-based column (byte offset)

  std::vector<Token> tokens_;
  bool done_; // true once tokenize() has run
};

} // namespace transpiler::lexer
