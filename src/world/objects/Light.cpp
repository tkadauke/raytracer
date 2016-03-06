#include "world/objects/Light.h"

Light::Light(Element* parent)
  : Transformable(parent),
    m_visible(true),
    m_color(Colord::white()),
    m_intensity(0.5)
{
}

#include "Light.moc"
