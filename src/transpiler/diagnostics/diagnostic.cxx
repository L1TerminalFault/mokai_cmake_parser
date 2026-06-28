#include <iostream>
#include <sstream>

#include "diagnostic.hxx"

namespace transpiler::diagnostics {

// -----------------------------------------------------------------------
// Diagnostic
// -----------------------------------------------------------------------
std::string_view Diagnostic::severityName(DiagSeverity s) {
  switch (s) {
  case DiagSeverity::Note:
    return "note";
  case DiagSeverity::Warning:
    return "warning";
  case DiagSeverity::Error:
    return "error";
  case DiagSeverity::Fatal:
    return "fatal";
  }
  return "unknown";
}

std::string Diagnostic::format() const {
  // Format: "error: path/file.cmake:12:4: message"
  std::ostringstream oss;
  oss << severityName(severity) << ": ";
  if (!file.empty()) {
    oss << file << ":";
    if (line > 0) {
      oss << line << ":";
      if (col > 0)
        oss << col << ": ";
    }
  }
  oss << message;
  return oss.str();
}

// -----------------------------------------------------------------------
// DiagnosticReporter
// -----------------------------------------------------------------------
void DiagnosticReporter::report(DiagSeverity severity, const std::string &file,
                                uint32_t line, uint32_t col,
                                const std::string &message) {
  diags_.emplace_back(severity, file, line, col, message);
}

void DiagnosticReporter::note(const std::string &f, uint32_t l, uint32_t c,
                              const std::string &m) {
  report(DiagSeverity::Note, f, l, c, m);
}
void DiagnosticReporter::warning(const std::string &f, uint32_t l, uint32_t c,
                                 const std::string &m) {
  report(DiagSeverity::Warning, f, l, c, m);
}
void DiagnosticReporter::error(const std::string &f, uint32_t l, uint32_t c,
                               const std::string &m) {
  report(DiagSeverity::Error, f, l, c, m);
}
void DiagnosticReporter::fatal(const std::string &f, uint32_t l, uint32_t c,
                               const std::string &m) {
  report(DiagSeverity::Fatal, f, l, c, m);
}

bool DiagnosticReporter::hasErrors() const {
  for (const auto &d : diags_)
    if (d.severity == DiagSeverity::Error || d.severity == DiagSeverity::Fatal)
      return true;
  return false;
}

bool DiagnosticReporter::hasFatals() const {
  for (const auto &d : diags_)
    if (d.severity == DiagSeverity::Fatal)
      return true;
  return false;
}

bool DiagnosticReporter::hasWarnings() const {
  for (const auto &d : diags_)
    if (d.severity == DiagSeverity::Warning)
      return true;
  return false;
}

void DiagnosticReporter::dump() const {
  for (const auto &d : diags_)
    std::cerr << d.format() << "\n";
}

} // namespace transpiler::diagnostics
