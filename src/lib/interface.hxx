#pragma once

#include <filesystem>

class IParser {
public:
  std::filesystem::path targetPath;
  std::filesystem::path outputPath;

  virtual int run() = 0;
  virtual ~IParser() {};
};
