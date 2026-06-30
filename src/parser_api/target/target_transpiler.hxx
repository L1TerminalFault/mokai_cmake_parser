#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

#include "utils/json.hxx"

namespace fs = std::filesystem;

namespace cmake_api::target {

class TargetTranspiler {
private:
  static std::string mapTargetType(const std::string &cmakeType) {
    if (cmakeType == "STATIC_LIBRARY")
      return "static_library";
    if (cmakeType == "SHARED_LIBRARY")
      return "shared_library";
    if (cmakeType == "EXECUTABLE")
      return "executable";
    return "static_library";
  }

  static bool isSourceFile(const fs::path &path) {
    std::string ext = path.extension().string();
    for (auto &c : ext)
      c = std::tolower(c);
    return (ext == ".cpp" || ext == ".cc" || ext == ".c" || ext == ".cxx");
  }

  static bool isHeaderFile(const fs::path &path) {
    std::string ext = path.extension().string();
    for (auto &c : ext)
      c = std::tolower(c);
    return (ext == ".h" || ext == ".hpp" || ext == ".hxx" || ext == ".inl");
  }

public:
  static void transpile(const fs::path &blueprintPath,
                        std::ostream &outStream) {
    std::ifstream file(blueprintPath);
    if (!file.is_open())
      return;

    nlohmann::json targetJson;
    file >> targetJson;

    std::string name = targetJson["name"];
    std::string mkaType = mapTargetType(targetJson["type"]);

    fs::path sourceDir =
        targetJson.contains("paths") && targetJson["paths"].contains("source")
            ? targetJson["paths"]["source"].get<std::string>()
            : "";

    outStream << "\n# "
                 "-------------------------------------------------------------"
                 "----------\n";
    outStream << "# TARGET DEFINITION: " << name << "\n";
    outStream << "# "
                 "-------------------------------------------------------------"
                 "----------\n";
    outStream << "[target." << name << "]\n";
    outStream << "type = \"" << mkaType << "\"\n";

    // Collections for de-duplicating fields across compile groups
    std::set<std::string> targetSources;
    std::set<std::string> targetIncludeDirs;
    std::set<std::string> targetDefines;
    std::set<std::string> targetFlags;

    // 1. Process individual source entries using their compileGroupIndex
    if (targetJson.contains("sources") &&
        targetJson.contains("compileGroups")) {
      auto compileGroups = targetJson["compileGroups"];

      for (const auto &src : targetJson["sources"]) {
        fs::path rawPath = src["path"].get<std::string>();
        fs::path resolvedPath =
            rawPath.is_absolute() ? rawPath : (sourceDir / rawPath);

        // Split files into sources or include paths based on type
        if (isSourceFile(resolvedPath)) {
          targetSources.insert(resolvedPath.string());
        } else if (isHeaderFile(resolvedPath)) {
          targetIncludeDirs.insert(
              resolvedPath.string()); // Maps explicitly listed headers here
        }

        // If this file belongs to a compile group, pull its definitions and
        // include paths
        if (src.contains("compileGroupIndex")) {
          size_t cgIdx = src["compileGroupIndex"].get<size_t>();
          if (cgIdx < compileGroups.size()) {
            auto group = compileGroups[cgIdx];

            // Extract compilation macros (-D)
            if (group.contains("defines")) {
              for (const auto &def : group["defines"]) {
                targetDefines.insert(def["define"].get<std::string>());
              }
            }

            // Extract search paths (-I)
            if (group.contains("includes")) {
              for (const auto &inc : group["includes"]) {
                targetIncludeDirs.insert(inc["path"].get<std::string>());
              }
            }

            // Extract raw compiler optimizations/flags
            if (group.contains("compileCommandFragments")) {
              for (const auto &frag : group["compileCommandFragments"]) {
                std::string rawFragment = frag["fragment"].get<std::string>();
                if (rawFragment.empty())
                  continue;

                // FIX: Use a stringstream to split space-separated clusters
                // into individual tokens
                std::stringstream ss(rawFragment);
                std::string subFlag;
                while (ss >> subFlag) {
                  // Filter out preprocessor macros and standard overrides
                  if (subFlag.rfind("-D", 0) != 0 && subFlag != "-std=c++23") {
                    targetFlags.insert(subFlag);
                  } else if (subFlag.rfind("-D", 0) == 0) {
                    // If a -D macro accidentally leaked into fragments,
                    // redirect it to defines
                    targetDefines.insert(subFlag.substr(2));
                  }
                }
              }
            }
          }
        }
      }
    }

    // 2. Output Translation Units
    outStream << "sources = [\n";
    for (const auto &src : targetSources) {
      outStream << "  \"" << src << "\",\n";
    }
    outStream << "]\n";

    // 3. Output Unified Headers and Search Paths
    outStream << "include_dirs = [\n";
    for (const auto &dir : targetIncludeDirs) {
      outStream << "  \"" << dir << "\",\n";
    }
    outStream << "]\n";

    // 4. Output Extracted Preprocessor Definitions
    outStream << "defines = [\n";
    for (const auto &def : targetDefines) {
      outStream << "  \"" << def << "\",\n";
    }
    outStream << "]\n";

    // 5. Output Compiler Flags
    outStream << "flags = [\n";
    for (const auto &flag : targetFlags) {
      outStream << "  \"" << flag << "\",\n";
    }
    outStream << "]\n";

    // 6. Trace Linked System Libraries
    outStream << "system_libs = [\n";
    if (targetJson.contains("link") &&
        targetJson["link"].contains("commandFragments")) {
      for (const auto &frag : targetJson["link"]["commandFragments"]) {
        if (frag["role"].get<std::string>() == "libraries") {
          std::string libStr = frag["fragment"].get<std::string>();
          if (libStr.rfind("-l", 0) == 0) {
            outStream << "  \"" << libStr.substr(2) << "\",\n";
          }
        }
      }
    }
    outStream << "]\n";

    // 7. Upstream Module Requirements
    outStream << "depends_on = [\n";
    if (targetJson.contains("dependencies")) {
      for (const auto &dep : targetJson["dependencies"]) {
        std::string fullId = dep["id"].get<std::string>();
        size_t splitPos = fullId.find("::");
        if (splitPos != std::string::npos) {
          outStream << "  \"" << fullId.substr(0, splitPos) << "\",\n";
        }
      }
    }
    outStream << "]\n";
  }
};

} // namespace cmake_api::target
