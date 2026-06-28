#pragma once

#include <print>
#include <string_view>

namespace transpiler::logger {

struct Logger {
  // ANSI Escape Codes for Colors
  static constexpr std::string_view RESET = "\033[0m";
  static constexpr std::string_view RED = "\033[31m";
  static constexpr std::string_view YELLOW = "\033[33m";
  static constexpr std::string_view GREEN = "\033[32m";
  static constexpr std::string_view BLUE = "\033[34m";
  static constexpr std::string_view MAGENTA = "\033[35m";

  static void error(std::string_view msg) {
    std::println("{}[ERROR]: {}{}", RED, msg, RESET);
  }

  static void warn(std::string_view msg) {
    std::println("{}[WARNING]: {}{}", YELLOW, msg, RESET);
  }

  static void log(std::string_view msg) {
    std::println("{}[LOG]: {}{}", RESET, msg, RESET);
  }

  static void info(std::string_view msg) {
    std::println("{}[NOTE]: {}{}", BLUE, msg, RESET);
  }

  static void tip(std::string_view msg) {
    std::println("{}[TIP]: {}{}", MAGENTA, msg, RESET);
  }

  static void printUsage(const char *prog) {
    log(std::string("usage: ") + prog +
        "[path/to/CMakeLists.txt] [output.toml]\n\n");
    log("Converts a CMake project to a mokai build system manifest.");
  }
};

} // namespace transpiler::logger
