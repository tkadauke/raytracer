#pragma once

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

class LDrawFileResolver {
public:
  virtual ~LDrawFileResolver() = default;

  [[nodiscard]] virtual std::unique_ptr<std::istream> open(const std::string& filename) const = 0;

  [[nodiscard]] virtual std::string cacheKey(const std::string& filename) const;

protected:
  [[nodiscard]] static std::string normalizedFilename(std::string filename);
};

class LDrawFilesystemResolver : public LDrawFileResolver {
public:
  explicit LDrawFilesystemResolver(std::vector<std::string> searchDirectories = {});

  void addSearchDirectory(const std::string& directory);

  [[nodiscard]] std::unique_ptr<std::istream> open(const std::string& filename) const override;
  [[nodiscard]] std::string cacheKey(const std::string& filename) const override;

private:
  std::vector<std::string> m_searchDirectories;
};
