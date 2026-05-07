#pragma once

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
    inline Object() {}
    virtual ~Object() {}

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

  private:
    std::string m_name;
  };
}
