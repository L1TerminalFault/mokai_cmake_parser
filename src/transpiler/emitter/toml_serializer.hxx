#pragma once

#include <sstream>
#include <string>

#include "manifest.hxx"

namespace transpiler::emitter {

// -----------------------------------------------------------------------
// TomlSerializer — converts a MokaiManifest into a TOML string.
// Outputs valid TOML that can be parsed by toml11 or other parsers.
// -----------------------------------------------------------------------
class TomlSerializer {
public:
  // Serialize a manifest to a TOML string
  static std::string serialize(const MokaiManifest &manifest);

private:
  static void serializeProject(std::ostringstream &oss,
                               const ProjectManifest &p);
  static void
  serializeOptions(std::ostringstream &oss,
                   const std::unordered_map<std::string, bool> &opts);
  static void serializeCompat(std::ostringstream &oss,
                              const Compatibility &compat);
  static void serializeFileGroups(std::ostringstream &oss,
                                  const std::vector<FileGroup> &groups);
  static void serializePropertyGroups(std::ostringstream &oss,
                                      const std::vector<PropertyGroup> &groups);
  static void serializeHooks(std::ostringstream &oss,
                             const std::vector<Hook> &hooks);
  static void
  serializeTargets(std::ostringstream &oss,
                   const std::unordered_map<std::string, Target> &targets);
  static void serializeExports(std::ostringstream &oss, const Exports &exports);
  static void serializeOutput(std::ostringstream &oss, const Output &output);

  // Helpers
  static std::string escapeString(const std::string &s);
  static std::string quoteString(const std::string &s);
  static void writeArray(std::ostringstream &oss,
                         const std::vector<std::string> &items,
                         const std::string &indent = "");
};

} // namespace transpiler::emitter
