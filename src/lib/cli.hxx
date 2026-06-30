#pragma once

#include "entry_parser.hxx"
#include "entry_transpiler.hxx"
#include "interface.hxx"
#include "utils/logger.hxx"

using namespace transpiler::entry;
using namespace cmake_api::entry;
using namespace logger;

namespace cli {

class Cli {
  int argc_;
  char **argv_;
  IParser *parser;

public:
  Cli(int argc, char **argv) : argc_(argc), argv_(argv) {}

  static int printUsage() {
    std::println("\n{}{}Mokai CMake Transpiler Engine Pass{}", BOLD, MAGENTA,
                 RESET);
    std::println("{}{}==================================={}", DIM, MAGENTA,
                 RESET);
    std::println("{}Usage:{} cmaketotoml "
                 "[path/to/project_dir_or_CMakeLists.txt] [output_path.toml]",
                 BOLD, RESET);
    std::println("{}Note:{} Leaving arguments blank defaults tracking paths "
                 "automatically.\n",
                 DIM, RESET);
    return 0;
  }

  int run() {
    fs::path targetProject = (argc_ >= 2) ? argv_[1] : ".";

    // if (targetProject.string() == "--help" || targetProject.string() == "-h")
    // {
    //   return printUsage();
    // }

    // 2. Validate structural contract (Ensure CMakeLists.txt is present)
    fs::path cmakeListsPath = targetProject / "CMakeLists.txt";
    if (!fs::exists(cmakeListsPath)) {
      logger::Logger::error("No valid 'CMakeLists.txt' found at: " +
                            fs::absolute(targetProject).string() + "\n");
      return 1;
    }
    ApiParser p(argc_, argv_);
    // parser = &p;
    if (!p.run())
      return 0;

    logger::Logger::warn(
        "CMake API parser failed. Trying static transpilation, this method is "
        "not guaranteed for large projects");
    Transpiler pa(argc_, argv_);
    // parser = &pa;
    return pa.run();
  }
};

} // namespace cli
