#pragma once

#include <map>
#include <string>

namespace render {
  /**
    * @brief Tiny base class — gives every named entity a `name()`
    *        without forcing it to derive from `QObject`.
    *
    * `Object` is the runtime-side equivalent of `world::Element`'s
    * name field, but without any of the Qt scene-graph machinery.
    * `Primitive`, `Material`, `Light`, and friends inherit from it
    * so the event-trace path in `State::recordEvent` can prefix
    * each line with `obj->name() + ": "` for readable per-ray
    * trace dumps.
    *
    * The name is purely informational — the renderer doesn't
    * dispatch on it, no two objects need distinct names, and an
    * unnamed object reads as an empty string in trace output.
    *
    * The world side (`world::Element`) carries its own UUID and
    * display-name machinery for editor purposes; `make_named<T>(...)`
    * in the world-to-runtime conversion path forwards the world
    * `Element`'s name into the runtime `Object`'s name field.
    */
  class Object {
  public:
    using Metadata = std::map<std::string, std::string>;

    inline Object() {
    }
    virtual ~Object() {
    }

    /// Set this object's display name. Used only by the trace
    /// machinery; safe to leave unset for objects you don't expect
    /// to surface in `state.events`.
    inline void setName(const std::string& name) {
      m_name = name;
    }

    /// @returns the display name, or the empty string if unset.
    inline const std::string& name() const {
      return m_name;
    }

    /// Attach an opaque string metadata value for diagnostics and inspection.
    /// Runtime rendering does not read these values.
    inline void setMetadataValue(const std::string& key, const std::string& value) {
      m_metadata[key] = value;
    }

    /// @returns the metadata value for @p key, or an empty string when absent.
    inline std::string metadataValue(const std::string& key) const {
      const auto it = m_metadata.find(key);
      return it == m_metadata.end() ? std::string() : it->second;
    }

    /// @returns all opaque diagnostic/inspection metadata attached to this object.
    inline const Metadata& metadata() const {
      return m_metadata;
    }

    /// Replace all opaque diagnostic/inspection metadata attached to this object.
    inline void setMetadata(const Metadata& metadata) {
      m_metadata = metadata;
    }

  private:
    std::string m_name;
    Metadata m_metadata;
  };
}
