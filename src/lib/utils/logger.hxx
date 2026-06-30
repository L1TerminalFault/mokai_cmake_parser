#pragma once

#include <print>
#include <string>

namespace logger {

static constexpr std::string RESET = "\033[0m";
static constexpr std::string BOLD = "\033[1m";
static constexpr std::string RED = "\033[31m";
static constexpr std::string YELLOW = "\033[33m";
static constexpr std::string GREEN = "\033[32m";
static constexpr std::string BLUE = "\033[34m";
static constexpr std::string MAGENTA = "\033[35m";
static constexpr std::string DIM = "\033[2m";
static constexpr std::string GREY = "\033[2;90m";

struct Logger {
  // ANSI Escape Codes
  static void error(std::string msg) {
    std::println("{}{} Error   {}│{} {}{}", BOLD, RED, GREY, RED, BOLD, msg,
                 RESET);
  }

  static void warn(std::string msg) {
    std::println("{}{} Warning {}│{} {}", BOLD, YELLOW, GREY, RESET, msg);
  }

  static void log(std::string msg) {
    std::println("{} Log     {}│ {}{}", DIM, GREY, msg, RESET);
  }

  static void info(std::string msg) {
    std::println("{}{} Note    {}│{} {}", BOLD, BLUE, GREY, RESET, msg);
  }

  static void success(std::string msg) {
    std::println("{}{} Success {}│{} {}", BOLD, GREEN, GREY, RESET, msg);
  }

  static void tip(std::string msg) {
    std::println("{}{} Tip     {}│ {}{}", BOLD, MAGENTA, GREY, msg, RESET);
  }
};

} // namespace logger
