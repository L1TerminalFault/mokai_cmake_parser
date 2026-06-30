#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace transpiler::emitter {

// -----------------------------------------------------------------------
// Manifest structures — direct 1:1 mirror of the mokai TOML spec.
// The emitter fills these in, then serializes to TOML.
// -----------------------------------------------------------------------

struct ProjectManifest {
  std::string name;
  std::optional<std::string> version;
  std::optional<std::string> license;
  std::optional<std::string> description;
  std::optional<std::string> homepage;
  std::optional<std::string> default_compiler;
  std::optional<std::string> cpp_version;

  struct {
    std::string file;
    std::string pattern;
  } version_from;

  std::vector<std::string> authors;
  std::vector<std::string> include_dirs;
  std::vector<std::string> dependencies;
};

struct PropertyGroup {
  std::string name;
  std::optional<std::string> condition;
  std::vector<std::string> defines;
  std::vector<std::string> flags;
};

struct Hook {
  std::string name;
  std::string on; // "pre_build", "post_build", "pre_target_build", etc.
  std::string run;
  std::optional<std::string> target;
  std::optional<std::string> pattern;
};

struct FileGroup {
  std::string name;
  std::vector<std::string> patterns;
};

struct TargetSourcesIf {
  std::string condition;
  std::vector<std::string> patterns;
};

struct TargetFlagsIf {
  std::string condition;
  std::vector<std::string> flags;
};

struct TargetPropertiesIf {
  std::string condition;
  std::vector<std::string> defines;
  std::vector<std::string> depends_on;
};

struct Target {
  std::string name;
  std::string type; // "executable", "static_library", "shared_library"
  std::vector<std::string> sources;
  std::vector<std::string> include_dirs;
  std::vector<std::string> properties; // @property_group names
  std::vector<std::string> flags;
  std::vector<std::string> system_libs;
  std::vector<std::string> depends_on;

  std::vector<TargetSourcesIf> sources_if;
  std::vector<TargetFlagsIf> flags_if;
  std::vector<TargetPropertiesIf> properties_if;
};

struct ExportProfile {
  std::string name;
  std::vector<std::string> targets;
};

struct Exports {
  std::vector<std::string> default_targets;
  std::vector<std::string> include_dirs;
  std::vector<std::string> defines_required;
  std::vector<std::string> defines_optional;
  std::unordered_map<std::string, ExportProfile> profiles;
};

struct OutputConfig {
  bool enabled;
  std::string subdir;
};

struct Output {
  std::string directory;
  std::unordered_map<std::string, OutputConfig> configs;
};

struct Compatibility {
  std::string min_cpp_version;
  std::optional<std::string> preferred_cpp_version;
  std::vector<std::string> unsupported_cpp_versions;

  struct Compilers {
    std::vector<std::string> supported;
    std::vector<std::string> unsupported;
  } compilers;
};

// -----------------------------------------------------------------------
// Full Manifest — the root containing everything
// -----------------------------------------------------------------------
struct MokaiManifest {
  ProjectManifest project;

  // [options]
  std::unordered_map<std::string, bool> options;
  Compatibility compatibility;
  std::vector<FileGroup> file_groups;

  // [property_group.NAME] - keyed by name
  std::unordered_map<std::string, PropertyGroup> property_groups;
  std::vector<Hook> hooks;

  // [target.NAME] - keyed by name
  std::unordered_map<std::string, Target> targets;

  // [exports]
  Exports exports;
  Output output;
};

} // namespace transpiler::emitter
