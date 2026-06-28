#include <cassert>
#include <cctype>

#include "parser.hxx"

namespace transpiler::parser {

// -----------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------
Parser::Parser(const std::vector<Token> &tokens, const std::string &filepath,
               DiagnosticReporter &reporter)
    : tokens_(tokens), filepath_(filepath), reporter_(reporter), pos_(0) {}

// -----------------------------------------------------------------------
// Public: parse()
// -----------------------------------------------------------------------
std::unique_ptr<FileNode> Parser::parse() {
  auto root = std::make_unique<FileNode>();
  root->filepath = filepath_;
  root->loc = tokens_.empty() ? SourceLocation{} : tokens_[0].loc;

  skipNewlines();
  while (!isAtEnd()) {
    auto stmt = parseStatement();
    if (stmt)
      root->children.push_back(std::move(stmt));
    skipNewlines();
  }
  return root;
}

// -----------------------------------------------------------------------
// Token navigation
// -----------------------------------------------------------------------
const Token &Parser::peek(int offset) const {
  size_t idx = pos_ + static_cast<size_t>(offset);
  if (idx >= tokens_.size())
    return tokens_.back(); // EndOfFile
  return tokens_[idx];
}

const Token &Parser::advance() {
  if (!isAtEnd())
    ++pos_;
  return tokens_[pos_ - 1];
}

bool Parser::isAtEnd() const {
  return pos_ >= tokens_.size() || tokens_[pos_].kind == TokenKind::EndOfFile;
}

bool Parser::check(TokenKind k, int offset) const {
  return peek(offset).kind == k;
}

bool Parser::match(TokenKind k) {
  if (check(k)) {
    advance();
    return true;
  }
  return false;
}

void Parser::skipNewlines() {
  while (check(TokenKind::Newline) || check(TokenKind::Comment) ||
         check(TokenKind::BracketComment)) {
    advance();
  }
}

void Parser::expectNewlineOrEof() {
  // After a command's closing paren, skip comments then expect newline or EOF.
  while (check(TokenKind::Comment) || check(TokenKind::BracketComment))
    advance();
  if (!isAtEnd() && !match(TokenKind::Newline)) {
    reporter_.warning(filepath_, peek().loc.line, peek().loc.col,
                      "expected newline after command, got '" +
                          std::string(Token::kindName(peek().kind)) + "'");
  }
}

void Parser::synchronize() {
  while (!isAtEnd() && !check(TokenKind::Newline))
    advance();
  match(TokenKind::Newline);
}

// -----------------------------------------------------------------------
// parseStatement — dispatch
// -----------------------------------------------------------------------
NodePtr Parser::parseStatement() {
  // Skip stray newlines/comments
  skipNewlines();
  if (isAtEnd())
    return nullptr;

  const Token &tok = peek();

  // Must be an identifier or keyword to start a statement
  if (tok.kind != TokenKind::Identifier && !tok.isKeyword()) {
    reporter_.error(filepath_, tok.loc.line, tok.loc.col,
                    "expected command name, got '" +
                        std::string(Token::kindName(tok.kind)) + "'");
    synchronize();
    return nullptr;
  }

  std::string name = toLower(tok.text);

  // Control-flow nodes get their own parse functions
  if (name == "if")
    return parseIfBlock();
  if (name == "foreach")
    return parseForeachBlock();
  if (name == "while")
    return parseWhileBlock();
  if (name == "function")
    return parseFunctionDef();
  if (name == "macro")
    return parseMacroDef();

  // Everything else is a plain command
  advance(); // consume the name token
  return parseCommand(tok);
}

// -----------------------------------------------------------------------
// parseCommand — plain command, name already consumed
// -----------------------------------------------------------------------
NodePtr Parser::parseCommand(const Token &nameTok) {
  auto node = std::make_unique<CommandNode>();
  node->loc = nameTok.loc;
  node->name = toLower(nameTok.text);

  // Expect '('
  if (!match(TokenKind::LParen)) {
    reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                    "expected '(' after command '" + node->name + "'");
    synchronize();
    return node;
  }

  node->args = parseArgList();

  // Expect ')'
  if (!match(TokenKind::RParen)) {
    reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                    "expected ')' to close command '" + node->name + "'");
    synchronize();
    return node;
  }

  expectNewlineOrEof();
  return node;
}

// -----------------------------------------------------------------------
// parseBody — parse statements until a terminator keyword is seen
// -----------------------------------------------------------------------
NodeList Parser::parseBody(const std::vector<std::string> &terminators) {
  NodeList body;
  skipNewlines();

  while (!isAtEnd()) {
    // Check if next identifier is a terminator
    const Token &tok = peek();
    if (tok.kind == TokenKind::Identifier || tok.isKeyword()) {
      std::string name = toLower(tok.text);
      bool isTerminator = false;
      for (const auto &t : terminators)
        if (name == t) {
          isTerminator = true;
          break;
        }
      if (isTerminator)
        break;
    }

    auto stmt = parseStatement();
    if (stmt)
      body.push_back(std::move(stmt));
    skipNewlines();
  }
  return body;
}

// -----------------------------------------------------------------------
// parseIfBlock
// -----------------------------------------------------------------------
NodePtr Parser::parseIfBlock() {
  auto node = std::make_unique<IfBlockNode>();
  node->loc = peek().loc;

  // --- parse if(...) ---
  {
    const Token &ifTok = advance(); // consume 'if'
    IfBranch branch;
    branch.loc = ifTok.loc;

    if (!match(TokenKind::LParen)) {
      reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                      "expected '(' after 'if'");
      synchronize();
    } else {
      branch.condition = parseArgList();
      if (!match(TokenKind::RParen))
        reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                        "expected ')' after if condition");
      expectNewlineOrEof();
    }
    branch.body = parseBody({"elseif", "else", "endif"});
    node->branches.push_back(std::move(branch));
  }

  // --- parse elseif / else chains ---
  while (!isAtEnd()) {
    skipNewlines();
    const Token &tok = peek();
    std::string kw = toLower(tok.text);

    if (kw == "elseif") {
      IfBranch branch;
      branch.loc = tok.loc;
      advance(); // consume 'elseif'

      if (!match(TokenKind::LParen)) {
        reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                        "expected '(' after 'elseif'");
        synchronize();
      } else {
        branch.condition = parseArgList();
        if (!match(TokenKind::RParen))
          reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                          "expected ')' after elseif condition");
        expectNewlineOrEof();
      }
      branch.body = parseBody({"elseif", "else", "endif"});
      node->branches.push_back(std::move(branch));

    } else if (kw == "else") {
      IfBranch branch;
      branch.loc = tok.loc;
      advance(); // consume 'else'
      // else() may have empty parens
      if (match(TokenKind::LParen)) {
        parseArgList(); // ignore any args (CMake allows else())
        match(TokenKind::RParen);
      }
      expectNewlineOrEof();
      branch.body = parseBody({"endif"});
      node->branches.push_back(std::move(branch));
      break; // else is always last before endif

    } else if (kw == "endif") {
      break;
    } else {
      // Unexpected token inside if block
      reporter_.error(filepath_, tok.loc.line, tok.loc.col,
                      "unexpected token '" + tok.text + "' inside if block");
      synchronize();
      break;
    }
  }

  // --- consume endif() ---
  skipNewlines();
  if (toLower(peek().text) == "endif") {
    advance(); // consume 'endif'
    if (match(TokenKind::LParen)) {
      parseArgList();
      match(TokenKind::RParen);
    }
    expectNewlineOrEof();
  } else {
    reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                    "expected 'endif'");
  }

  return node;
}

// -----------------------------------------------------------------------
// parseForeachBlock
// -----------------------------------------------------------------------
NodePtr Parser::parseForeachBlock() {
  auto node = std::make_unique<ForeachNode>();
  node->loc = peek().loc;
  advance(); // consume 'foreach'

  if (!match(TokenKind::LParen)) {
    reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                    "expected '(' after 'foreach'");
    synchronize();
    return node;
  }

  auto allArgs = parseArgList();
  if (!match(TokenKind::RParen)) {
    reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                    "expected ')' after foreach arguments");
  }
  expectNewlineOrEof();

  // First arg is the loop variable
  if (!allArgs.empty()) {
    if (allArgs[0].isPlainLiteral())
      node->loopVar = allArgs[0].literal();
    allArgs.erase(allArgs.begin());
  }

  // Detect mode keywords: IN LISTS/ITEMS/ZIP_LISTS, RANGE
  if (!allArgs.empty() && allArgs[0].isPlainLiteral()) {
    std::string first = allArgs[0].literal();
    // Upper-case compare
    std::string up;
    for (char c : first)
      up += (char)toupper((unsigned char)c);

    if (up == "RANGE") {
      node->mode = "RANGE";
      allArgs.erase(allArgs.begin());
    } else if (up == "IN" && allArgs.size() > 1 &&
               allArgs[1].isPlainLiteral()) {
      std::string second = allArgs[1].literal();
      std::string up2;
      for (char c : second)
        up2 += (char)toupper((unsigned char)c);
      if (up2 == "LISTS" || up2 == "ITEMS" || up2 == "ZIP_LISTS") {
        node->mode = up2;
        allArgs.erase(allArgs.begin(), allArgs.begin() + 2);
      }
    }
  }
  node->items = std::move(allArgs);
  node->body = parseBody({"endforeach"});

  // consume endforeach()
  skipNewlines();
  if (toLower(peek().text) == "endforeach") {
    advance();
    if (match(TokenKind::LParen)) {
      parseArgList();
      match(TokenKind::RParen);
    }
    expectNewlineOrEof();
  } else {
    reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                    "expected 'endforeach'");
  }
  return node;
}

// -----------------------------------------------------------------------
// parseWhileBlock
// -----------------------------------------------------------------------
NodePtr Parser::parseWhileBlock() {
  auto node = std::make_unique<WhileNode>();
  node->loc = peek().loc;
  advance(); // consume 'while'

  if (!match(TokenKind::LParen)) {
    reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                    "expected '(' after 'while'");
    synchronize();
    return node;
  }
  node->condition = parseArgList();
  if (!match(TokenKind::RParen))
    reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                    "expected ')' after while condition");
  expectNewlineOrEof();

  node->body = parseBody({"endwhile"});

  skipNewlines();
  if (toLower(peek().text) == "endwhile") {
    advance();
    if (match(TokenKind::LParen)) {
      parseArgList();
      match(TokenKind::RParen);
    }
    expectNewlineOrEof();
  } else {
    reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                    "expected 'endwhile'");
  }
  return node;
}

// -----------------------------------------------------------------------
// parseFunctionDef
// -----------------------------------------------------------------------
NodePtr Parser::parseFunctionDef() {
  auto node = std::make_unique<FunctionDefNode>();
  node->loc = peek().loc;
  advance(); // consume 'function'

  if (!match(TokenKind::LParen)) {
    reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                    "expected '(' after 'function'");
    synchronize();
    return node;
  }
  auto args = parseArgList();
  if (!match(TokenKind::RParen))
    reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                    "expected ')' after function signature");
  expectNewlineOrEof();

  // First arg is function name, rest are params
  if (!args.empty()) {
    if (args[0].isPlainLiteral())
      node->name = args[0].literal();
    for (size_t i = 1; i < args.size(); ++i)
      if (args[i].isPlainLiteral())
        node->params.push_back(args[i].literal());
  }
  node->body = parseBody({"endfunction"});

  skipNewlines();
  if (toLower(peek().text) == "endfunction") {
    advance();
    if (match(TokenKind::LParen)) {
      parseArgList();
      match(TokenKind::RParen);
    }
    expectNewlineOrEof();
  } else {
    reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                    "expected 'endfunction'");
  }
  return node;
}

// -----------------------------------------------------------------------
// parseMacroDef
// -----------------------------------------------------------------------
NodePtr Parser::parseMacroDef() {
  auto node = std::make_unique<MacroDefNode>();
  node->loc = peek().loc;
  advance(); // consume 'macro'

  if (!match(TokenKind::LParen)) {
    reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                    "expected '(' after 'macro'");
    synchronize();
    return node;
  }
  auto args = parseArgList();
  if (!match(TokenKind::RParen))
    reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                    "expected ')' after macro signature");
  expectNewlineOrEof();

  if (!args.empty()) {
    if (args[0].isPlainLiteral())
      node->name = args[0].literal();
    for (size_t i = 1; i < args.size(); ++i)
      if (args[i].isPlainLiteral())
        node->params.push_back(args[i].literal());
  }
  node->body = parseBody({"endmacro"});

  skipNewlines();
  if (toLower(peek().text) == "endmacro") {
    advance();
    if (match(TokenKind::LParen)) {
      parseArgList();
      match(TokenKind::RParen);
    }
    expectNewlineOrEof();
  } else {
    reporter_.error(filepath_, peek().loc.line, peek().loc.col,
                    "expected 'endmacro'");
  }
  return node;
}

// -----------------------------------------------------------------------
// Argument parsing
// -----------------------------------------------------------------------
bool Parser::isArgumentStart() const {
  TokenKind k = peek().kind;
  return k == TokenKind::Identifier || k == TokenKind::String ||
         k == TokenKind::BracketString || k == TokenKind::Number ||
         k == TokenKind::VarRef || k == TokenKind::EnvVarRef ||
         k == TokenKind::CacheVarRef || k == TokenKind::GenExpr ||
         peek().isKeyword(); // keywords are valid as unquoted args (e.g.
                             // if(TRUE))
}

std::vector<Argument> Parser::parseArgList() {
  std::vector<Argument> args;
  skipNewlines();
  while (!isAtEnd() && !check(TokenKind::RParen)) {
    if (check(TokenKind::Newline) || check(TokenKind::Comment) ||
        check(TokenKind::BracketComment)) {
      advance();
      continue;
    }
    if (!isArgumentStart())
      break;
    args.push_back(parseArgument());
  }
  return args;
}

Argument Parser::parseArgument() {
  const Token &tok = peek();

  // Quoted string — may contain embedded ${VAR}
  if (tok.kind == TokenKind::String) {
    advance();
    Argument arg;
    arg.loc = tok.loc;
    arg.isQuoted = true;
    arg.parts = decomposeParts(tok.text, ArgPart::Kind::VarRef, tok.loc);
    return arg;
  }

  // Bracket string — treated as a single literal (no interpolation)
  if (tok.kind == TokenKind::BracketString) {
    advance();
    return tokenToArgument(tok);
  }

  // Standalone var refs / gen exprs become single-part arguments
  if (tok.kind == TokenKind::VarRef || tok.kind == TokenKind::EnvVarRef ||
      tok.kind == TokenKind::CacheVarRef || tok.kind == TokenKind::GenExpr) {
    advance();
    Argument arg;
    arg.loc = tok.loc;
    ArgPart part;
    part.value = tok.text;
    switch (tok.kind) {
    case TokenKind::VarRef:
      part.kind = ArgPart::Kind::VarRef;
      break;
    case TokenKind::EnvVarRef:
      part.kind = ArgPart::Kind::EnvVarRef;
      break;
    case TokenKind::CacheVarRef:
      part.kind = ArgPart::Kind::CacheVarRef;
      break;
    default:
      part.kind = ArgPart::Kind::GenExpr;
      break;
    }
    arg.parts.push_back(std::move(part));
    return arg;
  }

  // Unquoted identifier / keyword / number — plain literal
  advance();
  return tokenToArgument(tok);
}

Argument Parser::tokenToArgument(const Token &tok) {
  Argument arg;
  arg.loc = tok.loc;
  arg.isQuoted = false;
  ArgPart part;
  part.kind = ArgPart::Kind::Literal;
  part.value = tok.text;
  arg.parts.push_back(std::move(part));
  return arg;
}

// Decompose a raw string (from a quoted token) into ArgParts.
// Handles ${VAR}, $ENV{VAR}, $CACHE{VAR} embedded in the text.
// Note: the Lexer already handles top-level ${} tokens; this handles
// interpolation inside quoted strings where the Lexer gave us the raw content.
std::vector<ArgPart> Parser::decomposeParts(const std::string &text,
                                            ArgPart::Kind /*varKind*/,
                                            const SourceLocation & /*loc*/) {
  std::vector<ArgPart> parts;
  std::string literal;
  size_t i = 0;

  auto flushLiteral = [&]() {
    if (!literal.empty()) {
      parts.push_back({ArgPart::Kind::Literal, std::move(literal)});
      literal.clear();
    }
  };

  while (i < text.size()) {
    if (text[i] == '$' && i + 1 < text.size()) {
      // Check for ${VAR}
      if (text[i + 1] == '{') {
        size_t end = text.find('}', i + 2);
        if (end != std::string::npos) {
          flushLiteral();
          parts.push_back(
              {ArgPart::Kind::VarRef, text.substr(i + 2, end - (i + 2))});
          i = end + 1;
          continue;
        }
      }
      // $ENV{VAR}
      if (text.substr(i + 1, 4) == "ENV{") {
        size_t end = text.find('}', i + 5);
        if (end != std::string::npos) {
          flushLiteral();
          parts.push_back(
              {ArgPart::Kind::EnvVarRef, text.substr(i + 5, end - (i + 5))});
          i = end + 1;
          continue;
        }
      }
      // $CACHE{VAR}
      if (text.substr(i + 1, 6) == "CACHE{") {
        size_t end = text.find('}', i + 7);
        if (end != std::string::npos) {
          flushLiteral();
          parts.push_back(
              {ArgPart::Kind::CacheVarRef, text.substr(i + 7, end - (i + 7))});
          i = end + 1;
          continue;
        }
      }
      // $<genexpr> — find balanced >
      if (text[i + 1] == '<') {
        int depth = 1;
        size_t j = i + 2;
        while (j < text.size() && depth > 0) {
          if (text[j] == '<')
            ++depth;
          else if (text[j] == '>')
            --depth;
          ++j;
        }
        if (depth == 0) {
          flushLiteral();
          parts.push_back(
              {ArgPart::Kind::GenExpr, text.substr(i + 2, j - (i + 2) - 1)});
          i = j;
          continue;
        }
      }
    }
    literal += text[i++];
  }
  flushLiteral();

  // If nothing was found, return a single literal
  if (parts.empty())
    parts.push_back({ArgPart::Kind::Literal, text});

  return parts;
}

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------
std::string Parser::toLower(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s)
    out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

} // namespace transpiler::parser
