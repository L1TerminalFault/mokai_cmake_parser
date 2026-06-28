#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "analyzer/analyzer.hxx"
#include "diagnostics/diagnostic.hxx"
#include "emitter/emitter.hxx"
#include "emitter/toml_serializer.hxx"
#include "lexer/lexer.hxx"
#include "parser/parser.hxx"

using namespace transpiler::analyzer;
using namespace transpiler::diagnostics;
using namespace transpiler::emitter;
using namespace transpiler::lexer;
using namespace transpiler::parser;
namespace fs = std::filesystem;

namespace cli {

class Cli {
  std::string outputPath;
  std::string cmakePath;

public:
  Cli(char **argv) {
    cmakePath = argv[1] ?: "./CMakeLists";
    outputPath = /*(argc > 2) ?*/ argv[2] ?: "./mokai.toml";
  }

  int run() {

    // Read the CMake file
    std::ifstream cmakeFile(cmakePath);
    if (!cmakeFile) {
      std::cerr << "error: cannot open '" << cmakePath << "'\n";
      return 1;
    }
    std::string cmakeSource((std::istreambuf_iterator<char>(cmakeFile)),
                            std::istreambuf_iterator<char>());

    // Get the source directory
    std::string sourceDir;
    try {
      sourceDir = fs::absolute(fs::path(cmakePath).parent_path()).string();
    } catch (...) {
      sourceDir = fs::path(cmakePath).parent_path().string();
    }
    if (sourceDir.empty())
      sourceDir = ".";
    std::string binaryDir =
        sourceDir; // simplified: assume build in-tree for now

    DiagnosticReporter reporter;

    // --- Lex ---
    Lexer lx(cmakeSource, cmakePath, reporter);
    const auto &tokens = lx.tokenize();

    if (reporter.hasFatals()) {
      std::cerr << "Lexing failed:\n";
      reporter.dump();
      return 1;
    }

    // --- Parse ---
    Parser parser(tokens, cmakePath, reporter);
    auto root = parser.parse();

    if (reporter.hasFatals()) {
      std::cerr << "Parsing failed:\n";
      reporter.dump();
      return 1;
    }

    // --- Analyze ---
    Analyzer analyzer(reporter, sourceDir, binaryDir);
    AnalyzerResult analysisResult = analyzer.analyze(std::move(root));

    if (reporter.hasErrors()) {
      std::cerr << "Analysis had errors:\n";
      reporter.dump();
      // Continue anyway — we still produce output
    }

    // --- Emit ---
    Emitter emitter(reporter, sourceDir);
    MokaiManifest manifest = emitter.emit(analysisResult);

    // --- Serialize to TOML ---
    std::string tomlOutput = TomlSerializer::serialize(manifest);

    // --- Write output ---
    if (outputPath.empty()) {
      std::cout << tomlOutput;
    } else {
      std::ofstream outFile(outputPath);
      if (!outFile) {
        std::cerr << "error: cannot open '" << outputPath << "' for writing\n";
        return 1;
      }
      outFile << tomlOutput;
      std::cout << "✓ Wrote mokai manifest to: " << outputPath << "\n";
    }

    // Print diagnostics summary
    if (reporter.hasWarnings() || reporter.hasErrors()) {
      reporter.dump();
    }

    return reporter.hasErrors() ? 1 : 0;
  }
};

}; // namespace cli
