#include <cassert>
#include <cctype>
#include <unordered_map>

#include "lexer.hxx"

namespace transpiler::lexer {

// -----------------------------------------------------------------------
// Token::kindName
// -----------------------------------------------------------------------
std::string_view Token::kindName(TokenKind k) {
  switch (k) {
  case TokenKind::Identifier:
    return "identifier";
  case TokenKind::String:
    return "string";
  case TokenKind::BracketString:
    return "bracket-string";
  case TokenKind::Number:
    return "number";
  case TokenKind::VarRef:
    return "variable-ref";
  case TokenKind::EnvVarRef:
    return "env-var-ref";
  case TokenKind::CacheVarRef:
    return "cache-var-ref";
  case TokenKind::GenExpr:
    return "generator-expression";
  case TokenKind::LParen:
    return "(";
  case TokenKind::RParen:
    return ")";
  case TokenKind::Newline:
    return "newline";
  case TokenKind::Comment:
    return "comment";
  case TokenKind::BracketComment:
    return "bracket-comment";
  case TokenKind::EndOfFile:
    return "<eof>";
  case TokenKind::Unknown:
    return "<unknown>";
  default:
    return "keyword";
  }
}

// -----------------------------------------------------------------------
// Keyword table
// -----------------------------------------------------------------------
static const std::unordered_map<std::string, TokenKind> kKeywords = {
    {"if", TokenKind::KW_if},
    {"elseif", TokenKind::KW_elseif},
    {"else", TokenKind::KW_else},
    {"endif", TokenKind::KW_endif},
    {"foreach", TokenKind::KW_foreach},
    {"endforeach", TokenKind::KW_endforeach},
    {"while", TokenKind::KW_while},
    {"endwhile", TokenKind::KW_endwhile},
    {"function", TokenKind::KW_function},
    {"endfunction", TokenKind::KW_endfunction},
    {"macro", TokenKind::KW_macro},
    {"endmacro", TokenKind::KW_endmacro},
    {"return", TokenKind::KW_return},
    {"break", TokenKind::KW_break},
    {"continue", TokenKind::KW_continue},
};

// -----------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------
Lexer::Lexer(std::string source, std::string filepath,
             DiagnosticReporter &reporter)
    : source_(source), filepath_(filepath), reporter_(reporter), pos_(0),
      line_(1), col_(1), done_(false) {}

// -----------------------------------------------------------------------
// Public: tokenize()
// -----------------------------------------------------------------------
const std::vector<Token> &Lexer::tokenize() {
  if (done_)
    return tokens_;
  done_ = true;

  while (!isAtEnd()) {
    skipWhitespaceAndContinuations();
    if (isAtEnd())
      break;

    char c = peek();

    // Newline is significant in CMake
    if (c == '\n') {
      tokens_.emplace_back(TokenKind::Newline, "\n", here());
      advance();
      continue;
    }

    // Comment or bracket comment
    if (c == '#') {
      tokens_.push_back(scanComment());
      continue;
    }

    // Parentheses — must be checked BEFORE the general identifier path
    if (c == '(') {
      tokens_.emplace_back(TokenKind::LParen, "(", here());
      advance();
      continue;
    }
    if (c == ')') {
      tokens_.emplace_back(TokenKind::RParen, ")", here());
      advance();
      continue;
    }

    // Quoted string
    if (c == '"') {
      tokens_.push_back(scanQuotedString());
      continue;
    }

    // Bracket string [[ or [=*[
    if (c == '[' && (peek(1) == '[' || peek(1) == '=')) {
      tokens_.push_back(scanBracketString());
      continue;
    }

    // Variable / generator expression
    if (c == '$') {
      tokens_.push_back(scanVarRef());
      continue;
    }

    // Any printable non-structural character starts an unquoted token
    // (identifiers, keywords, paths like src/main.cpp, flags like -Wall)
    if (std::isgraph(static_cast<unsigned char>(c)) && c != '(' && c != ')' &&
        c != '"' && c != '#' && c != '$') {
      tokens_.push_back(scanIdentifierOrKeyword());
      continue;
    }

    // Truly unknown character
    reporter_.error(filepath_, line_, col_,
                    std::string("unexpected character '") + c + "'");
    tokens_.emplace_back(TokenKind::Unknown, std::string(1, c), here());
    advance();
  }

  tokens_.emplace_back(TokenKind::EndOfFile, "", here());
  return tokens_;
}

// -----------------------------------------------------------------------
// Source navigation
// -----------------------------------------------------------------------
char Lexer::peek(int offset) const {
  size_t idx = pos_ + static_cast<size_t>(offset);
  if (idx >= source_.size())
    return '\0';
  return source_[idx];
}

char Lexer::advance() {
  char c = source_[pos_++];
  if (c == '\n') {
    ++line_;
    col_ = 1;
  } else {
    ++col_;
  }
  return c;
}

bool Lexer::isAtEnd() const { return pos_ >= source_.size(); }

SourceLocation Lexer::here() const {
  return SourceLocation(filepath_, line_, col_);
}

void Lexer::skipWhitespaceAndContinuations() {
  while (!isAtEnd()) {
    char c = peek();
    if (c == ' ' || c == '\t' || c == '\r') {
      advance();
    } else if (c == '\\' && peek(1) == '\n') {
      advance();
      advance();
    } else {
      break;
    }
  }
}

// -----------------------------------------------------------------------
// Scanners
// -----------------------------------------------------------------------

Token Lexer::scanIdentifierOrKeyword() {
  SourceLocation start = here();
  std::string text;

  // Unquoted argument: consume until whitespace or structural chars
  while (!isAtEnd()) {
    char c = peek();
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '(' ||
        c == ')' || c == '#' || c == '"' || c == '$')
      break;
    text += advance();
  }

  TokenKind kind = classifyKeyword(text);
  return Token(kind, std::move(text), start);
}

Token Lexer::scanQuotedString() {
  SourceLocation start = here();
  assert(peek() == '"');
  advance(); // consume opening "

  std::string raw;
  while (!isAtEnd() && peek() != '"') {
    if (peek() == '\\') {
      raw += advance();
      if (!isAtEnd())
        raw += advance();
    } else {
      raw += advance();
    }
  }

  if (isAtEnd()) {
    reporter_.fatal(filepath_, start.line, start.col,
                    "unterminated quoted string");
    return Token(TokenKind::String, raw, start);
  }
  advance(); // consume closing "
  return Token(TokenKind::String, resolveEscapes(raw, start), start);
}

Token Lexer::scanBracketString() {
  SourceLocation start = here();
  assert(peek() == '[');
  advance(); // consume first [

  int level = 0;
  while (!isAtEnd() && peek() == '=') {
    ++level;
    advance();
  }

  if (isAtEnd() || peek() != '[') {
    reporter_.error(filepath_, start.line, start.col,
                    "malformed bracket string opening");
    return Token(TokenKind::Unknown, "", start);
  }
  advance(); // consume second [

  std::string closing = "]";
  for (int i = 0; i < level; ++i)
    closing += "=";
  closing += "]";

  std::string content;
  while (!isAtEnd()) {
    bool match = true;
    for (size_t i = 0; i < closing.size(); ++i) {
      if (peek(static_cast<int>(i)) != closing[i]) {
        match = false;
        break;
      }
    }
    if (match) {
      for (size_t i = 0; i < closing.size(); ++i)
        advance();
      return Token(TokenKind::BracketString, std::move(content), start);
    }
    content += advance();
  }

  reporter_.fatal(filepath_, start.line, start.col,
                  "unterminated bracket string");
  return Token(TokenKind::BracketString, std::move(content), start);
}

Token Lexer::scanVarRef() {
  SourceLocation start = here();
  assert(peek() == '$');
  advance(); // consume $

  if (peek() == '<') {
    advance(); // consume <
    std::string expr;
    int depth = 1;
    while (!isAtEnd() && depth > 0) {
      char c = advance();
      if (c == '<')
        ++depth;
      else if (c == '>') {
        --depth;
        if (depth == 0)
          break;
      }
      expr += c;
    }
    if (depth > 0)
      reporter_.error(filepath_, start.line, start.col,
                      "unterminated generator expression");
    return Token(TokenKind::GenExpr, std::move(expr), start);
  }

  TokenKind kind = TokenKind::VarRef;
  if (source_.compare(pos_, 4, "ENV{") == 0) {
    kind = TokenKind::EnvVarRef;
    pos_ += 3;
    col_ += 3;
  } else if (source_.compare(pos_, 6, "CACHE{") == 0) {
    kind = TokenKind::CacheVarRef;
    pos_ += 5;
    col_ += 5;
  }

  if (peek() != '{') {
    reporter_.warning(filepath_, start.line, start.col,
                      "expected '{' after '$'");
    return Token(TokenKind::Unknown, "$", start);
  }
  advance(); // consume {

  std::string name;
  while (!isAtEnd() && peek() != '}') {
    name += advance();
  }
  if (isAtEnd()) {
    reporter_.fatal(filepath_, start.line, start.col,
                    "unterminated variable reference");
  } else {
    advance(); // consume }
  }

  return Token(kind, std::move(name), start);
}

Token Lexer::scanComment() {
  SourceLocation start = here();
  assert(peek() == '#');
  advance(); // consume #

  if (peek() == '[') {
    size_t saved_pos = pos_;
    uint32_t saved_col = col_;
    advance(); // consume [
    int level = 0;
    while (!isAtEnd() && peek() == '=') {
      ++level;
      advance();
    }
    if (peek() == '[') {
      pos_ = saved_pos;
      col_ = saved_col;
      Token inner = scanBracketString();
      return Token(TokenKind::BracketComment, inner.text, start);
    }
    pos_ = saved_pos;
    col_ = saved_col;
  }

  std::string text;
  while (!isAtEnd() && peek() != '\n') {
    text += advance();
  }
  size_t first = text.find_first_not_of(" \t");
  if (first != std::string::npos)
    text = text.substr(first);
  return Token(TokenKind::Comment, std::move(text), start);
}

Token Lexer::scanNumber() {
  SourceLocation start = here();
  std::string text;
  while (!isAtEnd() &&
         (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '.')) {
    text += advance();
  }
  return Token(TokenKind::Number, std::move(text), start);
}

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------
std::string Lexer::resolveEscapes(const std::string &raw,
                                  const SourceLocation &loc) {
  std::string out;
  out.reserve(raw.size());
  for (size_t i = 0; i < raw.size(); ++i) {
    if (raw[i] != '\\' || i + 1 >= raw.size()) {
      out += raw[i];
      continue;
    }
    ++i;
    switch (raw[i]) {
    case 'n':
      out += '\n';
      break;
    case 't':
      out += '\t';
      break;
    case 'r':
      out += '\r';
      break;
    case '"':
      out += '"';
      break;
    case '\\':
      out += '\\';
      break;
    case ';':
      out += ';';
      break;
    case '$':
      out += '$';
      break;
    default:
      reporter_.warning(filepath_, loc.line, loc.col,
                        std::string("unknown escape sequence '\\") + raw[i] +
                            "'");
      out += raw[i];
      break;
    }
  }
  return out;
}

TokenKind Lexer::classifyKeyword(const std::string &word) {
  std::string lower;
  lower.reserve(word.size());
  for (char c : word)
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  auto it = kKeywords.find(lower);
  if (it != kKeywords.end())
    return it->second;
  return TokenKind::Identifier;
}

} // namespace transpiler::lexer
