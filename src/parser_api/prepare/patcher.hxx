#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include "utils/logger.hxx"

using namespace logger;
namespace fs = std::filesystem;

namespace cmake_api::patcher {

class CMakePatcher {
private:
  fs::path rootDir;
  std::set<fs::path> visitedFiles;

  // Global header payload injected ONLY at the very top of the root
  // CMakeLists.txt
  const std::string MOKAI_GLOBAL_HEADER =
      "# "
      "======================================================================="
      "\n"
      "# MOKAI BUILD ENGINE — ULTIMATE HOOK INTERCEPTOR\n"
      "# "
      "======================================================================="
      "\n"
      "if(MOKAI_PARSE_ONLY)\n"
      "    # Create a dedicated directory for our custom modules\n"
      "    set(MOKAI_MOCK_MODULES_DIR "
      "\"${CMAKE_BINARY_DIR}/mokai_intercept_modules\")\n"
      "    file(MAKE_DIRECTORY \"${MOKAI_MOCK_MODULES_DIR}\")\n"
      "    \n"
      "    # Write our own version of FindPackageHandleStandardArgs to disk "
      "instantly\n"
      "    file(WRITE "
      "\"${MOKAI_MOCK_MODULES_DIR}/FindPackageHandleStandardArgs.cmake\"\n"
      "        \"macro(find_package_handle_standard_args _pkg)\\n\"\n"
      "        \"    message(STATUS \\\"[Mokai Core Intercept] Neutralizing "
      "standard args for: ${_pkg}\\\")\\n\"\n"
      "        \"    set(${_pkg}_FOUND TRUE)\\n\"\n"
      "        \"    string(TOUPPER \\\"${_pkg}\\\" _upper_pkg)\\n\"\n"
      "        \"    set(${_upper_pkg}_FOUND TRUE)\\n\"\n"
      "        \"    if(NOT TARGET ${_pkg})\\n\"\n"
      "        \"        add_library(${_pkg} UNKNOWN IMPORTED GLOBAL)\\n\"\n"
      "        \"    endif()\\n\"\n"
      "        \"    if(NOT TARGET ${_pkg}::${_pkg})\\n\"\n"
      "        \"        add_library(${_pkg}::${_pkg} UNKNOWN IMPORTED "
      "GLOBAL)\\n\"\n"
      "        \"    endif()\\n\"\n"
      "        \"    return()\\n\"\n"
      "        \"endmacro()\\n\"\n"
      "    )\n"
      "    \n"
      "    # Force CMake to check our mock directory FIRST before checking "
      "system directories\n"
      "    list(INSERT CMAKE_MODULE_PATH 0 \"${MOKAI_MOCK_MODULES_DIR}\")\n"
      "    \n"
      "    # Universal fallback for simple find_package invocations\n"
      "    macro(find_package pkgName)\n"
      "        message(STATUS \"[Mokai Intercept] Standard package sweep: "
      "${pkgName}\")\n"
      "        set(${pkgName}_FOUND TRUE)\n"
      "        string(TOUPPER \"${pkgName}\" upperPkg)\n"
      "        set(${upperPkg}_FOUND TRUE)\n"
      "        if(NOT TARGET ${pkgName})\n"
      "            add_library(${pkgName} UNKNOWN IMPORTED GLOBAL)\n"
      "        endif()\n"
      "        if(NOT TARGET ${pkgName}::${pkgName})\n"
      "            add_library(${pkgName}::${pkgName} UNKNOWN IMPORTED "
      "GLOBAL)\n"
      "        endif()\n"
      "    endmacro()\n"
      "endif()\n"
      "# "
      "======================================================================="
      "\n\n";

  void scanForReferences(const std::string &content,
                         const fs::path &currentDir) {
    std::regex subdirRegex(R"(add_subdirectory\s*\(\s*([^)\s#]+))",
                           std::regex_constants::icase);
    auto subdirBegin =
        std::sregex_iterator(content.begin(), content.end(), subdirRegex);
    auto subdirEnd = std::sregex_iterator();

    for (std::sregex_iterator i = subdirBegin; i != subdirEnd; ++i) {
      std::string sub = (*i)[1].str();
      sub.erase(std::remove(sub.begin(), sub.end(), '"'), sub.end());
      sub.erase(std::remove(sub.begin(), sub.end(), '\''), sub.end());

      fs::path nextTarget = currentDir / sub / "CMakeLists.txt";
      if (fs::exists(nextTarget)) {
        patchFile(nextTarget, false);
      }
    }

    std::regex includeRegex(R"(include\s*\(\s*([^)\s#]+))",
                            std::regex_constants::icase);
    auto includeBegin =
        std::sregex_iterator(content.begin(), content.end(), includeRegex);
    auto includeEnd = std::sregex_iterator();

    for (std::sregex_iterator i = includeBegin; i != includeEnd; ++i) {
      std::string inc = (*i)[1].str();
      inc.erase(std::remove(inc.begin(), inc.end(), '"'), inc.end());
      inc.erase(std::remove(inc.begin(), inc.end(), '\''), inc.end());

      if (inc.find(".cmake") != std::string::npos ||
          inc.find('/') != std::string::npos ||
          inc.find('\\') != std::string::npos) {
        if (inc.find(".cmake") == std::string::npos) {
          inc += ".cmake";
        }
        fs::path nextTarget = currentDir / inc;
        if (fs::exists(nextTarget)) {
          patchFile(nextTarget, false);
        }
      }
    }
  }

  std::string processAndWrapFetchBlocks(const std::string &content) {
    std::string modifiedContent = content;
    std::smatch match;
    std::vector<std::pair<std::string, std::string>> replacements;

    // --- Pass 1: FetchContent_Declare ---
    std::regex declareRegex(
        R"((FetchContent_Declare\s*\(\s*([a-zA-Z0-9_\-]+)[^)]*\)))",
        std::regex_constants::icase);
    std::string::const_iterator searchStart(content.cbegin());

    while (
        std::regex_search(searchStart, content.cend(), match, declareRegex)) {
      std::string fullBlock = match[1].str();
      std::string depName = match[2].str();

      std::string wrappedBlock = "if(MOKAI_PARSE_ONLY)\n"
                                 "    if(NOT TARGET " +
                                 depName +
                                 ")\n"
                                 "        add_library(" +
                                 depName +
                                 " UNKNOWN IMPORTED GLOBAL)\n"
                                 "    endif()\n"
                                 "else()\n"
                                 "    " +
                                 fullBlock +
                                 "\n"
                                 "endif()";

      replacements.push_back({fullBlock, wrappedBlock});
      searchStart = match[0].second;
    }

    for (const auto &[original, replacement] : replacements) {
      size_t pos = modifiedContent.find(original);
      if (pos != std::string::npos) {
        modifiedContent.replace(pos, original.length(), replacement);
      }
    }

    // --- Pass 2: FetchContent_MakeAvailable ---
    std::regex makeAvailableRegex(
        R"((FetchContent_MakeAvailable\s*\(\s*([a-zA-Z0-9_\-\s]+)\)))",
        std::regex_constants::icase);
    searchStart = modifiedContent.cbegin();
    replacements.clear();

    while (std::regex_search(searchStart, modifiedContent.cend(), match,
                             makeAvailableRegex)) {
      std::string fullBlock = match[1].str();

      std::string wrappedBlock = "if(NOT MOKAI_PARSE_ONLY)\n"
                                 "    " +
                                 fullBlock +
                                 "\n"
                                 "endif()";

      replacements.push_back({fullBlock, wrappedBlock});
      searchStart = match[0].second;
    }

    for (const auto &[original, replacement] : replacements) {
      size_t pos = modifiedContent.find(original);
      if (pos != std::string::npos) {
        modifiedContent.replace(pos, original.length(), replacement);
      }
    }

    return modifiedContent;
  }

public:
  explicit CMakePatcher(fs::path targetDir)
      : rootDir(fs::absolute(targetDir)) {}

  void patchFile(fs::path filePath, bool isRoot = false) {
    filePath = fs::absolute(filePath);

    if (visitedFiles.count(filePath) || !fs::exists(filePath)) {
      return;
    }
    visitedFiles.insert(filePath);

    std::ifstream fileIn(filePath, std::ios::in | std::ios::binary);
    if (!fileIn)
      return;

    std::string content((std::istreambuf_iterator<char>(fileIn)),
                        std::istreambuf_iterator<char>());
    fileIn.close();

    if (content.find("MOKAI BUILD ENGINE") != std::string::npos) {
      return;
    }

    std::string updatedContent = processAndWrapFetchBlocks(content);

    if (isRoot) {
      updatedContent = MOKAI_GLOBAL_HEADER + updatedContent;
    }

    if (updatedContent != content || isRoot) {
      logger::Logger::log("Shielding build context layout: " +
                          fs::relative(filePath, rootDir).string());

      std::ofstream fileOut(filePath, std::ios::out | std::ios::binary);
      if (!fileOut)
        return;
      fileOut << updatedContent;
      fileOut.close();
    }

    scanForReferences(updatedContent, filePath.parent_path());
  }

  size_t getPatchedCount() const { return visitedFiles.size(); }
};

} // namespace cmake_api::patcher
