#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace transpiler::lexer {

// -----------------------------------------------------------------------
// All token kinds the CMake lexer can produce.
// New syntax additions go here — the parser never sees raw characters.
// -----------------------------------------------------------------------
enum class TokenKind {
  // --- Literals ---
  Identifier,    // any unquoted word:  add_executable, MyTarget, ON, OFF
  String,        // "quoted string"  (quotes stripped, escapes resolved)
  BracketString, // [[ bracket string ]]
  Number,        // 42, 3.14  (rare in cmake but valid)

  // --- Variables ---
  VarRef,      // ${VAR}  — content is the inner name, e.g. "VAR"
  EnvVarRef,   // $ENV{VAR}
  CacheVarRef, // $CACHE{VAR}
  GenExpr, // $<...>  generator expression — stored as raw string, not parsed

  // --- Structure ---
  LParen,  // (
  RParen,  // )
  Newline, // \n  — significant in CMake (ends a command argument list)

  // --- Comments ---
  Comment,        // # ...   (single line)
  BracketComment, // #[[ ... ]]

  // --- Control (for the parser to recognize without strcmp) ---
  // These are emitted as Identifier but the parser promotes them.
  // Storing them here for the keyword table.
  KW_if,
  KW_elseif,
  KW_else,
  KW_endif,
  KW_foreach,
  KW_endforeach,
  KW_while,
  KW_endwhile,
  KW_function,
  KW_endfunction,
  KW_macro,
  KW_endmacro,
  KW_return,
  KW_break,
  KW_continue,

  // --- Meta ---
  EndOfFile,
  Unknown, // character the lexer could not classify — reported as diagnostic
};

// -----------------------------------------------------------------------
// Source location — stored in every token from the start.
// Never retrofitted; always present so diagnostics are always precise.
// -----------------------------------------------------------------------
struct SourceLocation {
  std::string file; // absolute path to the cmake file
  uint32_t line;    // 1-based
  uint32_t col;     // 1-based, byte offset on the line

  SourceLocation() : line(0), col(0) {}
  SourceLocation(std::string f, uint32_t l, uint32_t c)
      : file(std::move(f)), line(l), col(c) {}
};

// -----------------------------------------------------------------------
// A single lexed unit.
// `text` is always the resolved/cleaned content:
//   - String      → content without surrounding quotes, escapes resolved
//   - VarRef      → inner name only ("MY_VAR", not "${MY_VAR}")
//   - Identifier  → raw spelling
//   - Comment     → text after the # (trimmed)
// -----------------------------------------------------------------------
struct Token {
  TokenKind kind;
  std::string text;
  SourceLocation loc;

  Token() : kind(TokenKind::Unknown) {}
  Token(TokenKind k, std::string t, SourceLocation l)
      : kind(k), text(std::move(t)), loc(std::move(l)) {}

  // Convenience predicates
  bool is(TokenKind k) const { return kind == k; }
  bool isIdentifier() const { return kind == TokenKind::Identifier; }
  bool isString() const {
    return kind == TokenKind::String || kind == TokenKind::BracketString;
  }
  bool isEof() const { return kind == TokenKind::EndOfFile; }
  bool isKeyword() const {
    return kind >= TokenKind::KW_if && kind <= TokenKind::KW_continue;
  }

  // Human-readable kind name — useful for error messages
  static std::string_view kindName(TokenKind k);
};

} // namespace transpiler::lexer
