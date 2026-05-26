#include "core/formats/AssetResolver.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace core {
  namespace {
    fs::path normalizedRoot(const fs::path& path) {
      if (path.empty())
        return ".";
      return path.lexically_normal();
    }

    fs::path normalizedExistingPath(const fs::path& path) {
      std::error_code error;
      const fs::path canonical = fs::weakly_canonical(path, error);
      if (!error)
        return canonical;
      return path.lexically_normal();
    }

    fs::path normalizedReferencePath(std::string requestedPath) {
      std::replace(requestedPath.begin(), requestedPath.end(), '\\', '/');
      return fs::path(requestedPath).lexically_normal();
    }

    bool isRegularFile(const fs::path& path) {
      std::error_code error;
      return fs::is_regular_file(path, error);
    }

    bool pathExists(const fs::path& path) {
      std::error_code error;
      return fs::exists(path, error);
    }

    bool equalsIgnoreCase(const std::string& left, const std::string& right) {
      return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
                                                       [](unsigned char a, unsigned char b) {
                                                         return std::tolower(a) == std::tolower(b);
                                                       });
    }

    fs::path findExactCase(const fs::path& path) {
      fs::path resolved = path.root_path();
      if (resolved.empty())
        resolved = ".";

      for (const fs::path& part : path.relative_path()) {
        std::error_code error;
        bool found = false;
        for (const fs::directory_entry& entry : fs::directory_iterator(resolved, error)) {
          if (error)
            break;
          if (entry.path().filename() == part) {
            resolved = entry.path();
            found = true;
            break;
          }
        }

        if (!found)
          return {};
      }

      return isRegularFile(resolved) ? normalizedExistingPath(resolved) : fs::path();
    }

    fs::path findCaseInsensitive(const fs::path& path) {
      fs::path resolved = path.root_path();
      if (resolved.empty())
        resolved = ".";

      for (const fs::path& part : path.relative_path()) {
        const fs::path exact = resolved / part;
        if (pathExists(exact)) {
          resolved = exact;
          continue;
        }

        std::error_code error;
        bool found = false;
        for (const fs::directory_entry& entry : fs::directory_iterator(resolved, error)) {
          if (error)
            break;
          if (equalsIgnoreCase(entry.path().filename().string(), part.string())) {
            resolved = entry.path();
            found = true;
            break;
          }
        }

        if (!found)
          return {};
      }

      return isRegularFile(resolved) ? normalizedExistingPath(resolved) : fs::path();
    }

    std::string identityFor(const fs::path& path, AssetCaseSensitivity caseSensitivity) {
      std::string identity = normalizedExistingPath(path).generic_string();
      if (caseSensitivity == AssetCaseSensitivity::CaseInsensitive) {
        std::transform(identity.begin(), identity.end(), identity.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      }
      return identity;
    }

    std::string formatPathList(const std::vector<fs::path>& paths) {
      std::ostringstream message;
      for (std::size_t i = 0; i < paths.size(); ++i) {
        if (i > 0)
          message << ", ";
        message << paths[i].string();
      }
      return message.str();
    }

    std::string makeErrorMessage(const std::string& requestedPath,
                                 const std::vector<fs::path>& searchedRoots) {
      std::ostringstream message;
      message << "Unable to resolve import asset '" << requestedPath << "'";
      if (!searchedRoots.empty())
        message << ". Searched roots: " << formatPathList(searchedRoots);
      return message.str();
    }
  }

  AssetResolutionError::AssetResolutionError(std::string requestedPath,
                                             std::vector<fs::path> searchedRoots)
      : std::runtime_error(makeErrorMessage(requestedPath, searchedRoots)),
        m_requestedPath(std::move(requestedPath)),
        m_searchedRoots(std::move(searchedRoots)) {
  }

  const std::string& AssetResolutionError::requestedPath() const {
    return m_requestedPath;
  }

  const std::vector<fs::path>& AssetResolutionError::searchedRoots() const {
    return m_searchedRoots;
  }

  AssetResolver::AssetResolver(std::vector<fs::path> searchRoots,
                               AssetCaseSensitivity caseSensitivity)
      : m_caseSensitivity(caseSensitivity) {
    setSearchRoots(std::move(searchRoots));
  }

  void AssetResolver::addSearchRoot(fs::path root) {
    m_searchRoots.push_back(normalizedRoot(root));
  }

  void AssetResolver::setSearchRoots(std::vector<fs::path> roots) {
    m_searchRoots.clear();
    m_searchRoots.reserve(roots.size());
    for (fs::path& root : roots)
      addSearchRoot(std::move(root));
  }

  const std::vector<fs::path>& AssetResolver::searchRoots() const {
    return m_searchRoots;
  }

  void AssetResolver::setCaseSensitivity(AssetCaseSensitivity caseSensitivity) {
    m_caseSensitivity = caseSensitivity;
  }

  AssetCaseSensitivity AssetResolver::caseSensitivity() const {
    return m_caseSensitivity;
  }

  ResolvedAsset AssetResolver::resolve(const std::string& requestedPath,
                                       const fs::path& currentFile) const {
    const fs::path reference = normalizedReferencePath(requestedPath);
    std::vector<fs::path> candidates;

    if (reference.is_absolute()) {
      candidates.push_back(reference);
    } else {
      for (const fs::path& root : searchedRoots(currentFile))
        candidates.push_back(root / reference);
    }

    for (const fs::path& candidate : candidates) {
      if (m_caseSensitivity == AssetCaseSensitivity::Exact) {
        const fs::path exact = findExactCase(candidate);
        if (!exact.empty())
          return resolvedAssetForPath(exact);
      } else {
        const fs::path insensitive = findCaseInsensitive(candidate);
        if (!insensitive.empty())
          return resolvedAssetForPath(insensitive);
      }
    }

    std::vector<fs::path> roots = reference.is_absolute()
                                    ? std::vector<fs::path>{reference.parent_path()}
                                    : searchedRoots(currentFile);
    throw AssetResolutionError(requestedPath, std::move(roots));
  }

  std::vector<fs::path> AssetResolver::searchedRoots(const fs::path& currentFile) const {
    std::vector<fs::path> roots;
    if (!currentFile.empty())
      roots.push_back(normalizedRoot(currentFile.parent_path()));
    roots.insert(roots.end(), m_searchRoots.begin(), m_searchRoots.end());
    return roots;
  }

  ResolvedAsset AssetResolver::resolvedAssetForPath(const fs::path& path) const {
    const fs::path normalized = normalizedExistingPath(path);
    return {normalized, identityFor(normalized, m_caseSensitivity)};
  }

}
