# Mermaid Implementation Tasks

This document breaks down the PRD into manageable implementation tasks with detailed unit and acceptance tests.

---

## Overview

| Phase | Tasks | Estimated LOC | Dependencies |
|-------|-------|---------------|--------------|
| 1. Core Infrastructure | 8 | ~800 | None |
| 2. Inline Commands | 10 | ~600 | Phase 1 |
| 3. Mermaid File Command | 6 | ~300 | Phase 2 |
| 4. Auto-Generated Diagrams | 12 | ~2000 | Phase 1 |
| 5. Client-Side & Raw Output | 6 | ~400 | Phase 2, 4 |

---

# Phase 1: Core Infrastructure

## Task 1.1: Add Mermaid Configuration Options to config.xml

**File**: `src/config.xml`

**Description**: Add all MERMAID_* configuration options to the configuration schema.

**Changes**:
```xml
<!-- Add after PLANTUML section, around line 3800 -->
<option type='string' id='MERMAID_EXECUTABLE' format='file' defval=''>
<option type='string' id='MERMAID_CONFIG_FILE' format='file' defval=''>
<option type='enum' id='MERMAID_FORMAT' defval='svg'>
<option type='enum' id='MERMAID_THEME' defval='default'>
<option type='list' id='MERMAIDFILE_DIRS' format='dir'>
<option type='bool' id='USE_MERMAID' defval='0'>
<option type='bool' id='MERMAID_CLIENT_SIDE' defval='0' depends='USE_MERMAID'>
<option type='bool' id='MERMAID_OUTPUT_RAW' defval='0'>
```

### Unit Tests

**Test 1.1.1: Config option parsing**
```
Test: mermaid_config_parse
Input Doxyfile:
  MERMAID_EXECUTABLE = /usr/bin/mmdc
  MERMAID_FORMAT = svg
  MERMAID_THEME = dark
  USE_MERMAID = YES

Expected:
  - Config_getString(MERMAID_EXECUTABLE) == "/usr/bin/mmdc"
  - Config_getEnum(MERMAID_FORMAT) == "svg"
  - Config_getEnum(MERMAID_THEME) == "dark"
  - Config_getBool(USE_MERMAID) == true
```

**Test 1.1.2: Config defaults**
```
Test: mermaid_config_defaults
Input Doxyfile: (empty - no mermaid options)

Expected:
  - Config_getString(MERMAID_EXECUTABLE) == ""
  - Config_getEnum(MERMAID_FORMAT) == "svg"
  - Config_getEnum(MERMAID_THEME) == "default"
  - Config_getBool(USE_MERMAID) == false
  - Config_getBool(MERMAID_CLIENT_SIDE) == false
  - Config_getBool(MERMAID_OUTPUT_RAW) == false
```

**Test 1.1.3: Config enum validation**
```
Test: mermaid_config_invalid_enum
Input Doxyfile:
  MERMAID_FORMAT = invalid_format

Expected:
  - Warning: "argument 'invalid_format' for option MERMAID_FORMAT is not a valid enum value"
  - Falls back to default "svg"
```

**Test 1.1.4: Dependency validation**
```
Test: mermaid_config_dependency
Input Doxyfile:
  USE_MERMAID = NO
  MERMAID_CLIENT_SIDE = YES

Expected:
  - MERMAID_CLIENT_SIDE is ignored (depends on USE_MERMAID)
  - No error, but option has no effect
```

### Acceptance Criteria
- [ ] All 8 config options appear in generated Doxyfile template
- [ ] `doxygen -g` generates valid config with mermaid options
- [ ] Invalid enum values produce warnings
- [ ] Dependency chain (MERMAID_CLIENT_SIDE depends on USE_MERMAID) works

---

## Task 1.2: Validate MERMAID_EXECUTABLE Path

**File**: `src/configimpl.l`

**Description**: Add validation logic for MERMAID_EXECUTABLE path, similar to DOT_PATH validation.

**Changes**: Add validation function around line 1800 (near `checkDotPath`)

```cpp
static void checkMermaidPath()
{
  QCString mermaidExe = Config_getString(MERMAID_EXECUTABLE);
  if (mermaidExe.isEmpty()) {
    // Try to find mmdc in PATH
    mermaidExe = Portable::findExe("mmdc");
    if (!mermaidExe.isEmpty()) {
      Config_updateString(MERMAID_EXECUTABLE, mermaidExe);
    }
  }
  if (!mermaidExe.isEmpty() && !Portable::fileExists(mermaidExe)) {
    ConfigImpl::config_warn("MERMAID_EXECUTABLE '%s' does not exist\n",
                           qPrint(mermaidExe));
  }
}
```

### Unit Tests

**Test 1.2.1: Valid executable path**
```
Test: mermaid_exe_valid_path
Setup: Create mock /tmp/test_mmdc executable
Input Doxyfile:
  MERMAID_EXECUTABLE = /tmp/test_mmdc

Expected:
  - No warnings
  - Config_getString(MERMAID_EXECUTABLE) == "/tmp/test_mmdc"
```

**Test 1.2.2: Invalid executable path**
```
Test: mermaid_exe_invalid_path
Input Doxyfile:
  MERMAID_EXECUTABLE = /nonexistent/path/mmdc

Expected:
  - Warning: "MERMAID_EXECUTABLE '/nonexistent/path/mmdc' does not exist"
```

**Test 1.2.3: Auto-detect mmdc in PATH**
```
Test: mermaid_exe_autodetect
Setup: Ensure mmdc is in PATH (or mock Portable::findExe)
Input Doxyfile:
  MERMAID_EXECUTABLE = (empty)

Expected:
  - If mmdc found: Config populated with path
  - If mmdc not found: Config remains empty, no error
```

### Acceptance Criteria
- [ ] Valid paths are accepted without warnings
- [ ] Invalid paths produce clear warning messages
- [ ] Auto-detection of mmdc in PATH works when MERMAID_EXECUTABLE is empty
- [ ] File permissions are not checked (only existence)

---

## Task 1.3: Create MermaidManager Header

**File**: `src/mermaid.h` (new file)

**Description**: Define MermaidManager singleton class interface.

```cpp
#ifndef MERMAID_H
#define MERMAID_H

#include <map>
#include <string>
#include <vector>
#include "qcstring.h"
#include "containers.h"

class MermaidManager
{
  public:
    enum OutputFormat { MMD_SVG, MMD_PNG, MMD_PDF };

    static MermaidManager &instance();

    // Insert mermaid source, returns base filename for output
    QCString insert(const QCString &content,
                    const QCString &baseName,
                    const QCString &outDir,
                    OutputFormat format,
                    const QCString &srcFile,
                    int srcLine);

    // Process all pending diagrams
    void run();

    // Get output filename for a given base
    QCString getOutputFile(const QCString &baseName, OutputFormat format) const;

  private:
    MermaidManager() = default;
    ~MermaidManager() = default;

    struct DiagramInfo {
      QCString content;
      QCString outDir;
      OutputFormat format;
      QCString srcFile;
      int srcLine;
      QCString md5Hash;
    };

    using DiagramMap = std::map<std::string, DiagramInfo>;

    DiagramMap m_diagrams;
    bool m_hasRun = false;

    QCString computeMd5(const QCString &content) const;
    bool needsRegeneration(const QCString &baseName,
                           const QCString &md5Hash,
                           OutputFormat format) const;
    void executeMmdc(const QCString &inputFile,
                     const QCString &outputFile,
                     OutputFormat format);
};

#endif // MERMAID_H
```

### Unit Tests

**Test 1.3.1: Singleton instance**
```
Test: mermaid_manager_singleton
Code:
  MermaidManager &m1 = MermaidManager::instance();
  MermaidManager &m2 = MermaidManager::instance();

Expected:
  - &m1 == &m2 (same instance)
```

### Acceptance Criteria
- [ ] Header compiles without errors
- [ ] Singleton pattern prevents multiple instances
- [ ] All public methods declared
- [ ] Include guards present

---

## Task 1.4: Implement MermaidManager Core

**File**: `src/mermaid.cpp` (new file)

**Description**: Implement MermaidManager methods.

### Unit Tests

**Test 1.4.1: Content insertion**
```
Test: mermaid_manager_insert
Code:
  MermaidManager &mgr = MermaidManager::instance();
  QCString base = mgr.insert("graph TD\n  A-->B", "test_diagram",
                             "/tmp/out", MMD_SVG, "test.cpp", 10);

Expected:
  - base is non-empty (e.g., "test_diagram" or md5-based name)
  - Diagram stored in internal map
```

**Test 1.4.2: MD5 hash computation**
```
Test: mermaid_manager_md5
Code:
  // Same content should produce same hash
  QCString content = "graph TD\n  A-->B";
  QCString hash1 = mgr.computeMd5(content);
  QCString hash2 = mgr.computeMd5(content);
  QCString hash3 = mgr.computeMd5("graph TD\n  A-->C");

Expected:
  - hash1 == hash2
  - hash1 != hash3
  - hash1.length() == 32 (MD5 hex string)
```

**Test 1.4.3: Regeneration check**
```
Test: mermaid_manager_cache
Setup:
  - Create /tmp/out/test.svg with known content
  - Create /tmp/out/test.md5 with matching hash

Code:
  bool needs = mgr.needsRegeneration("test", hash, MMD_SVG);

Expected:
  - needs == false (cached version is valid)
  - Modify content hash -> needs == true
```

**Test 1.4.4: Multiple format support**
```
Test: mermaid_manager_formats
Code:
  QCString svg = mgr.getOutputFile("test", MMD_SVG);
  QCString png = mgr.getOutputFile("test", MMD_PNG);
  QCString pdf = mgr.getOutputFile("test", MMD_PDF);

Expected:
  - svg ends with ".svg"
  - png ends with ".png"
  - pdf ends with ".pdf"
```

### Acceptance Criteria
- [ ] Content insertion stores diagram info
- [ ] MD5 hashing is deterministic
- [ ] Cache checking prevents unnecessary regeneration
- [ ] All three output formats supported

---

## Task 1.5: Implement mmdc CLI Execution

**File**: `src/mermaid.cpp`

**Description**: Implement `executeMmdc()` and `run()` methods.

```cpp
void MermaidManager::executeMmdc(const QCString &inputFile,
                                  const QCString &outputFile,
                                  OutputFormat format)
{
  QCString mermaidExe = Config_getString(MERMAID_EXECUTABLE);
  if (mermaidExe.isEmpty()) {
    err("MERMAID_EXECUTABLE not configured\n");
    return;
  }

  QCString formatStr;
  switch (format) {
    case MMD_SVG: formatStr = "svg"; break;
    case MMD_PNG: formatStr = "png"; break;
    case MMD_PDF: formatStr = "pdf"; break;
  }

  QCString theme = Config_getEnum(MERMAID_THEME);
  QCString configFile = Config_getString(MERMAID_CONFIG_FILE);

  QCString args;
  args.sprintf("-i \"%s\" -o \"%s\" -e %s -t %s",
               qPrint(inputFile), qPrint(outputFile),
               qPrint(formatStr), qPrint(theme));

  if (!configFile.isEmpty()) {
    args += " -c \"" + configFile + "\"";
  }

  int exitCode = Portable::system(mermaidExe, args, false);
  if (exitCode != 0) {
    err("Mermaid CLI failed for %s (exit code %d)\n",
        qPrint(inputFile), exitCode);
  }
}
```

### Unit Tests

**Test 1.5.1: Successful execution (mock)**
```
Test: mermaid_execute_success
Setup:
  - Mock Portable::system to return 0
  - Set MERMAID_EXECUTABLE = "/usr/bin/mmdc"

Code:
  mgr.executeMmdc("/tmp/in.mmd", "/tmp/out.svg", MMD_SVG);

Expected:
  - Portable::system called with correct arguments
  - No error messages
```

**Test 1.5.2: Execution failure**
```
Test: mermaid_execute_failure
Setup:
  - Mock Portable::system to return 1

Code:
  mgr.executeMmdc("/tmp/in.mmd", "/tmp/out.svg", MMD_SVG);

Expected:
  - Error message: "Mermaid CLI failed for /tmp/in.mmd (exit code 1)"
```

**Test 1.5.3: Missing executable**
```
Test: mermaid_execute_no_exe
Setup:
  - Set MERMAID_EXECUTABLE = ""

Code:
  mgr.executeMmdc("/tmp/in.mmd", "/tmp/out.svg", MMD_SVG);

Expected:
  - Error message: "MERMAID_EXECUTABLE not configured"
  - Portable::system NOT called
```

**Test 1.5.4: Config file inclusion**
```
Test: mermaid_execute_with_config
Setup:
  - Set MERMAID_CONFIG_FILE = "/path/to/config.json"
  - Mock Portable::system

Code:
  mgr.executeMmdc("/tmp/in.mmd", "/tmp/out.svg", MMD_SVG);

Expected:
  - Command includes: -c "/path/to/config.json"
```

**Test 1.5.5: Theme option**
```
Test: mermaid_execute_theme
Setup:
  - Set MERMAID_THEME = "dark"
  - Mock Portable::system

Code:
  mgr.executeMmdc("/tmp/in.mmd", "/tmp/out.svg", MMD_SVG);

Expected:
  - Command includes: -t dark
```

### Acceptance Criteria
- [ ] Correct command line arguments constructed
- [ ] Exit codes properly handled
- [ ] Error messages are clear and actionable
- [ ] All config options (theme, config file) passed to mmdc
- [ ] Path quoting handles spaces correctly

---

## Task 1.6: Implement Batch Processing (run method)

**File**: `src/mermaid.cpp`

**Description**: Implement `run()` to process all pending diagrams.

```cpp
void MermaidManager::run()
{
  if (m_hasRun) return;
  m_hasRun = true;

  if (m_diagrams.empty()) return;

  msg("Generating Mermaid diagrams...\n");

  for (const auto &[baseName, info] : m_diagrams) {
    if (needsRegeneration(baseName, info.md5Hash, info.format)) {
      // Write .mmd file
      QCString inputFile = info.outDir + "/" + baseName + ".mmd";
      std::ofstream f(inputFile.str());
      f << info.content.str();
      f.close();

      // Generate output
      QCString outputFile = getOutputFile(baseName, info.format);
      executeMmdc(inputFile, info.outDir + "/" + outputFile, info.format);

      // Write MD5 cache file
      QCString md5File = info.outDir + "/" + baseName + ".md5";
      std::ofstream m(md5File.str());
      m << info.md5Hash.str();
      m.close();

      // Cleanup .mmd if DOT_CLEANUP is set
      if (Config_getBool(DOT_CLEANUP)) {
        Portable::unlink(inputFile);
      }
    }
  }
}
```

### Unit Tests

**Test 1.6.1: Empty diagram list**
```
Test: mermaid_run_empty
Code:
  MermaidManager &mgr = MermaidManager::instance();
  mgr.run();

Expected:
  - No files created
  - No errors
  - Returns immediately
```

**Test 1.6.2: Single diagram processing**
```
Test: mermaid_run_single
Setup:
  - Configure valid MERMAID_EXECUTABLE
  - Insert one diagram

Code:
  mgr.insert("graph TD\n  A-->B", "single", "/tmp/out", MMD_SVG, "test.cpp", 1);
  mgr.run();

Expected:
  - /tmp/out/single.mmd created (or cleaned up)
  - /tmp/out/single.svg created
  - /tmp/out/single.md5 created
```

**Test 1.6.3: Multiple diagrams**
```
Test: mermaid_run_multiple
Setup:
  - Insert 3 diagrams with different names

Code:
  mgr.insert("graph TD\n  A-->B", "d1", "/tmp/out", MMD_SVG, "a.cpp", 1);
  mgr.insert("graph LR\n  C-->D", "d2", "/tmp/out", MMD_PNG, "b.cpp", 5);
  mgr.insert("pie\n  a:1\n  b:2", "d3", "/tmp/out", MMD_SVG, "c.cpp", 10);
  mgr.run();

Expected:
  - All 3 output files created
  - Each has correct format (d1.svg, d2.png, d3.svg)
```

**Test 1.6.4: Caching prevents regeneration**
```
Test: mermaid_run_cached
Setup:
  - Insert diagram, run once
  - Insert same diagram again (same content, same name)

Code:
  mgr.insert("graph TD\n  A-->B", "cached", "/tmp/out", MMD_SVG, "test.cpp", 1);
  mgr.run();
  // Record file modification time
  auto mtime1 = getFileModTime("/tmp/out/cached.svg");

  // Clear and re-insert same content
  mgr.insert("graph TD\n  A-->B", "cached", "/tmp/out", MMD_SVG, "test.cpp", 1);
  mgr.run();
  auto mtime2 = getFileModTime("/tmp/out/cached.svg");

Expected:
  - mtime1 == mtime2 (file not regenerated)
  - mmdc NOT called second time
```

**Test 1.6.5: Idempotent run**
```
Test: mermaid_run_idempotent
Code:
  mgr.insert("graph TD\n  A-->B", "test", "/tmp/out", MMD_SVG, "test.cpp", 1);
  mgr.run();
  mgr.run(); // Second call
  mgr.run(); // Third call

Expected:
  - Only one generation occurs
  - m_hasRun flag prevents re-processing
```

### Acceptance Criteria
- [ ] All pending diagrams processed
- [ ] Caching prevents unnecessary regeneration
- [ ] Temporary .mmd files cleaned up when DOT_CLEANUP=YES
- [ ] MD5 files written for cache validation
- [ ] run() is idempotent

---

## Task 1.7: Add Debug Support

**File**: `src/debug.h`, `src/debug.cpp`

**Description**: Add Mermaid debug mask for diagnostic output.

```cpp
// In debug.h, add to enum:
Mermaid = 0x00080000,

// In debug.cpp, add to labelMap:
{ "mermaid", Debug::Mermaid },
```

### Unit Tests

**Test 1.7.1: Debug flag parsing**
```
Test: mermaid_debug_flag
Code:
  Debug::setFlag("mermaid");

Expected:
  - Debug::isFlagSet(Debug::Mermaid) == true
```

**Test 1.7.2: Debug output**
```
Test: mermaid_debug_output
Setup:
  - Enable Debug::Mermaid
  - Insert and run a diagram

Expected:
  - Debug output includes mermaid processing info
```

### Acceptance Criteria
- [ ] `-d mermaid` command line option works
- [ ] Debug output shows diagram processing details
- [ ] Does not affect normal operation when disabled

---

## Task 1.8: Integrate MermaidManager into Build

**Files**: `src/CMakeLists.txt`, `src/doxygen.cpp`

**Description**: Add mermaid.cpp to build and call MermaidManager::run() in main.

### Changes to CMakeLists.txt
```cmake
# Add to doxygen_SOURCES list:
mermaid.cpp
```

### Changes to doxygen.cpp
```cpp
// After PlantumlManager::instance().run() call (around line 13500):
MermaidManager::instance().run();
```

### Unit Tests

**Test 1.8.1: Build integration**
```
Test: mermaid_build
Command: cmake --build . --target doxygen

Expected:
  - Build succeeds
  - mermaid.o linked into doxygen executable
```

**Test 1.8.2: Runtime integration**
```
Test: mermaid_runtime
Setup:
  - Create test Doxyfile with MERMAID_EXECUTABLE configured
  - Create test source with @startmermaid

Command: doxygen Doxyfile

Expected:
  - MermaidManager::run() called during generation
  - Diagram output created
```

### Acceptance Criteria
- [ ] mermaid.cpp compiles and links
- [ ] MermaidManager::run() called at correct point in generation
- [ ] No runtime errors with empty mermaid config

---

# Phase 2: Inline Mermaid Commands

## Task 2.1: Register Mermaid Commands

**Files**: `src/cmdmapper.h`, `src/cmdmapper.cpp`

**Description**: Add CMD_STARTMERMAID and CMD_ENDMERMAID command IDs.

```cpp
// In cmdmapper.h, add to CommandType enum:
CMD_STARTMERMAID = 120,
CMD_ENDMERMAID   = 121,

// In cmdmapper.cpp, add to cmdMapper initialization:
{ "startmermaid", CMD_STARTMERMAID },
{ "endmermaid",   CMD_ENDMERMAID },
```

### Unit Tests

**Test 2.1.1: Command mapping**
```
Test: mermaid_cmd_map
Code:
  int cmd = Mappers::cmdMapper->map("startmermaid");

Expected:
  - cmd == CMD_STARTMERMAID
```

**Test 2.1.2: End command mapping**
```
Test: mermaid_cmd_end_map
Code:
  int cmd = Mappers::cmdMapper->map("endmermaid");

Expected:
  - cmd == CMD_ENDMERMAID
```

### Acceptance Criteria
- [ ] @startmermaid recognized as valid command
- [ ] @endmermaid recognized as valid command
- [ ] Command IDs don't conflict with existing commands

---

## Task 2.2: Extend DocVerbatim for Mermaid

**File**: `src/docnode.h`

**Description**: Add Mermaid type to DocVerbatim::Type enum.

```cpp
// In DocVerbatim class, extend Type enum:
enum Type {
  Code, HtmlOnly, ManOnly, LatexOnly, RtfOnly, XmlOnly,
  DocbookOnly, Dot, Msc, PlantUML,
  Mermaid  // <-- Add this
};
```

### Unit Tests

**Test 2.2.1: DocVerbatim Mermaid type**
```
Test: mermaid_docverbatim_type
Code:
  DocVerbatim node(nullptr, nullptr, "", "", DocVerbatim::Mermaid, false, "");

Expected:
  - node.type() == DocVerbatim::Mermaid
```

### Acceptance Criteria
- [ ] DocVerbatim::Mermaid type available
- [ ] Doesn't break existing DocVerbatim usage

---

## Task 2.3: Add Lexer States for Mermaid

**File**: `src/doctokenizer.l`

**Description**: Add lexer states for mermaid option and content parsing.

```lex
%x St_MermaidOpt
%x St_Mermaid

<St_Para,St_Title>"@startmermaid"/{BLANK}  {
  g_token->name = "mermaid";
  BEGIN(St_MermaidOpt);
  return TK_MERMAID_START;
}

<St_MermaidOpt>"{"[^}]*"}" {
  g_token->sectionId = QCString(yytext+1, yyleng-2);
  return TK_MERMAID_OPTION;
}

<St_MermaidOpt>{BLANK}*\n {
  BEGIN(St_Mermaid);
}

<St_Mermaid>"@endmermaid" {
  BEGIN(St_Para);
  return TK_MERMAID_END;
}

<St_Mermaid>.*\n {
  g_token->verb += yytext;
}
```

### Unit Tests

**Test 2.3.1: Basic tokenization**
```
Test: mermaid_tokenize_basic
Input:
  @startmermaid
  graph TD
    A-->B
  @endmermaid

Expected tokens:
  - TK_MERMAID_START
  - TK_MERMAID_END
  - token->verb == "graph TD\n  A-->B\n"
```

**Test 2.3.2: Tokenization with options**
```
Test: mermaid_tokenize_options
Input:
  @startmermaid{flowchart,"mydiagram"}
  graph TD
    A-->B
  @endmermaid

Expected tokens:
  - TK_MERMAID_START
  - TK_MERMAID_OPTION with sectionId="flowchart,\"mydiagram\""
  - TK_MERMAID_END
```

**Test 2.3.3: Unclosed block**
```
Test: mermaid_tokenize_unclosed
Input:
  @startmermaid
  graph TD
    A-->B
  (EOF without @endmermaid)

Expected:
  - Warning: "found @startmermaid without matching @endmermaid"
```

### Acceptance Criteria
- [ ] @startmermaid/@endmermaid tokens recognized
- [ ] Content between tags captured in token->verb
- [ ] Options in braces parsed correctly
- [ ] Unclosed blocks produce warnings

---

## Task 2.4: Parse Mermaid in Doc Parser

**File**: `src/docnode.cpp`

**Description**: Handle CMD_STARTMERMAID in document parser.

```cpp
// In DocPara::handleCommand(), add case:
case CMD_STARTMERMAID:
{
  parser()->tokenizer.setStateMermaid();
  int tok = parser()->tokenizer.lex();

  QCString content;
  QCString options;

  while (tok == TK_MERMAID_OPTION) {
    options = parser()->context.token->sectionId;
    tok = parser()->tokenizer.lex();
  }

  if (tok == TK_MERMAID_END) {
    content = parser()->context.token->verb;
  }

  children().append<DocVerbatim>(
    parser(), thisVariant(),
    parser()->context.context,
    content,
    DocVerbatim::Mermaid,
    parser()->context.isExample,
    parser()->context.exampleName,
    options
  );

  retval = Token::make_TK_NEWPARA();
}
break;
```

### Unit Tests

**Test 2.4.1: Basic parsing**
```
Test: mermaid_parse_basic
Input source:
  /**
   * @startmermaid
   * graph TD
   *     A-->B
   * @endmermaid
   */
  void func();

Expected:
  - DocVerbatim node created with type=Mermaid
  - Content = "graph TD\n    A-->B\n"
```

**Test 2.4.2: Parsing with caption**
```
Test: mermaid_parse_caption
Input:
  @startmermaid{,"Flow diagram"}
  graph TD
    A-->B
  @endmermaid

Expected:
  - DocVerbatim options contain caption
```

**Test 2.4.3: Empty mermaid block**
```
Test: mermaid_parse_empty
Input:
  @startmermaid
  @endmermaid

Expected:
  - Warning or empty DocVerbatim created
```

### Acceptance Criteria
- [ ] Mermaid blocks parsed into DocVerbatim nodes
- [ ] Content preserved with whitespace
- [ ] Options (type, caption) extracted
- [ ] Error handling for malformed blocks

---

## Task 2.5: HTML Output for Mermaid

**File**: `src/htmldocvisitor.cpp`

**Description**: Implement HTML rendering for DocVerbatim::Mermaid.

```cpp
void HtmlDocVisitor::operator()(const DocVerbatim &s)
{
  // ... existing cases ...

  case DocVerbatim::Mermaid:
  {
    QCString baseName = MermaidManager::instance().insert(
      s.text(),
      "", // auto-generate name
      m_ci.dir(),
      getMermaidFormat(),
      s.srcFile(),
      s.srcLine()
    );

    if (Config_getBool(MERMAID_CLIENT_SIDE)) {
      // Client-side rendering
      m_t << "<div class=\"mermaid\">\n";
      m_t << s.text();
      m_t << "</div>\n";
    } else {
      // Pre-rendered image
      QCString imgFile = MermaidManager::instance().getOutputFile(
        baseName, getMermaidFormat()
      );
      m_t << "<div class=\"mermaidgraph\">\n";
      m_t << "<img src=\"" << imgFile << "\" alt=\"Mermaid diagram\"/>\n";
      m_t << "</div>\n";
    }
  }
  break;
}
```

### Unit Tests

**Test 2.5.1: HTML image output**
```
Test: mermaid_html_image
Setup:
  - MERMAID_CLIENT_SIDE = NO
  - MERMAID_FORMAT = svg

Input:
  @startmermaid
  graph TD
    A-->B
  @endmermaid

Expected HTML:
  <div class="mermaidgraph">
  <img src="mermaid_abc123.svg" alt="Mermaid diagram"/>
  </div>
```

**Test 2.5.2: HTML client-side output**
```
Test: mermaid_html_clientside
Setup:
  - MERMAID_CLIENT_SIDE = YES

Input:
  @startmermaid
  graph TD
    A-->B
  @endmermaid

Expected HTML:
  <div class="mermaid">
  graph TD
    A-->B
  </div>
```

**Test 2.5.3: PNG format**
```
Test: mermaid_html_png
Setup:
  - MERMAID_FORMAT = png

Expected HTML:
  <img src="mermaid_abc123.png" .../>
```

### Acceptance Criteria
- [ ] SVG images embedded correctly
- [ ] PNG images embedded correctly
- [ ] Client-side mode outputs raw mermaid in div
- [ ] Proper HTML escaping applied
- [ ] Alt text provided for accessibility

---

## Task 2.6: LaTeX Output for Mermaid

**File**: `src/latexdocvisitor.cpp`

**Description**: Implement LaTeX rendering for DocVerbatim::Mermaid.

```cpp
void LatexDocVisitor::operator()(const DocVerbatim &s)
{
  // ... existing cases ...

  case DocVerbatim::Mermaid:
  {
    QCString baseName = MermaidManager::instance().insert(
      s.text(), "", m_ci.dir(),
      MermaidManager::MMD_PNG, // PNG for LaTeX
      s.srcFile(), s.srcLine()
    );

    QCString imgFile = MermaidManager::instance().getOutputFile(
      baseName, MermaidManager::MMD_PNG
    );

    m_t << "\\begin{figure}[H]\n";
    m_t << "\\centering\n";
    m_t << "\\includegraphics[width=0.8\\textwidth]{" << imgFile << "}\n";
    if (!s.caption().isEmpty()) {
      m_t << "\\caption{" << convertToLaTeX(s.caption()) << "}\n";
    }
    m_t << "\\end{figure}\n";
  }
  break;
}
```

### Unit Tests

**Test 2.6.1: LaTeX image inclusion**
```
Test: mermaid_latex_image
Input:
  @startmermaid
  graph TD
    A-->B
  @endmermaid

Expected LaTeX:
  \begin{figure}[H]
  \centering
  \includegraphics[width=0.8\textwidth]{mermaid_abc123.png}
  \end{figure}
```

**Test 2.6.2: LaTeX with caption**
```
Test: mermaid_latex_caption
Input:
  @startmermaid{,"My Diagram"}
  graph TD
    A-->B
  @endmermaid

Expected LaTeX:
  ...
  \caption{My Diagram}
  ...
```

### Acceptance Criteria
- [ ] PNG images used for LaTeX output
- [ ] Proper figure environment
- [ ] Captions rendered when provided
- [ ] Special LaTeX characters escaped

---

## Task 2.7: DocBook Output for Mermaid

**File**: `src/docbookvisitor.cpp`

**Description**: Implement DocBook rendering for DocVerbatim::Mermaid.

### Unit Tests

**Test 2.7.1: DocBook mediaobject**
```
Test: mermaid_docbook
Expected:
  <informalfigure>
    <mediaobject>
      <imageobject>
        <imagedata fileref="mermaid_abc123.png"/>
      </imageobject>
    </mediaobject>
  </informalfigure>
```

### Acceptance Criteria
- [ ] Proper DocBook mediaobject structure
- [ ] Image reference correct

---

## Task 2.8: RTF Output for Mermaid

**File**: `src/rtfdocvisitor.cpp`

**Description**: Implement RTF rendering for DocVerbatim::Mermaid.

### Unit Tests

**Test 2.8.1: RTF image**
```
Test: mermaid_rtf
Expected:
  - PNG image embedded in RTF format
```

### Acceptance Criteria
- [ ] RTF image embedding works
- [ ] PNG format used

---

## Task 2.9: XML Output for Mermaid

**File**: `src/xmldocvisitor.cpp`

**Description**: Implement XML output for DocVerbatim::Mermaid, preserving raw mermaid content.

### Unit Tests

**Test 2.9.1: XML mermaid element**
```
Test: mermaid_xml
Expected:
  <mermaid>
  graph TD
    A-->B
  </mermaid>
```

### Acceptance Criteria
- [ ] Raw mermaid content preserved in XML
- [ ] Proper XML escaping

---

## Task 2.10: Markdown Code Fence Support

**File**: `src/markdown.cpp`

**Description**: Detect ```mermaid code fences and convert to @startmermaid.

```cpp
// In processCodeBlock(), add detection:
if (lang == "mermaid" || lang == ".mermaid") {
  // Convert to @startmermaid block
  out += "@startmermaid\n";
  out += codeBlock;
  out += "@endmermaid\n";
  return;
}
```

### Unit Tests

**Test 2.10.1: Markdown mermaid fence**
```
Test: mermaid_markdown_fence
Input markdown:
  ```mermaid
  graph TD
    A-->B
  ```

Expected:
  - Converted to @startmermaid/@endmermaid internally
  - Rendered as mermaid diagram
```

**Test 2.10.2: Markdown with attributes**
```
Test: mermaid_markdown_attrs
Input:
  ```{.mermaid width=400}
  graph TD
    A-->B
  ```

Expected:
  - Width attribute preserved
```

### Acceptance Criteria
- [ ] ```mermaid fences recognized
- [ ] Attributes (width, height) extracted
- [ ] Proper integration with mermaid rendering

---

# Phase 3: Mermaid File Command

## Task 3.1: Add DocMermaidFile Class

**File**: `src/docnode.h`

**Description**: Create DocMermaidFile class mirroring DocDotFile.

```cpp
class DocMermaidFile : public DocDiagFile
{
  public:
    DocMermaidFile(DocParser *parser, DocNodeVariant *parent,
                   const QCString &name, const QCString &context,
                   const QCString &srcFile, int srcLine);

    Kind kind() const override { return Kind_MermaidFile; }
};
```

### Unit Tests

**Test 3.1.1: DocMermaidFile creation**
```
Test: mermaid_file_node
Code:
  DocMermaidFile node(parser, parent, "diagram.mmd", "", "test.cpp", 10);

Expected:
  - node.kind() == Kind_MermaidFile
  - node.name() == "diagram.mmd"
```

### Acceptance Criteria
- [ ] DocMermaidFile class defined
- [ ] Inherits from DocDiagFile
- [ ] Kind_MermaidFile added to Kind enum

---

## Task 3.2: Register mermaidfile Command

**Files**: `src/cmdmapper.h`, `src/cmdmapper.cpp`

**Description**: Add CMD_MERMAIDFILE command.

```cpp
CMD_MERMAIDFILE = 122,

{ "mermaidfile", CMD_MERMAIDFILE },
```

### Unit Tests

**Test 3.2.1: Command registration**
```
Test: mermaid_file_cmd
Expected:
  - Mappers::cmdMapper->map("mermaidfile") == CMD_MERMAIDFILE
```

### Acceptance Criteria
- [ ] @mermaidfile command recognized

---

## Task 3.3: Parse mermaidfile Command

**File**: `src/docnode.cpp`

**Description**: Handle CMD_MERMAIDFILE in parser.

```cpp
case CMD_MERMAIDFILE:
{
  QCString fileName;
  QCString caption;
  QCString width, height;

  // Parse arguments similar to dotfile
  // ...

  // Search in MERMAIDFILE_DIRS
  QCString fullPath = findFilePath(fileName,
    Config_getList(MERMAIDFILE_DIRS));

  if (fullPath.isEmpty()) {
    warn(parser()->context.fileName, parser()->tokenizer.getLineNr(),
         "mermaidfile '%s' not found", qPrint(fileName));
  } else {
    children().append<DocMermaidFile>(
      parser(), thisVariant(),
      fullPath, parser()->context.context,
      parser()->context.fileName, parser()->tokenizer.getLineNr()
    );
  }
}
break;
```

### Unit Tests

**Test 3.3.1: File found**
```
Test: mermaid_file_found
Setup:
  - Create /tmp/diagrams/flow.mmd
  - MERMAIDFILE_DIRS = /tmp/diagrams

Input:
  @mermaidfile flow.mmd

Expected:
  - DocMermaidFile node created
  - Full path resolved
```

**Test 3.3.2: File not found**
```
Test: mermaid_file_notfound
Input:
  @mermaidfile nonexistent.mmd

Expected:
  - Warning: "mermaidfile 'nonexistent.mmd' not found"
```

**Test 3.3.3: File with caption**
```
Test: mermaid_file_caption
Input:
  @mermaidfile flow.mmd "Architecture Overview"

Expected:
  - Caption stored in node
```

### Acceptance Criteria
- [ ] File lookup in MERMAIDFILE_DIRS
- [ ] Caption argument parsed
- [ ] Width/height arguments parsed
- [ ] Clear warning for missing files

---

## Task 3.4: HTML Visitor for DocMermaidFile

**File**: `src/htmldocvisitor.cpp`

**Description**: Render DocMermaidFile to HTML.

```cpp
void HtmlDocVisitor::operator()(const DocMermaidFile &mf)
{
  // Read file content
  QCString content = fileToString(mf.file());

  // Insert into MermaidManager
  QCString baseName = MermaidManager::instance().insert(
    content, mf.name(), m_ci.dir(),
    getMermaidFormat(), mf.srcFile(), mf.srcLine()
  );

  // Generate HTML (similar to inline mermaid)
  // ...
}
```

### Unit Tests

**Test 3.4.1: HTML output from file**
```
Test: mermaid_file_html
Setup:
  - Create test.mmd with "graph TD\n  A-->B"

Expected HTML:
  <div class="mermaidgraph">
  <img src="test.svg" .../>
  </div>
```

### Acceptance Criteria
- [ ] File content read correctly
- [ ] Same rendering as inline mermaid
- [ ] Caption rendered if provided

---

## Task 3.5: LaTeX Visitor for DocMermaidFile

**File**: `src/latexdocvisitor.cpp`

### Acceptance Criteria
- [ ] Same rendering as inline mermaid for LaTeX

---

## Task 3.6: Other Visitors for DocMermaidFile

**Files**: `src/docbookvisitor.cpp`, `src/rtfdocvisitor.cpp`, `src/xmldocvisitor.cpp`

### Acceptance Criteria
- [ ] All output formats handle DocMermaidFile

---

# Phase 4: Auto-Generated Diagrams

## Task 4.1: Create MermaidRenderer Interface

**File**: `src/mermaidgraph.h` (new)

**Description**: Define interface for rendering graphs to Mermaid syntax.

```cpp
#ifndef MERMAIDGRAPH_H
#define MERMAIDGRAPH_H

#include "dotgraph.h"
#include "textstream.h"

class MermaidGraph
{
  public:
    enum DiagramType {
      ClassDiagram,
      FlowchartLR,  // Left to right (call graphs)
      FlowchartTD,  // Top to down (include deps)
      FlowchartRL   // Right to left (caller graphs)
    };

    static void writeHeader(TextStream &t, DiagramType type);
    static void writeFooter(TextStream &t);

    static void writeNode(TextStream &t,
                          const QCString &id,
                          const QCString &label,
                          const QCString &shape = "box");

    static void writeEdge(TextStream &t,
                          const QCString &fromId,
                          const QCString &toId,
                          const QCString &label = "",
                          bool dashed = false);

    static void writeClassNode(TextStream &t,
                               const QCString &className,
                               const StringVector &members,
                               const StringVector &methods);

    static void writeInheritance(TextStream &t,
                                 const QCString &child,
                                 const QCString &parent);

    static void writeAssociation(TextStream &t,
                                 const QCString &from,
                                 const QCString &to,
                                 const QCString &label = "");

  private:
    static QCString escapeLabel(const QCString &label);
    static QCString escapeId(const QCString &id);
};

#endif
```

### Unit Tests

**Test 4.1.1: Flowchart header**
```
Test: mermaid_graph_flowchart_header
Code:
  TextStream t;
  MermaidGraph::writeHeader(t, MermaidGraph::FlowchartLR);

Expected:
  t.str() == "flowchart LR\n"
```

**Test 4.1.2: Class diagram header**
```
Test: mermaid_graph_class_header
Code:
  TextStream t;
  MermaidGraph::writeHeader(t, MermaidGraph::ClassDiagram);

Expected:
  t.str() == "classDiagram\n"
```

**Test 4.1.3: Node output**
```
Test: mermaid_graph_node
Code:
  TextStream t;
  MermaidGraph::writeNode(t, "n1", "MyFunction", "box");

Expected:
  t.str() == "    n1[MyFunction]\n"
```

**Test 4.1.4: Edge output**
```
Test: mermaid_graph_edge
Code:
  TextStream t;
  MermaidGraph::writeEdge(t, "n1", "n2", "calls");

Expected:
  t.str() == "    n1 -->|calls| n2\n"
```

**Test 4.1.5: Dashed edge**
```
Test: mermaid_graph_dashed_edge
Code:
  MermaidGraph::writeEdge(t, "n1", "n2", "", true);

Expected:
  t.str() contains "-.->""
```

**Test 4.1.6: Class node**
```
Test: mermaid_graph_class_node
Code:
  MermaidGraph::writeClassNode(t, "Animal",
    {"int age", "string name"},
    {"void eat()", "void sleep()"});

Expected:
  class Animal {
    +int age
    +string name
    +void eat()
    +void sleep()
  }
```

**Test 4.1.7: Inheritance**
```
Test: mermaid_graph_inheritance
Code:
  MermaidGraph::writeInheritance(t, "Dog", "Animal");

Expected:
  t.str() == "    Animal <|-- Dog\n"
```

**Test 4.1.8: Label escaping**
```
Test: mermaid_graph_escape
Code:
  QCString escaped = MermaidGraph::escapeLabel("foo->bar<T>");

Expected:
  escaped == "foo-&gt;bar&lt;T&gt;" or similar safe form
```

### Acceptance Criteria
- [ ] All diagram type headers generated correctly
- [ ] Node syntax correct for flowcharts and class diagrams
- [ ] Edge syntax correct with labels
- [ ] Special characters properly escaped
- [ ] Mermaid syntax valid and renderable

---

## Task 4.2: Implement MermaidGraph

**File**: `src/mermaidgraph.cpp` (new)

**Description**: Full implementation of MermaidGraph methods.

### Unit Tests
(Same as 4.1, but testing actual implementation)

### Acceptance Criteria
- [ ] All methods implemented
- [ ] Generated syntax passes mermaid validation
- [ ] Unicode characters handled

---

## Task 4.3: Add USE_MERMAID Toggle to DotGraph

**File**: `src/dotgraph.h`, `src/dotgraph.cpp`

**Description**: Add logic to select Mermaid output when USE_MERMAID=YES.

```cpp
// In DotGraph::writeGraph():
if (Config_getBool(USE_MERMAID)) {
  return writeGraphMermaid(t, ...);
} else {
  return writeGraphDot(t, ...);
}
```

### Unit Tests

**Test 4.3.1: USE_MERMAID selects mermaid**
```
Test: dot_graph_use_mermaid
Setup:
  - USE_MERMAID = YES

Code:
  DotGraph graph;
  graph.writeGraph(t, ...);

Expected:
  - Output is valid mermaid syntax, not DOT
```

**Test 4.3.2: Default uses DOT**
```
Test: dot_graph_default_dot
Setup:
  - USE_MERMAID = NO (default)

Expected:
  - Output is DOT syntax (existing behavior)
```

### Acceptance Criteria
- [ ] USE_MERMAID controls output format
- [ ] Existing DOT behavior unchanged when USE_MERMAID=NO

---

## Task 4.4: Mermaid Class Inheritance Diagrams

**File**: `src/dotclassgraph.cpp`

**Description**: Generate Mermaid classDiagram for inheritance.

```cpp
QCString DotClassGraph::writeGraphMermaid(TextStream &t, ...)
{
  MermaidGraph::writeHeader(t, MermaidGraph::ClassDiagram);

  // Write base class
  writeClassNodeMermaid(t, m_startNode);

  // Write inheritance relations
  for (const auto &edge : m_startNode->edges()) {
    if (edge.type() == EdgeInfo::Inheritance) {
      MermaidGraph::writeInheritance(t,
        edge.child()->label(),
        m_startNode->label());
    }
  }

  MermaidGraph::writeFooter(t);

  return MermaidManager::instance().insert(t.str(), ...);
}
```

### Unit Tests

**Test 4.4.1: Simple inheritance**
```
Test: mermaid_class_simple_inheritance
Input C++:
  class Animal {};
  class Dog : public Animal {};

Expected mermaid:
  classDiagram
      Animal <|-- Dog
```

**Test 4.4.2: Multiple inheritance**
```
Test: mermaid_class_multiple_inheritance
Input C++:
  class A {};
  class B {};
  class C : public A, public B {};

Expected mermaid:
  classDiagram
      A <|-- C
      B <|-- C
```

**Test 4.4.3: Deep hierarchy**
```
Test: mermaid_class_deep_hierarchy
Input:
  Animal -> Mammal -> Dog -> Labrador

Expected:
  classDiagram
      Animal <|-- Mammal
      Mammal <|-- Dog
      Dog <|-- Labrador
```

**Test 4.4.4: Class with members**
```
Test: mermaid_class_members
Input:
  class Animal {
    int age;
    void eat();
  };

Expected mermaid:
  classDiagram
      class Animal {
          +int age
          +void eat()
      }
```

### Acceptance Criteria
- [ ] Inheritance arrows correct direction
- [ ] Multiple inheritance supported
- [ ] Class members shown
- [ ] Access specifiers indicated (+, -, #)

---

## Task 4.5: Mermaid Collaboration Diagrams

**File**: `src/dotclassgraph.cpp`

**Description**: Generate Mermaid for collaboration/composition relationships.

### Unit Tests

**Test 4.5.1: Composition**
```
Test: mermaid_collab_composition
Input:
  class Car { Engine engine; };

Expected:
  classDiagram
      Car *-- Engine
```

**Test 4.5.2: Aggregation**
```
Test: mermaid_collab_aggregation
Input:
  class Department { vector<Employee*> staff; };

Expected:
  classDiagram
      Department o-- Employee
```

### Acceptance Criteria
- [ ] Composition arrows (*--)
- [ ] Aggregation arrows (o--)
- [ ] Labels on relationships

---

## Task 4.6: Mermaid Call Graphs

**File**: `src/dotcallgraph.cpp`

**Description**: Generate Mermaid flowchart for function call graphs.

```cpp
QCString DotCallGraph::writeGraphMermaid(TextStream &t, ...)
{
  MermaidGraph::writeHeader(t, MermaidGraph::FlowchartLR);

  // Root function node
  MermaidGraph::writeNode(t, nodeId(m_startNode),
                          m_startNode->label(), "box");

  // Called functions
  for (const auto &callee : m_startNode->callees()) {
    MermaidGraph::writeNode(t, nodeId(callee), callee->label());
    MermaidGraph::writeEdge(t, nodeId(m_startNode), nodeId(callee));
  }

  MermaidGraph::writeFooter(t);
  return MermaidManager::instance().insert(t.str(), ...);
}
```

### Unit Tests

**Test 4.6.1: Simple call graph**
```
Test: mermaid_call_simple
Input:
  void main() { init(); process(); }
  void init() {}
  void process() {}

Expected:
  flowchart LR
      main[main] --> init[init]
      main --> process[process]
```

**Test 4.6.2: Nested calls**
```
Test: mermaid_call_nested
Input:
  void a() { b(); }
  void b() { c(); }
  void c() {}

Expected:
  flowchart LR
      a --> b
      b --> c
```

**Test 4.6.3: Recursive call**
```
Test: mermaid_call_recursive
Input:
  void factorial(int n) { factorial(n-1); }

Expected:
  - Self-referencing edge
  - No infinite loop in generation
```

### Acceptance Criteria
- [ ] Left-to-right flowchart for call direction
- [ ] All called functions shown
- [ ] Recursion handled without infinite loop
- [ ] External functions distinguished

---

## Task 4.7: Mermaid Caller Graphs

**File**: `src/dotcallgraph.cpp`

**Description**: Generate reverse call graph (who calls this function).

### Unit Tests

**Test 4.7.1: Caller graph**
```
Test: mermaid_caller
Input:
  void helper() {}
  void a() { helper(); }
  void b() { helper(); }

Expected (for helper):
  flowchart RL
      a --> helper
      b --> helper
```

### Acceptance Criteria
- [ ] Right-to-left flowchart (callers on left)
- [ ] All callers shown

---

## Task 4.8: Mermaid Include Dependency Graphs

**File**: `src/dotincldepgraph.cpp`

**Description**: Generate Mermaid for #include dependencies.

### Unit Tests

**Test 4.8.1: Include graph**
```
Test: mermaid_include
Input:
  // main.cpp
  #include "util.h"
  #include "config.h"

Expected:
  flowchart TD
      main.cpp --> util.h
      main.cpp --> config.h
```

**Test 4.8.2: Included-by graph**
```
Test: mermaid_includedby
Expected:
  flowchart TD
      util.h --> main.cpp
      util.h --> test.cpp
```

### Acceptance Criteria
- [ ] Top-down flowchart
- [ ] System vs local includes distinguished
- [ ] Missing files marked

---

## Task 4.9: Mermaid Directory Dependency Graphs

**File**: `src/dotdirdeps.cpp`

**Description**: Generate Mermaid for directory-level dependencies.

### Unit Tests

**Test 4.9.1: Directory deps**
```
Test: mermaid_dirdeps
Expected:
  flowchart TD
      src --> include
      src --> lib
```

### Acceptance Criteria
- [ ] Directories as nodes
- [ ] Dependencies shown

---

## Task 4.10: Mermaid Group Collaboration Graphs

**File**: `src/dotgroupcollaboration.cpp`

### Acceptance Criteria
- [ ] Module/group relationships shown

---

## Task 4.11: HTML Integration for Auto-Generated Mermaid

**File**: `src/htmlgen.cpp`

**Description**: Embed mermaid diagrams in HTML output.

### Unit Tests

**Test 4.11.1: Class page with mermaid**
```
Test: mermaid_html_class_page
Setup:
  - USE_MERMAID = YES
  - MERMAID_FORMAT = svg

Expected:
  - Class page contains <img src="...svg">
  - SVG file generated
```

### Acceptance Criteria
- [ ] Diagrams appear on appropriate pages
- [ ] Links work
- [ ] Consistent styling

---

## Task 4.12: LaTeX Integration for Auto-Generated Mermaid

**File**: `src/latexgen.cpp`

### Acceptance Criteria
- [ ] PNG images included in LaTeX
- [ ] Proper sizing

---

# Phase 5: Client-Side & Raw Output

## Task 5.1: Include Mermaid.js in HTML Output

**File**: `src/htmlgen.cpp`, `src/resourcemgr.cpp`

**Description**: Add mermaid.js library to generated HTML.

```cpp
// Option 1: CDN link
void HtmlGenerator::writeFooterFile(TextStream &t)
{
  if (Config_getBool(USE_MERMAID) && Config_getBool(MERMAID_CLIENT_SIDE)) {
    t << "<script src=\"https://cdn.jsdelivr.net/npm/mermaid/dist/mermaid.min.js\"></script>\n";
    t << "<script>mermaid.initialize({startOnLoad:true});</script>\n";
  }
}

// Option 2: Bundled (add to resources)
```

### Unit Tests

**Test 5.1.1: CDN script tag**
```
Test: mermaid_js_cdn
Setup:
  - MERMAID_CLIENT_SIDE = YES

Expected HTML footer:
  <script src="https://cdn.jsdelivr.net/npm/mermaid/dist/mermaid.min.js">
```

**Test 5.1.2: Mermaid initialization**
```
Test: mermaid_js_init
Expected:
  - mermaid.initialize() called
```

### Acceptance Criteria
- [ ] Mermaid.js loaded only when MERMAID_CLIENT_SIDE=YES
- [ ] Initialization script present
- [ ] Diagrams render in browser

---

## Task 5.2: CSS Styling for Mermaid Diagrams

**File**: `src/htmlgen.cpp` or CSS resources

**Description**: Add CSS for mermaid diagram containers.

```css
div.mermaid {
  text-align: center;
  margin: 1em 0;
}

div.mermaidgraph {
  text-align: center;
  margin: 1em 0;
}

div.mermaidgraph img {
  max-width: 100%;
  height: auto;
}
```

### Unit Tests

**Test 5.2.1: CSS inclusion**
```
Test: mermaid_css
Expected:
  - doxygen.css contains mermaid styles
```

### Acceptance Criteria
- [ ] Diagrams centered
- [ ] Responsive sizing
- [ ] Consistent with doxygen theme

---

## Task 5.3: Raw Mermaid Output Mode

**File**: Multiple visitors

**Description**: When MERMAID_OUTPUT_RAW=YES, output raw mermaid syntax.

```cpp
// In HTML visitor:
if (Config_getBool(MERMAID_OUTPUT_RAW)) {
  m_t << "<pre><code class=\"language-mermaid\">\n";
  m_t << s.text();
  m_t << "</code></pre>\n";
}
```

### Unit Tests

**Test 5.3.1: Raw output HTML**
```
Test: mermaid_raw_html
Setup:
  - MERMAID_OUTPUT_RAW = YES

Expected:
  <pre><code class="language-mermaid">
  graph TD
    A-->B
  </code></pre>
```

**Test 5.3.2: Raw output XML**
```
Test: mermaid_raw_xml
Expected:
  <mermaid>
  graph TD
    A-->B
  </mermaid>
```

### Acceptance Criteria
- [ ] Raw syntax preserved exactly
- [ ] Proper code block formatting
- [ ] Works for both inline and auto-generated diagrams

---

## Task 5.4: Auto-Generated Diagrams in Raw Mode

**Description**: When USE_MERMAID=YES and MERMAID_OUTPUT_RAW=YES, output raw mermaid for class/call graphs.

### Unit Tests

**Test 5.4.1: Raw class diagram**
```
Test: mermaid_raw_class_graph
Setup:
  - USE_MERMAID = YES
  - MERMAID_OUTPUT_RAW = YES

Input:
  class Animal {};
  class Dog : public Animal {};

Expected HTML:
  <pre><code class="language-mermaid">
  classDiagram
      Animal <|-- Dog
  </code></pre>
```

### Acceptance Criteria
- [ ] Auto-generated diagrams output as raw mermaid
- [ ] Useful for AI parsing

---

## Task 5.5: Theme Support for Client-Side Rendering

**File**: `src/htmlgen.cpp`

**Description**: Pass MERMAID_THEME to client-side initialization.

```javascript
mermaid.initialize({
  startOnLoad: true,
  theme: 'dark'  // From MERMAID_THEME config
});
```

### Unit Tests

**Test 5.5.1: Dark theme**
```
Test: mermaid_client_theme
Setup:
  - MERMAID_THEME = dark
  - MERMAID_CLIENT_SIDE = YES

Expected:
  - mermaid.initialize({...theme:'dark'...})
```

### Acceptance Criteria
- [ ] Theme passed to mermaid.js
- [ ] All four themes work

---

## Task 5.6: Error Handling and Fallbacks

**Description**: Graceful degradation when mermaid rendering fails.

### Unit Tests

**Test 5.6.1: mmdc failure fallback**
```
Test: mermaid_fallback
Setup:
  - Invalid mermaid syntax

Expected:
  - Warning message with source location
  - Placeholder or raw syntax shown instead of broken image
```

**Test 5.6.2: Missing mmdc fallback**
```
Test: mermaid_no_mmdc
Setup:
  - MERMAID_EXECUTABLE not set or invalid

Expected:
  - Clear error message
  - Diagram not rendered (but build continues)
```

### Acceptance Criteria
- [ ] Build doesn't fail on mermaid errors
- [ ] Clear error messages with file:line
- [ ] Graceful degradation

---

# Integration Test Suite

## Test Suite: Full Pipeline Tests

### Test I.1: Complete HTML Generation
```
Test: mermaid_integration_html
Setup:
  - Multi-file C++ project
  - USE_MERMAID = YES
  - Inline mermaid diagrams
  - @mermaidfile commands

Run: doxygen Doxyfile

Verify:
  - All diagram SVGs generated
  - HTML pages reference correct images
  - Navigation links work
  - No errors or warnings (except expected)
```

### Test I.2: Complete LaTeX Generation
```
Test: mermaid_integration_latex
Run: doxygen + pdflatex

Verify:
  - PDF generates without errors
  - Diagrams appear in PDF
  - Correct positioning
```

### Test I.3: Client-Side Rendering
```
Test: mermaid_integration_clientside
Setup:
  - MERMAID_CLIENT_SIDE = YES

Verify:
  - HTML loads in browser
  - Diagrams render via JavaScript
  - Theme applied correctly
```

### Test I.4: Large Codebase Performance
```
Test: mermaid_integration_performance
Setup:
  - 100+ classes with inheritance
  - 50+ call graphs

Verify:
  - Completes in reasonable time
  - Caching works (second run faster)
  - No memory issues
```

### Test I.5: Backwards Compatibility
```
Test: mermaid_integration_compat
Setup:
  - USE_MERMAID = NO (default)
  - Existing project with DOT diagrams

Verify:
  - All existing diagrams still work
  - No changes to output
```

---

# Acceptance Test Checklist

## Phase 1 Acceptance
- [ ] All config options documented in generated Doxyfile
- [ ] `doxygen -h` shows mermaid options
- [ ] MermaidManager processes diagrams
- [ ] Caching prevents unnecessary regeneration
- [ ] Clear errors when mmdc missing/fails

## Phase 2 Acceptance
- [ ] @startmermaid/@endmermaid works in all comment styles
- [ ] ```mermaid code fences work in markdown
- [ ] HTML output renders diagrams
- [ ] LaTeX output includes diagrams
- [ ] Captions displayed

## Phase 3 Acceptance
- [ ] @mermaidfile includes external files
- [ ] MERMAIDFILE_DIRS search works
- [ ] File not found produces warning

## Phase 4 Acceptance
- [ ] USE_MERMAID=YES switches to mermaid diagrams
- [ ] Class inheritance diagrams correct
- [ ] Call/caller graphs correct
- [ ] Include dependency graphs correct
- [ ] Same information as DOT diagrams

## Phase 5 Acceptance
- [ ] Client-side rendering works in browser
- [ ] Raw output mode preserves syntax
- [ ] Themes apply correctly
- [ ] Graceful error handling

---

# Test File Templates

## testing/100_mermaid_basic/

**Doxyfile**:
```
PROJECT_NAME = MermaidBasic
GENERATE_HTML = YES
GENERATE_LATEX = NO
MERMAID_EXECUTABLE = mmdc
INPUT = .
```

**input.cpp**:
```cpp
/**
 * @brief Test function with mermaid diagram
 *
 * @startmermaid
 * graph TD
 *     A[Start] --> B{Decision}
 *     B -->|Yes| C[OK]
 *     B -->|No| D[End]
 * @endmermaid
 */
void testFunction() {}
```

**expected/html/index.html**: Contains mermaid diagram image

---

## testing/103_mermaid_class_graph/

**Doxyfile**:
```
PROJECT_NAME = MermaidClassGraph
GENERATE_HTML = YES
USE_MERMAID = YES
CLASS_GRAPH = YES
INPUT = .
```

**input.cpp**:
```cpp
class Animal {
public:
    virtual void speak() = 0;
};

class Dog : public Animal {
public:
    void speak() override {}
};

class Cat : public Animal {
public:
    void speak() override {}
};
```

**expected**: Class hierarchy in mermaid format with Animal as parent of Dog and Cat.
