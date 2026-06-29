#pragma once

#include <print>
#include <string_view>

namespace logger {

static constexpr std::string_view RESET = "\033[0m";
static constexpr std::string_view BOLD = "\033[1m";
static constexpr std::string_view RED = "\033[31m";
static constexpr std::string_view YELLOW = "\033[33m";
static constexpr std::string_view GREEN = "\033[32m";
static constexpr std::string_view BLUE = "\033[34m";
static constexpr std::string_view MAGENTA = "\033[35m";
static constexpr std::string_view DIM = "\033[2m";
static constexpr std::string_view GREY = "\033[2;90m";

struct Logger {
  // ANSI Escape Codes
  static void error(std::string_view msg) {
    std::println("{}{}   error   {}│{} {}{}", BOLD, RED, GREY, RED, BOLD, msg, RESET);
  }

  static void warn(std::string_view msg) {
    std::println("{}{}   warning {}│{} {}", BOLD, YELLOW, GREY, RESET, msg);
  }

  static void log(std::string_view msg) {
    std::println("{}   log     {}│ {}{}", DIM, GREY, msg, RESET);
  }

  static void info(std::string_view msg) {
    std::println("{}{}   note    {}│{} {}", BOLD, BLUE, GREY, RESET, msg);
  }

  static void success(std::string_view msg) {
    std::println("{}{}   success {}│{} {}", BOLD, GREEN, GREY, RESET, msg);
  }

  static void tip(std::string_view msg) {
    std::println("{}{}  tip     {}│ {}{}", BOLD, MAGENTA, GREY, msg, RESET);
  }
};

} // namespace logger
