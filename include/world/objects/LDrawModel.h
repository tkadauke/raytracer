#pragma once

#include <memory>

#include "world/objects/Surface.h"

/**
  * Imports an LDraw `.ldr`, `.dat`, or MPD model into a world scene.
  *
  * The inherited `scale` transform controls model size; `libraryPath` points at
  * an optional LDraw parts library root containing directories such as `parts`
  * and `p`.
  */
class LDrawModel : public Surface {
  Q_OBJECT
  Q_PROPERTY(QString filePath READ filePath WRITE setFilePath)
  Q_PROPERTY(QString libraryPath READ libraryPath WRITE setLibraryPath)
  Q_PROPERTY(bool smoothNormals READ smoothNormals WRITE setSmoothNormals)

public:
  explicit LDrawModel(Element* parent = nullptr);

  inline const QString& filePath() const {
    return m_filePath;
  }

  inline void setFilePath(const QString& filePath) {
    m_filePath = filePath;
  }

  inline const QString& libraryPath() const {
    return m_libraryPath;
  }

  inline void setLibraryPath(const QString& libraryPath) {
    m_libraryPath = libraryPath;
  }

  inline bool smoothNormals() const {
    return m_smoothNormals;
  }

  inline void setSmoothNormals(bool smoothNormals) {
    m_smoothNormals = smoothNormals;
  }

  std::shared_ptr<render::Primitive> toRaytracerPrimitive() const override;

private:
  QString m_filePath;
  QString m_libraryPath;
  bool m_smoothNormals;
};
