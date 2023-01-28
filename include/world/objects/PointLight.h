#pragma once

#include "world/objects/Light.h"

/**
  * Represents a point light. A point light only has a position in space, which
  * means that light rays at different points on a surface come in at different
  * angles.
  *
  * <table><tr>
  * <td>@image html point_light_position_1.png "Position 1"</td>
  * <td>@image html point_light_position_2.png "Position 2"</td>
  * <td>@image html point_light_position_3.png "Position 3"</td>
  * <td>@image html point_light_position_4.png "Position 4"</td>
  * <td>@image html point_light_position_5.png "Position 5"</td>
  * </tr></table>
  */
class PointLight : public Light {
  Q_OBJECT;

public:
  explicit PointLight(Element* parent = nullptr);

  virtual std::shared_ptr<raytracer::Light> toRaytracer() const;
};
