#pragma once

#include <string>

#include "analyzer/analyzer.hxx"
#include "manifest.hxx"

using namespace transpiler::analyzer;

namespace transpiler::emitter {

// -----------------------------------------------------------------------
// Emitter — walks the AnalyzerResult and produces a MokaiManifest.
// -----------------------------------------------------------------------
// class Emitter {
// public:
//   Emitter(DiagnosticReporter &reporter, const std::string &sourceDir);
//
//   // Transform an AnalyzerResult into a MokaiManifest
//   MokaiManifest emit(const AnalyzerResult &result);
//
// private:
//   void emitProject(const ResolvedProject &proj);
//   void emitOptions(const std::vector<ResolvedOption> &opts);
//   void emitTargets(const std::vector<ResolvedTarget> &tgts);
//
//   DiagnosticReporter &reporter_;
//   std::string sourceDir_;
//   MokaiManifest manifest_;
// };

class Emitter {
public:
  Emitter(DiagnosticReporter &reporter, const std::string &sourceDir,
          const std::string &outputDir = "");

  // Transform an AnalyzerResult into a MokaiManifest
  // Paths in the result are absolute; emitted paths are relative to outputDir
  MokaiManifest emit(const AnalyzerResult &result);

private:
  // Convert an absolute path to relative (from outputDir)
  std::string makeRelative(const std::string &absolutePath) const;
  void emitProject(const ResolvedProject &proj);
  void emitOptions(const std::vector<ResolvedOption> &opts);
  void emitTargets(const std::vector<ResolvedTarget> &tgts);

  DiagnosticReporter &reporter_;
  std::string sourceDir_;
  std::string outputDir_;
  MokaiManifest manifest_;
};

} // namespace transpiler::emitter
//
