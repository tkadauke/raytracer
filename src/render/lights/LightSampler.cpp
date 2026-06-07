#include "render/lights/LightSampler.h"

#include "render/lights/Light.h"

#include <algorithm>
#include <cmath>

using namespace render;

LightSampler::LightSampler(const Scene::Lights& lights) {
  std::size_t lightIndex = 0;
  for (const auto& light : lights) {
    if (light) {
      m_entries.push_back(Entry{light.get(), lightIndex, weightFor(*light)});
    }
    ++lightIndex;
  }

  normalizeWeights();
}

bool LightSampler::empty() const {
  return m_entries.empty();
}

std::size_t LightSampler::size() const {
  return m_entries.size();
}

LightSampler::Selection LightSampler::select(double unitSample) const {
  if (empty() || m_totalWeight <= 0.0) {
    return Selection();
  }

  if (!(unitSample >= 0.0)) {
    unitSample = 0.0;
  }
  unitSample = std::min(unitSample, std::nextafter(1.0, 0.0));
  const double target = unitSample * m_totalWeight;

  double cumulative = 0.0;
  for (std::size_t entryIndex = 0; entryIndex != m_entries.size(); ++entryIndex) {
    const Entry& entry = m_entries[entryIndex];
    cumulative += entry.weight;
    if (target < cumulative) {
      return Selection{entry.light, entry.lightIndex, selectionPdf(entryIndex)};
    }
  }

  const std::size_t entryIndex = m_entries.size() - 1u;
  const Entry& entry = m_entries[entryIndex];
  return Selection{entry.light, entry.lightIndex, selectionPdf(entryIndex)};
}

double LightSampler::selectionPdf(std::size_t entryIndex) const {
  if (entryIndex >= m_entries.size() || m_totalWeight <= 0.0) {
    return 0.0;
  }
  return m_entries[entryIndex].weight / m_totalWeight;
}

double LightSampler::pdf(const Vector3d& point, const Vector3d& direction) const {
  double result = 0.0;
  for (std::size_t entryIndex = 0; entryIndex != m_entries.size(); ++entryIndex) {
    const Entry& entry = m_entries[entryIndex];
    result += selectionPdf(entryIndex) * entry.light->pdf(point, direction);
  }
  return result;
}

double LightSampler::weightFor(const Light& light) const {
  const std::optional<Colord> power = light.power();
  const Colord weightColor = power.has_value() ? *power : light.emission();
  return std::max(0.0, weightColor.max());
}

void LightSampler::normalizeWeights() {
  m_totalWeight = 0.0;
  for (const Entry& entry : m_entries) {
    m_totalWeight += entry.weight;
  }

  if (m_totalWeight <= 0.0 && !m_entries.empty()) {
    useUniformWeights();
  }
}

void LightSampler::useUniformWeights() {
  for (Entry& entry : m_entries) {
    entry.weight = 1.0;
  }
  m_totalWeight = static_cast<double>(m_entries.size());
}
