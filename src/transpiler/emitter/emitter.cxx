#include "emitter.hxx"
#include <filesystem>

namespace transpiler::emitter {
namespace fs = std::filesystem;

Emitter::Emitter(DiagnosticReporter &reporter, const std::string &sourceDir,
                 const std::string &outputDir)
    : reporter_(reporter), sourceDir_(sourceDir), outputDir_(outputDir) {
  // Initialize manifest defaults
  manifest_.project.name = "unknown";

  // Commented out cuz mokai is about to deprecate it
  // manifest_.project.cpp_version = "c++23";
  manifest_.project.include_dirs.clear();
  manifest_.project.dependencies.clear();

  // manifest_.compatibility.min_cpp_version = "c++20";
  // manifest_.compatibility.compilers.supported = {"clang", "gcc"};
  // manifest_.compatibility.compilers.unsupported = {"msvc"};

  // manifest_.output.directory = "./build_output";
  // manifest_.output.configs["debug"].enabled = true;
  // manifest_.output.configs["debug"].subdir = "bin/debug";
  // manifest_.output.configs["release"].enabled = true;
  // manifest_.output.configs["release"].subdir = "bin/release";
}

std::string Emitter::makeRelative(const std::string &absolutePath) const {
  if (absolutePath.empty())
    return "";
  if (outputDir_.empty())
    return absolutePath;

  try {
    auto abs = fs::path(absolutePath).lexically_normal();
    auto base = fs::path(outputDir_).lexically_normal();
    // Try to compute relative path
    auto rel = fs::relative(abs, base);
    return rel.string();
  } catch (...) {
    return absolutePath; // fallback to absolute if relative fails
  }
}

// MokaiManifest Emitter::emit(const AnalyzerResult &result) {
//   emitProject(result.project);
//   // emitOptions(result.options);
//   emitTargets(result.targets);
//
//   // Global include dirs
//   manifest_.project.include_dirs = result.globalIncludeDirs;
//
//   // Global dependencies
//   manifest_.project.dependencies = result.globalDependencies;
//
//   return std::move(manifest_);
// }

MokaiManifest Emitter::emit(const AnalyzerResult &result) {
  // MokaiManifest manifest;

  // [project]
  // manifest.project.name = result.project.name;
  // if (!result.project.version.empty())
  //     manifest.project.version = result.project.version;
  // if (!result.project.cppStandard.empty())
  //     manifest.project.cpp_version = result.project.cppStandard;
  //
  emitProject(result.project);
  // emitOptions(result.options);
  emitTargets(result.targets);

  // Global include dirs as relative paths
  for (const auto &dir : result.globalIncludeDirs) {
    manifest_.project.include_dirs.push_back(makeRelative(dir));
  }

  // [options]
  for (const auto &opt : result.options) {
    manifest_.options[opt.name] = opt.defaultValue;
  }

  // Global dependencies
  manifest_.project.dependencies = result.globalDependencies;

  // [target.NAME]
  // for (const auto &rtgt : result.targets) {
  //   Target tgt;
  //   tgt.name = rtgt.name;
  //   tgt.type = rtgt.type;
  //
  //   // Convert absolute paths to relative
  //   for (const auto &src : rtgt.sources)
  //     tgt.sources.push_back(makeRelative(src));
  //   for (const auto &inc : rtgt.includeDirs)
  //     tgt.include_dirs.push_back(makeRelative(inc));
  //
  //   tgt.flags = rtgt.flags;
  //   tgt.depends_on = rtgt.dependsOn;
  //   tgt.properties = rtgt.properties;
  //
  //   // sources_if
  //   for (const auto &si : rtgt.sourcesIf) {
  //     if (!si.condition.empty()) {
  //       TargetSourcesIf tsi;
  //       tsi.condition = si.condition;
  //       for (const auto &p : si.patterns)
  //         tsi.patterns.push_back(makeRelative(p));
  //       tgt.sources_if.push_back(tsi);
  //     }
  //   }

  // flags_if
  // for (const auto &fi : rtgt.flagsIf) {
  //   if (!fi.condition.empty()) {
  //     TargetFlagsIf tfi;
  //     tfi.condition = fi.condition;
  //     tfi.flags = fi.flags;
  //     tgt.flags_if.push_back(tfi);
  //   }
  // }
  //
  // properties_if (defines + depends_on)
  // for (const auto &di : rtgt.definesIf) {
  //   if (!di.condition.empty()) {
  //     TargetPropertiesIf tpi;
  //     tpi.condition = di.condition;
  //     tpi.defines = di.defines;
  //     tgt.properties_if.push_back(tpi);
  //   }
  // }
  // for (const auto &dpi : rtgt.dependsIf) {
  //   if (!dpi.condition.empty()) {
  //     TargetPropertiesIf tpi;
  //     tpi.condition = dpi.condition;
  //     tpi.depends_on = dpi.deps;
  //     tgt.properties_if.push_back(tpi);
  //   }
  // }

  // manifest_.targets[tgt.name] = std::move(tgt);
  // }

  // [exports]
  // for (const auto &tgt : result.targets) {
  //   if (tgt.type == "executable") {
  //     manifest_.exports.default_targets.push_back(tgt.name);
  //     break; // Just the first executable
  //   }
  // }
  //
  for (const auto &inc : result.globalIncludeDirs)
    manifest_.exports.include_dirs.push_back(makeRelative(inc));

  // return manifest;
  return std::move(manifest_);
}

void Emitter::emitProject(const ResolvedProject &proj) {
  manifest_.project.name = proj.name;
  if (!proj.version.empty())
    manifest_.project.version = proj.version;
  if (!proj.cppStandard.empty())
    manifest_.project.cpp_version = proj.cppStandard;
}

void Emitter::emitOptions(const std::vector<ResolvedOption> &opts) {
  for (const auto &opt : opts) {
    manifest_.options[opt.name] = opt.defaultValue;
  }
}

void Emitter::emitTargets(const std::vector<ResolvedTarget> &tgts) {
  for (const auto &rtgt : tgts) {
    Target tgt;
    tgt.name = rtgt.name;
    tgt.type = rtgt.type;

    // tgt.sources = rtgt.sources;
    // tgt.include_dirs = rtgt.includeDirs;

    // Convert absolute paths to relative
    for (const auto &src : rtgt.sources)
      tgt.sources.push_back(makeRelative(src));
    for (const auto &inc : rtgt.includeDirs)
      tgt.include_dirs.push_back(makeRelative(inc));

    tgt.flags = rtgt.flags;
    tgt.system_libs = rtgt.systemLibs;
    tgt.depends_on = rtgt.dependsOn;
    tgt.properties = rtgt.properties;

    // Convert conditional sources
    for (const auto &cond : rtgt.sourcesIf) {
      if (!cond.condition.empty()) {
        TargetSourcesIf si;
        si.condition = cond.condition;
        // si.patterns = cond.patterns;
        for (const auto &p : cond.patterns)
          si.patterns.push_back(makeRelative(p));
        tgt.sources_if.push_back(si);
      }
    }

    // Convert conditional flags
    for (const auto &cond : rtgt.flagsIf) {
      if (!cond.condition.empty()) {
        TargetFlagsIf fi;
        fi.condition = cond.condition;
        fi.flags = cond.flags;
        tgt.flags_if.push_back(fi);
      }
    }

    // Convert conditional defines
    for (const auto &cond : rtgt.definesIf) {
      if (!cond.condition.empty()) {
        TargetPropertiesIf pi;
        pi.condition = cond.condition;
        pi.defines = cond.defines;
        tgt.properties_if.push_back(pi);
      }
    }

    // Convert conditional depends
    for (const auto &cond : rtgt.dependsIf) {
      if (!cond.condition.empty()) {
        TargetPropertiesIf pi;
        pi.condition = cond.condition;
        pi.depends_on = cond.deps;
        tgt.properties_if.push_back(pi);
      }
    }

    manifest_.targets[tgt.name] = std::move(tgt);
  }

  // Set default_targets to the first executable found
  for (const auto &rtgt : tgts) {
    if (rtgt.type == "static_library" || rtgt.type == "dynamic_library") {
      manifest_.exports.default_targets.push_back(rtgt.name);
      break;
    }
  }
}

} // namespace transpiler::emitter
