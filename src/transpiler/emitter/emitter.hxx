#pragma once

#include <string>

#include "analyzer/analyzer.hxx"
#include "manifest.hxx"

using namespace transpiler::analyzer;

namespace transpiler::emitter {

// -----------------------------------------------------------------------
// Emitter — walks the AnalyzerResult and produces a MokaiManifest.
// -----------------------------------------------------------------------
class Emitter {
public:
  Emitter(DiagnosticReporter &reporter, const std::string &sourceDir);

  // Transform an AnalyzerResult into a MokaiManifest
  MokaiManifest emit(const AnalyzerResult &result);

private:
  void emitProject(const ResolvedProject &proj);
  void emitOptions(const std::vector<ResolvedOption> &opts);
  void emitTargets(const std::vector<ResolvedTarget> &tgts);

  DiagnosticReporter &reporter_;
  std::string sourceDir_;
  MokaiManifest manifest_;
};

} // namespace transpiler::emitter
