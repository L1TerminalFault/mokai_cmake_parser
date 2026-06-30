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
  // fs::path outputPath;
  // fs::path targetPath;

public:
  Transpiler(int argc, char **argv) {
    if (argc > 1 && argv[1]) {
      targetPath = fs::path(argv[1]).make_preferred().string();
    } else {
      targetPath = (fs::path(".") / "CMakeLists.txt").make_preferred().string();
    }

    // FIX: Removed duplicate block that was overwriting Windows-compatible
    // paths
    if (argc > 2 && argv[2]) {
      std::string arg2(argv[2]);
      if (arg2 == ".") {
        outputPath = (fs::path(".") / "mokai.toml").make_preferred().string();
      } else {
        outputPath = fs::path(arg2).make_preferred().string();
      }
    } else {
      outputPath = (fs::path(".") / "mokai.toml").make_preferred().string();
    }
  }

  static int printUsage() {
    std::println("\n{}{}Mokai CMake Transpiler Engine Pass{}", BOLD, MAGENTA,
                 RESET);
    std::println("{}{}==================================={}", DIM, MAGENTA,
                 RESET);
    std::println(
        "{}Usage:{} cmaketotoml <path/to/CMakeLists.txt> [output_path.toml]",
        BOLD, RESET);
    std::println("{}Note:{}  Providing '.' or leaving the second argument "
                 "blank defaults to './mokai.toml'\n",
                 DIM, RESET);
    return 0;
  }

  int run() override {
    // Check for help flags before opening any file streams
    if (targetPath == "--help" || targetPath == "-h") {
      return printUsage();
    }

    Logger::info(std::string("Target workspace entry file: ") +
                 targetPath.string());

    // Read the CMake file
    std::ifstream cmakeFile(targetPath);
    if (!cmakeFile) {
      Logger::error(
          std::string("Failed to open source descriptor stream at: '") +
          targetPath.string() + "'");
      return 1;
    }
    std::string cmakeSource((std::istreambuf_iterator<char>(cmakeFile)),
                            std::istreambuf_iterator<char>());

    // Get the source directory
    std::string sourceDir;
    try {
      sourceDir = fs::absolute(fs::path(targetPath).parent_path()).string();
    } catch (...) {
      sourceDir = fs::path(targetPath).parent_path().string();
    }
    if (sourceDir.empty())
      sourceDir = ".";
    std::string binaryDir = sourceDir;

    DiagnosticReporter reporter;

    // --- Lex ---
    Logger::log("Running Lexical analyzer scanning patterns...");
    Lexer lx(cmakeSource, targetPath, reporter);
    const auto &tokens = lx.tokenize();

    if (reporter.hasFatals()) {
      Logger::error(
          "Token scanning phase encountered critical structural faults.");
      reporter.dump();
      return 1;
    }

    // --- Parse ---
    Logger::log("Parsing structural tokens into AST nodes...");
    Parser parser(tokens, targetPath, reporter);
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
    // FIX: Only open the file *now* so we don't wipe existing files if the
    // compilation crashes mid-way
    std::ofstream outFile(outputPath);
    if (!outFile) {
      Logger::error(
          std::string("Failed to write to destination path stream: '") +
          outputPath.string() + "'");
      return 1;
    }

    outFile << tomlOutput;
    Logger::success(std::string("Generated transpiled manifest"));

    if (reporter.hasWarnings() || reporter.hasErrors()) {
      reporter.dump();
    }

    return reporter.hasErrors() ? 1 : 0;
  }
};

} // namespace transpiler::entry
