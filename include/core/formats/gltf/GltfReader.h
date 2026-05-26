#pragma once

#include "core/formats/AssetResolver.h"
#include "core/formats/gltf/GltfAsset.h"
#include "core/formats/gltf/GltfDiagnostic.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace core::gltf {

  struct ReadResult {
    std::optional<Asset> asset;
    Diagnostics diagnostics;

    [[nodiscard]] bool ok() const {
      return asset.has_value() && !diagnostics.hasErrors();
    }
  };

  class Reader {
  public:
    [[nodiscard]] static ReadResult readFile(const std::filesystem::path& path,
                                             AssetResolver resolver = AssetResolver());
    [[nodiscard]] static ReadResult readJson(const std::string& json,
                                             const std::filesystem::path& currentFile = {},
                                             AssetResolver resolver = AssetResolver());
    [[nodiscard]] static ReadResult readGlb(const std::vector<std::uint8_t>& bytes,
                                            const std::filesystem::path& currentFile = {},
                                            AssetResolver resolver = AssetResolver());
  };

}
