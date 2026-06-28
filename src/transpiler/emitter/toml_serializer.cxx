#include <algorithm>

#include "emitter/manifest.hxx"
#include "toml_serializer.hxx"

namespace transpiler::emitter {

std::string TomlSerializer::serialize(const MokaiManifest &manifest) {
  std::ostringstream oss;

  // Sections in order per spec
  serializeProject(oss, manifest.project);
  serializeOptions(oss, manifest.options);
  serializeCompat(oss, manifest.compatibility);
  serializeFileGroups(oss, manifest.file_groups);
  serializePropertyGroups(oss, manifest.property_groups);
  serializeHooks(oss, manifest.hooks);
  serializeTargets(oss, manifest.targets);
  serializeExports(oss, manifest.exports);
  serializeOutput(oss, manifest.output);

  return oss.str();
}

std::string TomlSerializer::escapeString(const std::string &s) {
  std::string result;
  for (char c : s) {
    switch (c) {
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\t':
      result += "\\t";
      break;
    case '\r':
      result += "\\r";
      break;
    default:
      result += c;
      break;
    }
  }
  return result;
}

std::string TomlSerializer::quoteString(const std::string &s) {
  return "\"" + escapeString(s) + "\"";
}

void TomlSerializer::writeArray(std::ostringstream &oss,
                                const std::vector<std::string> &items,
                                const std::string &indent) {
  if (items.empty()) {
    oss << "[]";
    return;
  }
  if (items.size() == 1) {
    oss << "[ " << quoteString(items[0]) << " ]";
    return;
  }
  oss << "[\n";
  for (size_t i = 0; i < items.size(); ++i) {
    oss << indent << "  " << quoteString(items[i]);
    if (i + 1 < items.size())
      oss << ",";
    oss << "\n";
  }
  oss << indent << "]";
}

void TomlSerializer::serializeProject(std::ostringstream &oss,
                                      const ProjectManifest &p) {
  oss << "# "
         "====================================================================="
         "==\n";
  oss << "# 1. ROOT PROJECT MANIFEST\n";
  oss << "# "
         "====================================================================="
         "==\n";
  oss << "[project]\n";
  oss << "name = " << quoteString(p.name) << "\n";

  if (p.version)
    oss << "version = " << quoteString(*p.version) << "\n";
  if (p.license)
    oss << "license = " << quoteString(*p.license) << "\n";
  if (p.description)
    oss << "description = " << quoteString(*p.description) << "\n";
  if (p.homepage)
    oss << "homepage = " << quoteString(*p.homepage) << "\n";
  if (p.default_compiler)
    oss << "default_compiler = " << quoteString(*p.default_compiler) << "\n";

  if (p.cpp_version)
    oss << "cpp_version = " << quoteString(*p.cpp_version) << "\n";

  if (p.version_from_file || p.version_from_pattern) {
    oss << "version_from = { file = " << quoteString(*p.version_from_file)
        << ", pattern = " << quoteString(*p.version_from_pattern) << " }\n";
  }

  if (!p.authors.empty()) {
    oss << "authors = ";
    writeArray(oss, p.authors);
    oss << "\n";
  }

  if (!p.include_dirs.empty()) {
    oss << "include_dirs = ";
    writeArray(oss, p.include_dirs);
    oss << "\n";
  }

  if (!p.dependencies.empty()) {
    oss << "dependencies = ";
    writeArray(oss, p.dependencies);
    oss << "\n";
  }
  oss << "\n";
}

void TomlSerializer::serializeOptions(
    std::ostringstream &oss,
    const std::unordered_map<std::string, bool> &opts) {
  if (opts.empty())
    return;

  oss << "# "
         "====================================================================="
         "==\n";
  oss << "# 2. GLOBAL OPTION TOGGLES\n";
  oss << "# "
         "====================================================================="
         "==\n";
  oss << "[options]\n";
  for (const auto &[name, value] : opts) {
    oss << name << " = " << (value ? "true" : "false") << "\n";
  }
  oss << "\n";
}

void TomlSerializer::serializeCompat(std::ostringstream &oss,
                                     const Compatibility &compat) {
  if (compat.compilers.supported.empty() ||
      compat.compilers.unsupported.empty() ||
      compat.unsupported_cpp_versions.empty() ||
      compat.min_cpp_version.empty() || compat.preferred_cpp_version->empty())
    return;
  oss << "# "
         "====================================================================="
         "==\n";
  oss << "# 3. COMPATIBILITY MATRIX\n";
  oss << "# "
         "====================================================================="
         "==\n";
  if (!(compat.min_cpp_version.empty() ||
        compat.preferred_cpp_version->empty() ||
        compat.unsupported_cpp_versions.empty())) {
    oss << "[compatibility]\n";
    oss << "min_cpp_version = " << quoteString(compat.min_cpp_version) << "\n";
    if (compat.preferred_cpp_version)
      oss << "preferred_cpp_version = "
          << quoteString(*compat.preferred_cpp_version) << "\n";
    if (!compat.unsupported_cpp_versions.empty()) {
      oss << "unsupported_cpp_versions = ";
      writeArray(oss, compat.unsupported_cpp_versions);
      oss << "\n";
    }
  };

  if (compat.compilers.supported.empty() &&
      compat.compilers.unsupported.empty())
    return;
  oss << "\n[compatibility.compilers]\n";
  if (compat.compilers.supported.size())
    oss << "supported = ";
  writeArray(oss, compat.compilers.supported);
  oss << "\n";
  if (compat.compilers.unsupported.size())
    oss << "unsupported = ";
  writeArray(oss, compat.compilers.unsupported);
  oss << "\n\n";
}

void TomlSerializer::serializeFileGroups(std::ostringstream &oss,
                                         const std::vector<FileGroup> &groups) {
  if (groups.empty())
    return;

  oss << "# "
         "====================================================================="
         "==\n";
  oss << "# 4. FILE GROUP ALIASING\n";
  oss << "# "
         "====================================================================="
         "==\n";
  for (const auto &grp : groups) {
    oss << "[[file_group]]\n";
    oss << "name = " << quoteString(grp.name) << "\n";
    oss << "patterns = ";
    writeArray(oss, grp.patterns);
    oss << "\n\n";
  }
}

void TomlSerializer::serializePropertyGroups(
    std::ostringstream &oss, const std::vector<PropertyGroup> &groups) {
  if (groups.empty())
    return;

  oss << "# "
         "====================================================================="
         "==\n";
  oss << "# 5. PROPERTY GROUPS\n";
  oss << "# "
         "====================================================================="
         "==\n";
  for (const auto &grp : groups) {
    oss << "[[property_group]]\n";
    oss << "name = " << quoteString(grp.name) << "\n";
    if (grp.condition)
      oss << "condition = " << quoteString(*grp.condition) << "\n";
    if (!grp.defines.empty()) {
      oss << "defines = ";
      writeArray(oss, grp.defines);
      oss << "\n";
    }
    if (!grp.flags.empty()) {
      oss << "flags = ";
      writeArray(oss, grp.flags);
      oss << "\n";
    }
    oss << "\n";
  }
}

void TomlSerializer::serializeHooks(std::ostringstream &oss,
                                    const std::vector<Hook> &hooks) {
  if (hooks.empty())
    return;

  oss << "# "
         "====================================================================="
         "==\n";
  oss << "# 6. LIFECYCLE INTERCEPTOR HOOKS\n";
  oss << "# "
         "====================================================================="
         "==\n";
  for (const auto &hk : hooks) {
    oss << "[[hook]]\n";
    oss << "name = " << quoteString(hk.name) << "\n";
    oss << "on = " << quoteString(hk.on) << "\n";
    oss << "run = " << quoteString(hk.run) << "\n";
    if (hk.target)
      oss << "target = " << quoteString(*hk.target) << "\n";
    if (hk.pattern)
      oss << "pattern = " << quoteString(*hk.pattern) << "\n";
    oss << "\n";
  }
}

void TomlSerializer::serializeTargets(
    std::ostringstream &oss,
    const std::unordered_map<std::string, Target> &targets) {
  if (targets.empty())
    return;

  oss << "# "
         "====================================================================="
         "==\n";
  oss << "# 7. TARGET EXECUTIONS MATRIX\n";
  oss << "# "
         "====================================================================="
         "==\n";

  // Sort targets by name for deterministic output
  std::vector<std::string> names;
  for (const auto &[name, _] : targets)
    names.push_back(name);
  std::sort(names.begin(), names.end());

  for (const auto &name : names) {
    auto it = targets.find(name);
    const Target &tgt = it->second;

    oss << "[target.\"" << name << "\"]\n";
    oss << "type = " << quoteString(tgt.type) << "\n";

    if (!tgt.sources.empty()) {
      oss << "sources = ";
      writeArray(oss, tgt.sources);
      oss << "\n";
    }
    if (!tgt.include_dirs.empty()) {
      oss << "include_dirs = ";
      writeArray(oss, tgt.include_dirs);
      oss << "\n";
    }
    if (!tgt.properties.empty()) {
      oss << "properties = ";
      writeArray(oss, tgt.properties);
      oss << "\n";
    }
    if (!tgt.flags.empty()) {
      oss << "flags = ";
      writeArray(oss, tgt.flags);
      oss << "\n";
    }
    if (!tgt.system_libs.empty()) {
      oss << "system_libs = ";
      writeArray(oss, tgt.system_libs);
      oss << "\n";
    }
    if (!tgt.depends_on.empty()) {
      oss << "depends_on = ";
      writeArray(oss, tgt.depends_on);
      oss << "\n";
    }

    // Added escaped (")s due to cmake's wierd toml incompatible structure
    for (const auto &si : tgt.sources_if) {
      oss << "[[target.\"" << name << "\".sources_if]]\n";
      oss << "condition = " << quoteString(si.condition) << "\n";
      oss << "patterns = ";
      writeArray(oss, si.patterns);
      oss << "\n";
    }

    for (const auto &fi : tgt.flags_if) {
      oss << "[[target.\"" << name << "\".flags_if]]\n";
      oss << "condition = " << quoteString(fi.condition) << "\n";
      oss << "flags = ";
      writeArray(oss, fi.flags);
      oss << "\n";
    }

    for (const auto &pi : tgt.properties_if) {
      oss << "[[target.\"" << name << "\".properties_if]]\n";
      oss << "condition = " << quoteString(pi.condition) << "\n";
      if (!pi.defines.empty()) {
        oss << "defines = ";
        writeArray(oss, pi.defines);
        oss << "\n";
      }
      if (!pi.depends_on.empty()) {
        oss << "depends_on = ";
        writeArray(oss, pi.depends_on);
        oss << "\n";
      }
    }

    oss << "\n";
  }
}

void TomlSerializer::serializeExports(std::ostringstream &oss,
                                      const Exports &exports) {
  if (exports.default_targets.size()) {
    oss << "# "
           "==================================================================="
           "=="
           "==\n";
    oss << "# 8. INTERFACE DISTRIBUTION LAYER\n";
    oss << "# "
           "==================================================================="
           "=="
           "==\n";
    oss << "[exports]\n";

    if (!exports.default_targets.empty()) {
      oss << "default_targets = ";
      writeArray(oss, exports.default_targets);
      oss << "\n";
    }

    if (!exports.include_dirs.empty()) {
      oss << "include_dirs = ";
      writeArray(oss, exports.include_dirs);
      oss << "\n";
    }

    if (!exports.defines_required.empty()) {
      oss << "defines_required = ";
      writeArray(oss, exports.defines_required);
      oss << "\n";
    }

    if (!exports.defines_optional.empty()) {
      oss << "defines_optional = ";
      writeArray(oss, exports.defines_optional);
      oss << "\n";
    }

    for (const auto &prof : exports.profiles) {
      oss << "[exports.profile.\"" << prof.name << "\"]\n";
      oss << "targets = ";
      writeArray(oss, prof.targets);
      oss << "\n\n";
    }
  }
}

void TomlSerializer::serializeOutput(std::ostringstream &oss,
                                     const Output &output) {
  if (output.directory.length()) {
    oss << "# "
           "==================================================================="
           "=="
           "==\n";
    oss << "# 9. ARTIFACT GENERATION LAYOUT\n";
    oss << "# "
           "==================================================================="
           "=="
           "==\n";
    oss << "[output]\n";
    oss << "directory = " << quoteString(output.directory) << "\n\n";

    for (const auto &[name, cfg] : output.configs) {
      oss << "[output.configs.\"" << name << "\"]\n";
      oss << "enabled = " << (cfg.enabled ? "true" : "false") << "\n";
      oss << "subdir = " << quoteString(cfg.subdir) << "\n\n";
    }
  }
}

} // namespace transpiler::emitter
