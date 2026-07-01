#pragma once

#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <string>

namespace fs = std::filesystem;

namespace cmake_api::prescan {

class PreScanner {
public:
  std::set<std::string> packages;
  std::set<std::string> modules;
  std::set<std::string> importedTargets;
  std::set<std::string> fetchProjects;

private:
  std::set<fs::path> visitedFiles;

public:
  void scanFile(const fs::path &file) {
    fs::path abs = fs::absolute(file);

    if (!fs::exists(abs))
      return;

    if (visitedFiles.contains(abs))
      return;

    visitedFiles.insert(abs);

    std::ifstream in(abs);
    if (!in)
      return;

    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());

    //
    // ----------------------------------------------------------
    // find_package(...)
    // ----------------------------------------------------------
    //
    {
      std::regex rx(R"(find_package\s*\(\s*([A-Za-z0-9_\-]+))",
                    std::regex::icase);

      for (std::sregex_iterator it(content.begin(), content.end(), rx), end;
           it != end; ++it) {
        packages.insert((*it)[1].str());
      }
    }

    //
    // ----------------------------------------------------------
    // include(...)
    // ----------------------------------------------------------
    //
    {
      std::regex rx(R"(include\s*\(\s*([A-Za-z0-9_\-./\\]+))",
                    std::regex::icase);

      for (std::sregex_iterator it(content.begin(), content.end(), rx), end;
           it != end; ++it) {
        modules.insert((*it)[1].str());
      }
    }

    //
    // ----------------------------------------------------------
    // FetchContent_Declare(...)
    // ----------------------------------------------------------
    //
    {
      std::regex rx(R"(FetchContent_Declare\s*\(\s*([A-Za-z0-9_\-]+))",
                    std::regex::icase);

      for (std::sregex_iterator it(content.begin(), content.end(), rx), end;
           it != end; ++it) {
        fetchProjects.insert((*it)[1].str());
      }
    }

    //
    // ----------------------------------------------------------
    // FetchContent_MakeAvailable(...)
    // ----------------------------------------------------------
    //
    {
      std::regex rx(R"(FetchContent_MakeAvailable\s*\(([^)]*)\))",
                    std::regex::icase);

      for (std::sregex_iterator it(content.begin(), content.end(), rx), end;
           it != end; ++it) {

        std::string args = (*it)[1].str();

        std::regex word(R"([A-Za-z0-9_\-]+)");

        for (std::sregex_iterator jt(args.begin(), args.end(), word), jend;
             jt != jend; ++jt) {
          fetchProjects.insert(jt->str());
        }
      }
    }

    //
    // ----------------------------------------------------------
    // Imported targets (Foo::Bar)
    // ----------------------------------------------------------
    //
    {
      std::regex rx(R"(([A-Za-z0-9_\-+.]+::[A-Za-z0-9_\-+.]+))");

      for (std::sregex_iterator it(content.begin(), content.end(), rx), end;
           it != end; ++it) {
        importedTargets.insert((*it)[1].str());
      }
    }
  }

  size_t packageCount() const { return packages.size(); }

  size_t importedTargetCount() const { return importedTargets.size(); }

  size_t fetchProjectCount() const { return fetchProjects.size(); }

  void clear() {
    packages.clear();
    modules.clear();
    importedTargets.clear();
    fetchProjects.clear();
    visitedFiles.clear();
  }
};

} // namespace cmake_api::prescan
