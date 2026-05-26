#include "world/import/ImportResult.h"

#include "world/objects/Element.h"
#include "world/objects/Group.h"
#include "world/objects/Scene.h"

#include <algorithm>
#include <utility>

namespace world {

  ImportResult::ImportResult() = default;

  ImportResult::ImportResult(std::unique_ptr<Element> root, ImportSourceMetadata source)
      : m_root(std::move(root)),
        m_source(std::move(source)) {
  }

  ImportResult::~ImportResult() = default;

  ImportResult::ImportResult(ImportResult&&) noexcept = default;

  ImportResult& ImportResult::operator=(ImportResult&&) noexcept = default;

  ImportResult ImportResult::failed(std::vector<ImportDiagnostic> diagnostics,
                                    ImportSourceMetadata source) {
    ImportResult result;
    result.m_diagnostics = std::move(diagnostics);
    result.m_source = std::move(source);
    return result;
  }

  bool ImportResult::succeeded() const {
    return hasRoot() && !hasErrors();
  }

  bool ImportResult::failed() const {
    return !succeeded();
  }

  bool ImportResult::hasRoot() const {
    return static_cast<bool>(m_root);
  }

  bool ImportResult::hasErrors() const {
    return std::any_of(m_diagnostics.begin(), m_diagnostics.end(),
                       [](const ImportDiagnostic& diagnostic) { return diagnostic.isError(); });
  }

  bool ImportResult::hasWarnings() const {
    return std::any_of(m_diagnostics.begin(), m_diagnostics.end(),
                       [](const ImportDiagnostic& diagnostic) { return diagnostic.isWarning(); });
  }

  Element* ImportResult::root() const {
    return m_root.get();
  }

  Scene* ImportResult::sceneRoot() const {
    return qobject_cast<Scene*>(m_root.get());
  }

  Group* ImportResult::groupRoot() const {
    return qobject_cast<Group*>(m_root.get());
  }

  std::unique_ptr<Element> ImportResult::takeRoot() {
    return std::move(m_root);
  }

  void ImportResult::setRoot(std::unique_ptr<Element> root) {
    m_root = std::move(root);
  }

  const std::vector<ImportDiagnostic>& ImportResult::diagnostics() const {
    return m_diagnostics;
  }

  void ImportResult::addDiagnostic(const ImportDiagnostic& diagnostic) {
    m_diagnostics.push_back(diagnostic);
  }

  const ImportSourceMetadata& ImportResult::source() const {
    return m_source;
  }

  void ImportResult::setSource(ImportSourceMetadata source) {
    m_source = std::move(source);
  }

}
