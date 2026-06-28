#include "emitter.hxx"

namespace transpiler::emitter {

Emitter::Emitter(DiagnosticReporter &reporter, const std::string &sourceDir)
    : reporter_(reporter), sourceDir_(sourceDir) {
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

MokaiManifest Emitter::emit(const AnalyzerResult &result) {
  emitProject(result.project);
  // emitOptions(result.options);
  emitTargets(result.targets);

  // Global include dirs
  manifest_.project.include_dirs = result.globalIncludeDirs;

  // Global dependencies
  manifest_.project.dependencies = result.globalDependencies;

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
    tgt.sources = rtgt.sources;
    tgt.include_dirs = rtgt.includeDirs;
    tgt.flags = rtgt.flags;
    tgt.system_libs = rtgt.systemLibs;
    tgt.depends_on = rtgt.dependsOn;
    tgt.properties = rtgt.properties;

    // Convert conditional sources
    for (const auto &cond : rtgt.sourcesIf) {
      if (!cond.condition.empty()) {
        TargetSourcesIf si;
        si.condition = cond.condition;
        si.patterns = cond.patterns;
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
  // for (const auto &rtgt : tgts) {
  //   if (rtgt.type == "executable") {
  //     manifest_.exports.default_targets.push_back(rtgt.name);
  //     break;
  //   }
  // }
}

} // namespace transpiler::emitter
