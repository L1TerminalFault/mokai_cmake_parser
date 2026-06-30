#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "analyzer/analyzer.hxx"
#include "diagnostics/diagnostic.hxx"
#include "emitter/emitter.hxx"
#include "emitter/toml_serializer.hxx"
#include "interface.hxx"
#include "lexer/lexer.hxx"
#include "parser/parser.hxx"
#include "utils/logger.hxx"

using namespace transpiler::analyzer;
using namespace transpiler::diagnostics;
using namespace transpiler::emitter;
using namespace transpiler::lexer;
using namespace transpiler::parser;
using namespace logger;

namespace fs = std::filesystem;

namespace transpiler::entry {

class Transpiler : public IParser {
public:
  Transpiler(int argc, char **argv) {
    // FIX 1: If no argument is passed, treat the target directory as current
    // directory "."
    fs::path resolvedTargetDir =
        (argc > 1 && argv[1]) ? fs::path(argv[1]) : fs::path(".");

    // Explicitly pair it to the file node for the stream check pass later
    if (fs::is_directory(resolvedTargetDir)) {
      targetPath = (resolvedTargetDir / "CMakeLists.txt").make_preferred();
    } else {
      targetPath = resolvedTargetDir.make_preferred();
    }

    // Output destination parsing logic
    if (argc > 2 && argv[2]) {
      std::string arg2(argv[2]);
      if (arg2 == ".") {
        outputPath = (fs::path(".") / "mokai.toml").make_preferred();
      } else {
        outputPath = fs::path(arg2).make_preferred();
      }
    } else {
      // If no output target folder path is explicit, fallback relative to the
      // input file path
      outputPath = (targetPath.parent_path() / "mokai.toml").make_preferred();
    }
  }

  int run() override {
    Logger::info("Parsing using static transpiler", "static transpiler");

    // FIX 2: Validate that the file target descriptor structurally exists
    // before streaming
    if (!fs::exists(targetPath) || fs::is_directory(targetPath)) {
      Logger::error("Mokai Error: No valid 'CMakeLists.txt' found at "
                    "structural pathway: '" +
                    fs::absolute(targetPath).string() + "'");
      return 1;
    }

    // Read the CMake file safely
    std::ifstream cmakeFile(targetPath);
    if (!cmakeFile) {
      Logger::error("Failed to open source descriptor stream at: '" +
                    targetPath.string() + "'");
      return 1;
    }
    std::string cmakeSource((std::istreambuf_iterator<char>(cmakeFile)),
                            std::istreambuf_iterator<char>());

    // FIX 3: Safe, non-throwing environment layout extraction passes
    std::string sourceDir;
    try {
      sourceDir = fs::absolute(targetPath.parent_path()).string();
    } catch (...) {
      sourceDir = targetPath.parent_path().string();
    }
    if (sourceDir.empty())
      sourceDir = ".";
    std::string binaryDir = sourceDir;

    DiagnosticReporter reporter;

    // --- Lex ---
    Logger::log("Running Lexical analyzer scanning patterns...");
    Lexer lx(cmakeSource, targetPath.string(), reporter);
    const auto &tokens = lx.tokenize();

    if (reporter.hasFatals()) {
      Logger::error(
          "Token scanning phase encountered critical structural faults.");
      reporter.dump();
      return 1;
    }

    // --- Parse ---
    Logger::log("Parsing structural tokens into AST nodes...");
    Parser parser(tokens, targetPath.string(), reporter);
    auto root = parser.parse();

    if (reporter.hasFatals()) {
      Logger::error("Grammar parser rejected source engine token alignments.");
      reporter.dump();
      return 1;
    }

    // --- Analyze ---
    Logger::log("Analyzing compilation graphs and metadata targets...");
    Analyzer analyzer(reporter, sourceDir, binaryDir);
    AnalyzerResult analysisResult = analyzer.analyze(std::move(root));

    if (reporter.hasErrors()) {
      Logger::warn(
          "Semantic pass recovered from minor target configuration anomalies.");
      reporter.dump();
    }

    // --- Emit ---
    Logger::log("Emitting internal manifest representations...");
    Emitter emitter(reporter, sourceDir);
    MokaiManifest manifest = emitter.emit(analysisResult);

    // --- Serialize to TOML ---
    Logger::log("Running TOML document node layout construction pass...");
    std::string tomlOutput = TomlSerializer::serialize(manifest);

    // --- Write output ---
    std::ofstream outFile(outputPath);
    if (!outFile) {
      Logger::error("Failed to write to destination path stream: '" +
                    outputPath.string() + "'");
      return 1;
    }

    outFile << tomlOutput;
    Logger::success("Generated transpiled manifest at: " + outputPath.string());

    if (reporter.hasWarnings() || reporter.hasErrors()) {
      reporter.dump();
    }

    return reporter.hasErrors() ? 1 : 0;
  }
};

} // namespace transpiler::entry
