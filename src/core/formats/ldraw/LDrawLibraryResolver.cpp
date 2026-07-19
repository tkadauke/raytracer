#include "core/formats/ldraw/LDrawLibraryResolver.h"

#include "core/formats/AssetResolver.h"
#include "core/formats/ldraw/LDrawParseError.h"
#include "core/util/StringUtil.h"

#include <fstream>
#include <sstream>
#include <utility>

using namespace std;
namespace fs = std::filesystem;

namespace {
  string toLookupKey(const fs::path& path) {
    return core::util::lowercase(path.generic_string());
  }

  fs::path normalizeExistingPath(const fs::path& path) {
    std::error_code error;
    const fs::path canonical = fs::weakly_canonical(path, error);
    if (!error)
      return canonical;
    return path.lexically_normal();
  }

  [[noreturn]] void throwResolverError(const string& detail) {
    throw LDrawParseError(0, detail, __FILE__, __LINE__);
  }

  string formatPathList(const vector<fs::path>& paths) {
    ostringstream message;
    for (size_t i = 0; i < paths.size(); ++i) {
      if (i > 0)
        message << ", ";
      message << paths[i].string();
    }
    return message.str();
  }

  vector<fs::path> findCycle(const vector<fs::path>& stack, const fs::path& path) {
    const string key = toLookupKey(path);
    for (auto i = stack.begin(); i != stack.end(); ++i) {
      if (toLookupKey(*i) == key) {
        vector<fs::path> cycle(i, stack.end());
        cycle.push_back(path);
        return cycle;
      }
    }
    return {};
  }

  bool hasSubfileReference(const LDrawCommand& command) {
    return holds_alternative<LDrawSubfileReference>(command);
  }
}

LDrawLibraryResolver::LDrawLibraryResolver(fs::path libraryRoot)
    : m_libraryRoot(std::move(libraryRoot)) {
}

void LDrawLibraryResolver::setLibraryRoot(fs::path libraryRoot) {
  m_libraryRoot = std::move(libraryRoot);
}

const fs::path& LDrawLibraryResolver::libraryRoot() const {
  return m_libraryRoot;
}

LDrawLibraryResolver::DocumentPtr LDrawLibraryResolver::load(const fs::path& path) {
  return loadMutable(path);
}

LDrawLibraryResolver::DocumentPtr LDrawLibraryResolver::loadWithSubfiles(const fs::path& path) {
  vector<fs::path> stack;
  return loadWithSubfiles(path, stack);
}

LDrawLibraryResolver::DocumentPtr
LDrawLibraryResolver::resolve(const LDrawResolvedDocument& currentDocument,
                              const string& filename) {
  return loadMutable(resolvePath(currentDocument.path, filename));
}

LDrawLibraryResolver::DocumentPtr LDrawLibraryResolver::resolve(
  const LDrawResolvedDocument& currentDocument,
  const string& filename,
  LDrawDiagnostics& diagnostics,
  int lineNumber) {
  try {
    return resolve(currentDocument, filename);
  } catch (const LDrawParseError&) {
    LDrawDiagnostic diagnostic;
    diagnostic.severity = LDrawDiagnosticSeverity::Error;
    diagnostic.code = LDrawDiagnosticCode::MissingSubfile;
    diagnostic.file = currentDocument.path.string();
    diagnostic.lineNumber = lineNumber;
    diagnostic.message = "unable to resolve LDraw subfile";
    diagnostic.reference = filename;
    for (const auto& root : searchRoots(currentDocument.path))
      diagnostic.searchedRoots.push_back(root.string());
    diagnostics.add(std::move(diagnostic));
    throw;
  }
}

vector<fs::path> LDrawLibraryResolver::searchRoots(const fs::path& currentFile) const {
  vector<fs::path> roots;
  if (!currentFile.empty())
    roots.push_back(currentFile.parent_path());

  if (!m_libraryRoot.empty()) {
    roots.push_back(m_libraryRoot / "parts");
    roots.push_back(m_libraryRoot / "parts" / "s");
    roots.push_back(m_libraryRoot / "p");
    roots.push_back(m_libraryRoot / "p" / "48");
    roots.push_back(m_libraryRoot / "models");
  }
  return roots;
}

size_t LDrawLibraryResolver::cacheSize() const {
  return m_cache.size();
}

void LDrawLibraryResolver::clearCache() {
  m_cache.clear();
}

LDrawLibraryResolver::MutableDocumentPtr LDrawLibraryResolver::loadMutable(const fs::path& path) {
  const fs::path normalized = normalizeExistingPath(path);
  const string key = toLookupKey(normalized);
  const auto cached = m_cache.find(key);
  if (cached != m_cache.end())
    return cached->second;

  ifstream input(normalized);
  if (!input) {
    ostringstream message;
    message << "Unable to read LDraw file '" << path.string() << "'";
    throwResolverError(message.str());
  }

  auto document = make_shared<LDrawResolvedDocument>();
  document->path = normalized;
  document->commands = LDrawParser().parse(input);
  m_cache.emplace(key, document);
  return document;
}

LDrawLibraryResolver::MutableDocumentPtr LDrawLibraryResolver::loadWithSubfiles(
  const fs::path& path,
  vector<fs::path>& stack) {
  const fs::path normalized = normalizeExistingPath(path);
  const vector<fs::path> cycle = findCycle(stack, normalized);
  if (!cycle.empty()) {
    ostringstream message;
    message << "Cycle detected while loading LDraw subfiles: " << formatPathList(cycle);
    throwResolverError(message.str());
  }

  auto document = loadMutable(normalized);
  stack.push_back(document->path);
  for (const LDrawCommand& command : document->commands) {
    if (!hasSubfileReference(command))
      continue;
    const auto& reference = get<LDrawSubfileReference>(command);
    loadWithSubfiles(resolvePath(document->path, reference.filename), stack);
  }
  stack.pop_back();
  return document;
}

fs::path LDrawLibraryResolver::resolvePath(const fs::path& currentFile, const string& filename) const {
  const core::AssetResolver resolver(searchRoots(currentFile),
                                     core::AssetCaseSensitivity::CaseInsensitive);
  try {
    return resolver.resolve(filename).path;
  } catch (const core::AssetResolutionError& error) {
    ostringstream message;
    message << "Unable to resolve LDraw subfile '" << filename << "' from '"
            << currentFile.string() << "'. Searched roots: "
            << formatPathList(error.searchedRoots());
    throwResolverError(message.str());
  }
}
