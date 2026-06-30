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
    std::println("{} Error     {}{}", RED, msg, RESET);
  }

  static void warn(std::string msg) {
    std::println("{} Warning   {}{}", YELLOW, RESET, msg);
  }

  static void log(std::string msg) {
    std::println("{} Log       {}{}", GREY, msg, RESET);
  }

  static void info(std::string msg) {
    std::println("{} Info      {}{}", BLUE, RESET, msg);
  }

  static void success(std::string msg) {
    std::println("{} Success   {}{}", GREEN, RESET, msg);
  }

  static void tip(std::string msg) {
    std::println("{} Tip       {}{}", MAGENTA, msg, RESET);
  }
};

} // namespace logger
