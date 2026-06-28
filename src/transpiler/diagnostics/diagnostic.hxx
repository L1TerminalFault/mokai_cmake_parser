#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace transpiler::diagnostics {

// -----------------------------------------------------------------------
// Severity levels — mirrors what you'd expect from clang/gcc output.
// -----------------------------------------------------------------------
enum class DiagSeverity {
  Note,    // informational, never blocks the pipeline
  Warning, // suspicious but we can make a best-effort translation
  Error,   // translation is wrong or incomplete — still continues
  Fatal,   // cannot continue parsing this file (e.g. unterminated bracket)
};

// -----------------------------------------------------------------------
// A single diagnostic event.
// Every field is always filled — no optionals here.
// -----------------------------------------------------------------------
struct Diagnostic {
  DiagSeverity severity;
  std::string file;
  uint32_t line; // 1-based; 0 = unknown
  uint32_t col;  // 1-based; 0 = unknown
  std::string message;

  Diagnostic(DiagSeverity s, std::string f, uint32_t l, uint32_t c,
             std::string m)
      : severity(s), file(std::move(f)), line(l), col(c),
        message(std::move(m)) {}

  // One-liner for log output:  error: src/foo.cmake:12:4: unknown token '$'
  std::string format() const;

  static std::string_view severityName(DiagSeverity s);
};

// -----------------------------------------------------------------------
// DiagnosticReporter — shared sink passed through the entire pipeline.
//
// Every stage (Lexer, Parser, Analyzer, Emitter) holds a reference to the
// same reporter so all diagnostics end up in one ordered list.
// The pipeline checks hasErrors() after each stage before continuing.
// -----------------------------------------------------------------------
class DiagnosticReporter {
public:
  void report(DiagSeverity severity, const std::string &file, uint32_t line,
              uint32_t col, const std::string &message);

  // Convenience wrappers
  void note(const std::string &file, uint32_t l, uint32_t c,
            const std::string &msg);
  void warning(const std::string &file, uint32_t l, uint32_t c,
               const std::string &msg);
  void error(const std::string &file, uint32_t l, uint32_t c,
             const std::string &msg);
  void fatal(const std::string &file, uint32_t l, uint32_t c,
             const std::string &msg);

  bool hasErrors() const; // Error or Fatal present
  bool hasFatals() const; // Fatal present
  bool hasWarnings() const;

  const std::vector<Diagnostic> &all() const { return diags_; }

  // Print all diagnostics to stderr in a clang-style format
  void dump() const;

  // Reset — used between test cases
  void clear() { diags_.clear(); }

private:
  std::vector<Diagnostic> diags_;
};

} // namespace transpiler::diagnostics
