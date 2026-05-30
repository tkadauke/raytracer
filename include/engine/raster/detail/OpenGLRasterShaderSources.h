#pragma once

namespace engine::raster::detail {
  constexpr const char* kOpenGLRasterVertexShader = R"glsl(
uniform mat4 viewProjection;
uniform bool useMatrixProjection;
attribute vec4 position;
attribute vec3 worldPosition;
attribute vec3 normal;
attribute vec4 color;
attribute vec2 uv;
attribute float alphaScale;
attribute float materialDiffuse;
attribute vec3 materialSpecularColor;
attribute float materialSpecularCoefficient;
attribute float materialSpecularExponent;
attribute vec3 ambientLighting;
attribute vec3 directLighting;
attribute vec3 specular;
attribute float albedoMode;
varying vec3 fragmentWorldPosition;
varying vec3 fragmentNormal;
varying vec4 vertexColor;
varying vec2 vertexUV;
varying float fragmentAlphaScale;
varying float fragmentMaterialDiffuse;
varying vec3 fragmentMaterialSpecularColor;
varying float fragmentMaterialSpecularCoefficient;
varying float fragmentMaterialSpecularExponent;
varying vec3 fragmentAmbientLighting;
varying vec3 fragmentDirectLighting;
varying vec3 fragmentSpecular;
varying float fragmentAlbedoMode;
void main() {
  if (useMatrixProjection) {
    gl_Position = viewProjection * vec4(worldPosition, 1.0);
  } else {
    gl_Position = vec4(position.xyz * position.w, position.w);
  }
  fragmentWorldPosition = worldPosition;
  fragmentNormal = normal;
  vertexColor = color;
  vertexUV = uv;
  fragmentAlphaScale = alphaScale;
  fragmentMaterialDiffuse = materialDiffuse;
  fragmentMaterialSpecularColor = materialSpecularColor;
  fragmentMaterialSpecularCoefficient = materialSpecularCoefficient;
  fragmentMaterialSpecularExponent = materialSpecularExponent;
  fragmentAmbientLighting = ambientLighting;
  fragmentDirectLighting = directLighting;
  fragmentSpecular = specular;
  fragmentAlbedoMode = albedoMode;
}
)glsl";

  constexpr const char* kOpenGLRasterFragmentShader = R"glsl(
varying vec3 fragmentWorldPosition;
varying vec3 fragmentNormal;
varying vec4 vertexColor;
varying vec2 vertexUV;
varying float fragmentAlphaScale;
varying float fragmentMaterialDiffuse;
varying vec3 fragmentMaterialSpecularColor;
varying float fragmentMaterialSpecularCoefficient;
varying float fragmentMaterialSpecularExponent;
varying vec3 fragmentAmbientLighting;
varying vec3 fragmentDirectLighting;
varying vec3 fragmentSpecular;
varying float fragmentAlbedoMode;
uniform vec3 cameraPosition;
uniform int directionalLightCount;
uniform vec3 directionalLightDirection[8];
uniform vec3 directionalLightRadiance[8];
uniform int pointLightCount;
uniform vec3 pointLightPosition[8];
uniform vec3 pointLightRadiance[8];
uniform bool alphaTestEnabled;
uniform int alphaFunc;
uniform float alphaReference;
uniform sampler2D imageTexture;
uniform vec2 imageUVScale;
uniform vec3 albedoTint;
uniform vec3 checkerBright;
uniform vec3 checkerDark;
uniform bool shadowTextureEnabled;
uniform sampler2D shadowTexture;
uniform vec3 shadowOrigin;
uniform vec3 shadowRight;
uniform vec3 shadowUp;
uniform vec3 shadowForward;
uniform float shadowHalfExtent;
uniform float shadowDepthScale;
uniform float shadowBias;
uniform int shadowFilterRadius;
uniform vec2 shadowTexelSize;
bool alphaPass(float alpha) {
  if (!alphaTestEnabled) return true;
  if (alphaFunc == 0) return false;
  if (alphaFunc == 1) return alpha < alphaReference;
  if (alphaFunc == 2) return alpha == alphaReference;
  if (alphaFunc == 3) return alpha <= alphaReference;
  if (alphaFunc == 4) return alpha > alphaReference;
  if (alphaFunc == 5) return alpha >= alphaReference;
  if (alphaFunc == 6) return alpha != alphaReference;
  return true;
}
float shadowSampleVisibility(vec2 uv, float receiver, float bias) {
  if (uv.x < 0.0 || uv.y < 0.0 || uv.x >= 1.0 || uv.y >= 1.0) return 1.0;
  float occluderDepth = texture2D(shadowTexture, uv).r;
  if (occluderDepth >= 0.999999) return 1.0;
  return receiver <= occluderDepth + bias ? 1.0 : 0.0;
}
float shadowVisibility(vec3 worldPosition) {
  if (!shadowTextureEnabled) return 1.0;
  vec3 rel = worldPosition - shadowOrigin;
  float lightX = dot(rel, shadowRight);
  float lightY = dot(rel, shadowUp);
  float receiverDepth = dot(rel, shadowForward);
  if (receiverDepth < 0.0) return 1.0;
  vec2 uv = vec2((lightX / shadowHalfExtent + 1.0) * 0.5,
                (lightY / shadowHalfExtent + 1.0) * 0.5);
  float receiver = receiverDepth / shadowDepthScale;
  float bias = shadowBias / shadowDepthScale;
  if (shadowFilterRadius <= 0) {
    return shadowSampleVisibility(uv, receiver, bias);
  }
  float lit = 0.0;
  float samples = 0.0;
  for (int dy = -4; dy <= 4; ++dy) {
    for (int dx = -4; dx <= 4; ++dx) {
      if (dx >= -shadowFilterRadius && dx <= shadowFilterRadius &&
          dy >= -shadowFilterRadius && dy <= shadowFilterRadius) {
        lit += shadowSampleVisibility(uv + vec2(float(dx), float(dy)) * shadowTexelSize, receiver, bias);
        samples += 1.0;
      }
    }
  }
  return lit / samples;
}
void addDirectionalLighting(vec3 normal, vec3 viewDir, float shadow,
                            inout vec3 directLighting, inout vec3 specularLighting) {
  for (int i = 0; i < 8; ++i) {
    if (i < directionalLightCount) {
      vec3 lightDir = normalize(directionalLightDirection[i]);
      float nDotL = max(0.0, dot(normal, lightDir));
      if (nDotL > 0.0) {
        vec3 radiance = directionalLightRadiance[i];
        directLighting += radiance * fragmentMaterialDiffuse * nDotL * shadow;
        if (fragmentMaterialSpecularCoefficient > 0.0) {
          vec3 lobeDirection = normalize(-lightDir + normal * 2.0 * nDotL);
          float lobeDotView = max(0.0, dot(lobeDirection, viewDir));
          if (lobeDotView > 0.0) {
            specularLighting += fragmentMaterialSpecularColor *
              fragmentMaterialSpecularCoefficient *
              pow(lobeDotView, fragmentMaterialSpecularExponent) * radiance * nDotL * shadow;
          }
        }
      }
    }
  }
}
void addPointLighting(vec3 normal, vec3 viewDir,
                      inout vec3 directLighting, inout vec3 specularLighting) {
  for (int i = 0; i < 8; ++i) {
    if (i < pointLightCount) {
      vec3 lightDir = normalize(pointLightPosition[i] - fragmentWorldPosition);
      float nDotL = max(0.0, dot(normal, lightDir));
      if (nDotL > 0.0) {
        vec3 radiance = pointLightRadiance[i];
        directLighting += radiance * fragmentMaterialDiffuse * nDotL;
        if (fragmentMaterialSpecularCoefficient > 0.0) {
          vec3 lobeDirection = normalize(-lightDir + normal * 2.0 * nDotL);
          float lobeDotView = max(0.0, dot(lobeDirection, viewDir));
          if (lobeDotView > 0.0) {
            specularLighting += fragmentMaterialSpecularColor *
              fragmentMaterialSpecularCoefficient *
              pow(lobeDotView, fragmentMaterialSpecularExponent) * radiance * nDotL;
          }
        }
      }
    }
  }
}
void main() {
  vec4 sourceColor = vertexColor;
  if (fragmentAlbedoMode > 0.5 && fragmentAlbedoMode < 1.5) {
    sourceColor = vec4(vertexUV.x, vertexUV.y, 0.0, 1.0);
    sourceColor.a = max(max(sourceColor.r, sourceColor.g), sourceColor.b) * fragmentAlphaScale;
  } else if (fragmentAlbedoMode > 1.5 && fragmentAlbedoMode < 2.5) {
    sourceColor = texture2D(imageTexture, vertexUV * imageUVScale);
    sourceColor.a = max(max(sourceColor.r, sourceColor.g), sourceColor.b) * fragmentAlphaScale;
  } else if (fragmentAlbedoMode > 2.5 && fragmentAlbedoMode < 3.5) {
    vec2 scaledUV = vertexUV * imageUVScale;
    float checker = mod(floor(scaledUV.x) + floor(scaledUV.y), 2.0);
    vec3 checkerColor = mix(checkerBright, checkerDark, step(0.5, checker));
    sourceColor = vec4(checkerColor, 1.0);
    sourceColor.a = max(max(sourceColor.r, sourceColor.g), sourceColor.b) * fragmentAlphaScale;
  } else if (fragmentAlbedoMode > 3.5 && fragmentAlbedoMode < 4.5) {
    float checker = mod(floor(fragmentWorldPosition.x) + floor(fragmentWorldPosition.z), 2.0);
    vec3 checkerColor = mix(checkerBright, checkerDark, step(0.5, checker));
    sourceColor = vec4(checkerColor, 1.0);
    sourceColor.a = max(max(sourceColor.r, sourceColor.g), sourceColor.b) * fragmentAlphaScale;
  }
  sourceColor.rgb *= albedoTint;
  if (fragmentAlbedoMode > 0.5) {
    sourceColor.a = max(max(sourceColor.r, sourceColor.g), sourceColor.b) * fragmentAlphaScale;
  }
  vec3 normal = normalize(fragmentNormal);
  vec3 viewDir = normalize(cameraPosition - fragmentWorldPosition);
  float shadow = shadowVisibility(fragmentWorldPosition);
  vec3 shaderDirectLighting = vec3(0.0, 0.0, 0.0);
  vec3 shaderSpecular = vec3(0.0, 0.0, 0.0);
  addDirectionalLighting(normal, viewDir, shadow, shaderDirectLighting, shaderSpecular);
  addPointLighting(normal, viewDir, shaderDirectLighting, shaderSpecular);
  sourceColor.rgb = sourceColor.rgb * (fragmentAmbientLighting + fragmentDirectLighting +
                                       shaderDirectLighting) + fragmentSpecular + shaderSpecular;
  if (!alphaPass(sourceColor.a)) discard;
  gl_FragColor = sourceColor;
}
)glsl";
}
