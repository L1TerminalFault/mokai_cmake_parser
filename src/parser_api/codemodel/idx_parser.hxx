#pragma once

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "utils/json.hxx"

namespace cmake_api::codemodel {

class IndexParser {
public:
  static std::string extractCodemodel(const std::filesystem::path &replyDir) {
    for (const auto &entry : std::filesystem::directory_iterator(replyDir)) {
      if (entry.path().filename().string().rfind("index-", 0) == 0) {
        std::ifstream file(entry.path());
        if (!file.is_open())
          continue;

        nlohmann::json indexJson;
        file >> indexJson;

        // Extracting via the official client registration hook string
        return indexJson["reply"]["client-mokai"]["query.json"]["responses"][0]
                        ["jsonFile"];
      }
    }
    throw std::runtime_error(
        "Could not locate a valid active environment index file.");
  }
};

} // namespace cmake_api::codemodel
