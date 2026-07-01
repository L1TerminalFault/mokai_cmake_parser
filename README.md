# CMake to Mokai Build System Transpiler

## Project in its early stage please **DO NOT** use for production purposes

## Build: use `mokai build` (obviously)

## Architecture (main)

### Parser CMake API 

- If the project builds with `cmake` the tool will ask cmake itself and build reliable structure for mokai

- If it doesn't build it will fallback to the Static Transpiler fallback method.

## Architecture (fallback)

### Lexer (`src/lexer/`)

- **token.hpp**: Token kinds, SourceLocation tracking
- **lexer.cpp**: Full CMake tokenizer handling quoted strings, bracket strings, variable references, generator expressions, comments, line continuations

### Parser (`src/parser/`)

- **parser.hpp/cpp**: Recursive descent parser producing an Abstract Syntax Tree (AST)
  - Collapses control-flow constructs (`if/elseif/else/endif`, `foreach/endforeach`, `while/endwhile`, `function`, `macro`) into tree nodes
  - Decomposes arguments into composite parts (literals + embedded `${VAR}` references)
  - Error recovery via synchronization

### AST (`src/ast/`)

- **node.hpp**:
  - `CommandNode` — plain CMake commands
  - `IfBlockNode`, `ForeachNode`, `WhileNode` — control-flow structures
  - `FunctionDefNode`, `MacroDefNode` — user-defined callables
  - `FileNode` — root of a parsed file
  - `IVisitor` interface — clean add-a-pass architecture

### Analyzer (`src/analyzer/`)

- **scope.cpp**: Variable scope stack (function frames, cache variables, environment)
- **expander.cpp**: Argument expansion and CMake list handling
- **condition_mapper.cpp**: Translates CMake `if()` conditions to mokai condition strings
  - Maps OS predicates: `WIN32` → `os == windows`, `UNIX` → `(os == linux || os == macos)`
  - Maps compiler predicates: `MSVC` → `compiler == msvc`, `CMAKE_COMPILER_IS_GNUCXX` → `compiler == gcc`
  - Handles version checks, build types, C++ standards
- **tree_merger.cpp**: Follows `include()` and `add_subdirectory()`, inline-merges referenced files
- **analyzer.cpp**: Orchestrates all passes, walks the AST, builds an intermediate `AnalyzerResult` with resolved targets, options, and options

### Emitter (`src/emitter/`)

- **manifest.hpp**: C++ data structures mirroring the mokai TOML schema
- **emitter.cpp**: Transforms `AnalyzerResult` into `MokaiManifest`
- **toml_serializer.cpp**: Serializes `MokaiManifest` to valid TOML output

### Diagnostics (`src/diagnostics/`)

- **diagnostic.hpp/cpp**: Unified diagnostic reporting (notes, warnings, errors, fatals)
  - Clang-style output formatting
  - Shared across all pipeline stages

## Key Design Decisions

### 1. **Full Compiler Architecture**

- Rather than regex/line-based parsing, uses a proper lexer → parser → AST pipeline
- Enables handling of nested constructs, variable resolution, and complex conditions
- Future passes (validation, optimization) can be added as new `IVisitor` implementations

### 2. **AST Visitor Pattern**

- Every analysis/emission pass implements `IVisitor`
- Adding a new pass doesn't require touching the node types
- Clean separation of concerns

### 3. **Expandable Condition Mapping**

- CMake condition logic isolated in `condition_mapper.cpp`
- Heuristics for mapping CMake predicates are documented and easy to extend
- Unknown conditions emit warnings but don't fail the entire conversion

### 4. **Multi-File Support via TreeMerger**

- `include()` and `add_subdirectory()` are **inline-merged** into the AST
- Cycle detection prevents infinite includes
- `CMAKE_CURRENT_SOURCE_DIR` tracking approximates scope semantics

### 5. **Robust Path Handling**

- All paths resolved relative to the containing file's directory
- Graceful degradation if filesystem operations fail
- Works with both absolute and relative paths

## Supported CMake Constructs

### Commands

- `project()` — name, version, languages
- `set()` / `option()` — variables and cache variables (mapped to mokai options)
- `add_executable()`, `add_library()` — target definitions
- `target_sources()`, `target_include_directories()`, `target_link_libraries()` — target properties
- `target_compile_options()`, `target_compile_definitions()` — compilation flags/defines
- `cmake_minimum_required()` — baseline version check
- `find_package()` — dependency tracking
- User-defined `function()` and `macro()` definitions

### Control Flow

- `if/elseif/else/endif` with condition mapping to mokai format
- `foreach/endforeach` — loop unrolling for static analysis
- `while/endwhile` — loop body analyzed once (static approximation)

### Variables

- Normal scope variables: `${VAR}`
- Environment variables: `$ENV{VAR}`
- Cache variables: `$CACHE{VAR}`
- Generator expressions: `$<...>` (preserved as-is for runtime evaluation)

## Output

The converter produces a mokai manifest TOML with:

- **[project]**: name, version, C++ standard, include directories, dependencies
- **[options]**: boolean option toggles extracted from `option()` and `set(...CACHE BOOL...)`
- **[compatibility]**: min/preferred C++ versions, supported compilers
- **[target.NAME]**: executable/library definitions with sources, include dirs, flags, defines, dependencies
  - **[[target.NAME.sources_if]]** / **[[target.NAME.flags_if]]** — conditional per-configuration variants
- **[exports]**: public include dirs, required/optional defines, export profiles
- **[output]**: build artifact layout (debug/release configs)

## Example

### Input: `CMakeLists.txt`

```cmake
project(MyApp VERSION 1.0)
set(CMAKE_CXX_STANDARD 20)
option(BUILD_TESTS "Build tests" ON)

add_executable(myapp src/main.cpp src/core.cpp)
target_include_directories(myapp PRIVATE src/include)
target_link_libraries(myapp PRIVATE pthread)
```

### Output: `output.toml`

```toml
[project]
name = "MyApp"
version = "1.0"
cpp_version = "c++20"

[options]
BUILD_TESTS = true

[target.myapp]
type = "executable"
sources = [ "src/main.cpp", "src/core.cpp" ]
include_dirs = [ "src/include" ]
system_libs = [ "pthread" ]

[exports]
default_targets = [ "myapp" ]
```

<!-- ## Testing

All pipeline stages are tested (lexer, parser, analyzer stubs):
```bash
g++ -std=c++20 -I src src/*.cpp src/*/*.cpp tests/test_lexer.cpp -o test_lexer
g++ -std=c++20 -I src src/*.cpp src/*/*.cpp tests/test_parser.cpp -o test_parser
./test_lexer
./test_parser
``` -->

## Limitations & Future Work

1. **Generator Expressions**: Preserved as-is; not evaluated (runtime-only)
2. **While Loops**: Analyzed once (static approximation, not a true loop)
3. **Macro Expansion**: Registered but not expanded during analysis (future work)
4. **CMake Scripting**: No support for complex CMake procedural code (beyond target/property definitions)
5. **Platform-Specific Logic**: Best-effort condition mapping; some corner cases emit warnings
