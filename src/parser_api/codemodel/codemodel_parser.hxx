#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "utils/json.hxx"

namespace cmake_api::codemodel {

struct ProjectMetadata {
  std::string projectName;
  std::vector<std::pair<std::string, std::string>>
      targets; // {target_name, target_json_file}
};

class CodemodelParser {
public:
  static ProjectMetadata
  resolveProjectGraph(const std::filesystem::path &codemodelPath) {
    std::ifstream file(codemodelPath);
    if (!file.is_open()) {
      throw std::runtime_error(
          "Parser: Unable to read codemodel graph configuration.");
    }

    nlohmann::json cmJson;
    file >> cmJson;

    ProjectMetadata meta;
    // Extract project source scope name
    meta.projectName =
        cmJson["configurations"][0]["projects"][0]["name"].get<std::string>();

    auto targets = cmJson["configurations"][0]["targets"];
    for (const auto &target : targets) {
      meta.targets.push_back({target["name"], target["jsonFile"]});
    }

    return meta;
  }
};

} // namespace cmake_api::codemodel
