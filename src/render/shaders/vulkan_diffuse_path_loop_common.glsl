layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

const uint gpuIntersectionLeafNodeFlag = 1u;
const uint gpuIntersectionTrianglePrimitiveKind = 1u;
const uint gpuIntersectionSpherePrimitiveKind = 2u;
const uint gpuIntersectionPlanePrimitiveKind = 3u;
const uint gpuIntersectionRectanglePrimitiveKind = 4u;
const uint gpuIntersectionDiskPrimitiveKind = 5u;
const uint gpuIntersectionOpenCylinderPrimitiveKind = 6u;
const uint gpuIntersectionTorusPrimitiveKind = 7u;
const uint gpuTracingMatteMaterialKind = 1u;
const uint gpuTracingEmissiveMaterialKind = 2u;
const uint gpuTracingPhongMaterialKind = 3u;
const uint gpuTracingReflectiveMaterialKind = 4u;
const uint gpuTracingTransparentMaterialKind = 5u;
const uint gpuTracingPortalMaterialKind = 6u;
const uint gpuTracingConstantColorTextureKind = 1u;
const uint gpuTracingCheckerBoardTextureKind = 2u;
const uint gpuTracingImageTextureKind = 3u;
const uint gpuTracingTintedTextureKind = 4u;
const uint gpuTracingUvColorTextureKind = 5u;
const uint gpuTracingPlanarTextureMappingKind = 1u;
const uint gpuTracingUvTextureMappingKind = 2u;
const uint gpuTracingTextureMappingMask = 0xffu;
const uint gpuTracingTextureWrapClampFlag = 256u;
const uint gpuTracingTextureFilterBilinearFlag = 512u;
const uint gpuTracingPointLightKind = 1u;
const uint gpuTracingDirectionalLightKind = 2u;
const uint gpuTracingRectangularAreaLightKind = 3u;
const uint gpuDiffusePathLoopAccumulationTargetPath = 1u;
const uint gpuDiffusePathLoopAccumulationTargetSampleSlot = 2u;
const uint gpuPrimaryPathGenerationModePinhole = 1u;
const uint gpuPrimaryPathGenerationModeOrthographic = 2u;
const uint gpuPrimaryPathGenerationModeThinLens = 3u;
const uint gpuPrimaryPathGenerationModeEquirectangular = 4u;
const uint gpuPrimaryPathGenerationModeSpherical = 5u;
const uint gpuPrimaryPathGenerationModeFishEye = 6u;
const uint gpuPrimaryPathGenerationModeTiltShift = 7u;
const uint gpuDiffusePathStateActiveFlag = 1u;
const uint gpuDiffusePathStateTerminatedFlag = 2u;
const uint gpuDiffusePathStateSampledFromBsdfFlag = 4u;
const uint gpuDiffusePathStateBsdfSampleDeltaFlag = 8u;
const uint gpuDiffusePathStateUnsupportedFlag = 16u;
const uint gpuDiffusePathDenoiserFeatureValidFlag = 1u;
const uint gpuDiffusePathStepEventInactive = 0u;
const uint gpuDiffusePathStepEventMiss = 1u;
const uint gpuDiffusePathStepEventHit = 2u;
const uint gpuDiffusePathStepEventUnsupported = 3u;
const uint gpuSampleInitialCoordinateState = 0x811c9dc5u;
const uint gpuSampleCoordinateStep = 0x9e3779b9u;
const uint gpuPrimaryPathSampleDimensionBase = 3u;
const uint gpuPrimaryPathSampleDimensionStride = 4u;
const float pathLoopInvPi = 0.31830988618379067154;
const float pathLoopTau = 6.28318530717958647692;
const float pathLoopRayEpsilon = 1.0e-7;
const float pathLoopMinimumHitDistance = 1.0e-4;
const float pathLoopMinimumContinuationProbability = 0.05;
const uint pathLoopMaxTextureEvaluationDepth = 8u;

struct GpuDiffusePathLoopLaunchParameters {
  uint layoutVersion;
  uint maxDepth;
  uint russianRouletteDepth;
  uint directLightSamples;
  uint captureDiagnostics;
  uint initialPathCount;
  uint imageWidth;
  uint imageHeight;
  uint materialCount;
  uint textureCount;
  uint lightCount;
  uint environmentCount;
  uint debugIdCount;
  uint geometryByteOffset;
  uint materialByteOffset;
  uint textureByteOffset;
  uint lightByteOffset;
  uint environmentByteOffset;
  uint debugIdByteOffset;
  uint sceneUploadBytes;
  uint accumulationTargetMode;
  uint bvhByteOffset;
  uint primitiveByteOffset;
  uint triangleByteOffset;
  uint sphereByteOffset;
  uint planeByteOffset;
  uint rectangleByteOffset;
  uint diskByteOffset;
  uint openCylinderByteOffset;
  uint torusByteOffset;
  uint transformByteOffset;
  uint bvhNodeCount;
  uint primitiveCount;
  uint triangleCount;
  uint sphereCount;
  uint planeCount;
  uint rectangleCount;
  uint diskCount;
  uint openCylinderCount;
  uint torusCount;
  uint transformCount;
  uint primaryPathGenerationMode;
  uint primaryPathSamplesPerPixel;
  uint primaryPathSampleSeed;
  uint primaryPathRequestedWidth;
  int primaryPathRequestedLeft;
  int primaryPathRequestedTop;
  uint primaryPathRequestedHeight;
  uint primaryPathActualWidth;
  int primaryPathActualLeft;
  int primaryPathActualTop;
  uint primaryPathActualHeight;
  uint captureDenoiserFeatures;
  uint displayResolveTonemap;
  uint reserved2;
  uint reserved3;
  vec4 primaryPathOrigin;
  vec4 primaryPathTopLeft;
  vec4 primaryPathRight;
  vec4 primaryPathDown;
  vec4 primaryPathLensRight;
  vec4 primaryPathLensUp;
  vec4 primaryPathForward;
  vec4 primaryPathLensParameters;
};

struct GpuIntersectionBounds {
  vec4 minimum;
  vec4 maximum;
};

struct GpuIntersectionBvhNode {
  GpuIntersectionBounds bounds;
  uint leftOrFirstPrimitive;
  uint primitiveCount;
  uint flags;
  uint reserved;
};

struct GpuIntersectionPrimitiveRecord {
  GpuIntersectionBounds bounds;
  uint kind;
  uint material;
  uint object;
  uint transform;
  uint payloadOffset;
  uint payloadCount;
  uint reserved0;
  uint reserved1;
};

struct GpuIntersectionTrianglePayload {
  vec4 point0;
  vec4 point1;
  vec4 point2;
  vec4 normal0;
  vec4 normal1;
  vec4 normal2;
  vec4 uv0;
  vec4 uv1;
  vec4 uv2;
  vec4 minimumHitDistance;
};

struct GpuIntersectionSpherePayload {
  vec4 centerRadius;
};

struct GpuIntersectionPlanePayload {
  vec4 normalDistance;
};

struct GpuIntersectionRectanglePayload {
  vec4 corner;
  vec4 leg1;
  vec4 leg2;
  vec4 normal;
};

struct GpuIntersectionDiskPayload {
  vec4 centerRadius;
  vec4 normalMinimumHitDistance;
};

struct GpuIntersectionOpenCylinderPayload {
  vec4 radiusHalfHeight;
};

struct GpuIntersectionTorusPayload {
  vec4 sweptTubeRadius;
};

struct GpuIntersectionTransformPayload {
  vec4 pointMatrix0;
  vec4 pointMatrix1;
  vec4 pointMatrix2;
  vec4 pointMatrix3;
  vec4 normalMatrix0;
  vec4 normalMatrix1;
  vec4 normalMatrix2;
  vec4 normalMatrix3;
  vec4 inversePointMatrix0;
  vec4 inversePointMatrix1;
  vec4 inversePointMatrix2;
  vec4 inversePointMatrix3;
  vec4 inverseDirectionMatrix0;
  vec4 inverseDirectionMatrix1;
  vec4 inverseDirectionMatrix2;
  vec4 inverseDirectionMatrix3;
};

struct GpuIntersectionRay {
  vec4 origin;
  vec4 direction;
  float minDistance;
  float maxDistance;
  float timeSample;
  uint flags;
  uint rayIndex;
  uint reserved0;
  uint reserved1;
  uint reserved2;
};

struct GpuIntersectionHitRecord {
  uint hit;
  uint material;
  uint object;
  uint primitiveRecord;
  uint rayIndex;
  uint reservedId0;
  uint reservedId1;
  uint reservedId2;
  float distance;
  float reservedDistance0;
  float reservedDistance1;
  float reservedDistance2;
  vec4 point;
  vec4 normal;
  vec4 uv;
  vec4 barycentric;
};

struct LocalPrimitiveHit {
  bool hit;
  float distance;
  vec4 point;
  vec4 normal;
  vec4 uv;
  vec4 barycentric;
};

struct GpuTracingMaterialRecord {
  uint kind;
  uint albedoTexture;
  uint emissionTexture;
  uint flags;
  vec4 parameters;
  vec4 specularParameters;
  vec4 continuationParameters;
  vec4 transmissionParameters;
  vec4 portalOriginMatrix0;
  vec4 portalOriginMatrix1;
  vec4 portalOriginMatrix2;
  vec4 portalOriginMatrix3;
  vec4 portalDirectionMatrix0;
  vec4 portalDirectionMatrix1;
  vec4 portalDirectionMatrix2;
};

struct GpuTracingTextureRecord {
  uint kind;
  uint payloadOffset;
  uint payloadCount;
  uint flags;
  vec4 parameters;
};

struct GpuTracingLightRecord {
  uint kind;
  uint emissionTexture;
  uint flags;
  uint object;
  vec4 positionOrDirection;
  vec4 u;
  vec4 v;
  vec4 parameters;
};

struct DirectLightSelection {
  uint valid;
  uint lightIndex;
  float pdf;
};

struct DirectLightSample {
  uint valid;
  uint delta;
  vec3 direction;
  vec4 radiance;
  float distance;
  float pdf;
};

struct DirectLightEstimate {
  vec4 radiance;
  uint sampleCount;
  uint visibilityRayCount;
  uint contributingSampleCount;
  uint occludedSampleCount;
};

struct GpuDiffusePathStateRecord {
  GpuIntersectionRay ray;
  vec4 throughput;
  vec4 accumulatedRadiance;
  uint pixelIndex;
  uint primarySampleIndex;
  uint depth;
  uint sampleSeed;
  uint sampleDimensionBase;
  uint sampleDimensionStride;
  uint flags;
  uint reserved0;
  float previousBsdfPdf;
  float previousLightPdf;
  uint previousMaterial;
  uint previousEventFlags;
  uint reserved1;
  uint reserved2;
  uint reserved3;
  uint reserved4;
};

struct GpuDiffusePathStepRecord {
  uint event;
  uint pathIndex;
  uint pixelIndex;
  uint primarySampleIndex;
  uint depth;
  uint material;
  uint object;
  uint flags;
  vec4 emittedRadiance;
  vec4 directLightRadiance;
  vec4 missRadiance;
  vec4 continuationThroughput;
  uint directLightSampleCount;
  uint directLightVisibilityRayCount;
  uint directLightContributingSampleCount;
  uint directLightOccludedSampleCount;
};

struct GpuDiffusePathDenoiserFeatureRecord {
  uint pixelIndex;
  uint primarySampleIndex;
  uint flags;
  uint reserved;
  vec4 albedo;
  vec4 normal;
  float depth;
  float reservedDepth0;
  float reservedDepth1;
  float reservedDepth2;
};

layout(std430, binding = 0) readonly buffer PathLoopParameters {
  GpuDiffusePathLoopLaunchParameters parameters;
};

layout(std430, binding = 1) writeonly buffer EchoedPathLoopParameters {
  GpuDiffusePathLoopLaunchParameters echoedParameters;
};

layout(std430, binding = 2) readonly buffer SceneUpload {
  uint sceneWords[];
};

layout(std430, binding = 3) readonly buffer InitialPathStates {
  GpuDiffusePathStateRecord initialPathStates[];
};

layout(std430, binding = 4) buffer ActivePathStates {
  GpuDiffusePathStateRecord activePathStates[];
};

layout(std430, binding = 5) writeonly buffer NextPathStates {
  GpuDiffusePathStateRecord nextPathStates[];
};

layout(std430, binding = 6) writeonly buffer StepRecords {
  GpuDiffusePathStepRecord stepRecords[];
};

layout(std430, binding = 7) buffer RetainedIndices {
  uint retainedIndices[];
};

layout(std430, binding = 8) buffer Accumulation {
  uint accumulationWords[];
};

layout(std430, binding = 9) buffer DenoiserFeatures {
  GpuDiffusePathDenoiserFeatureRecord denoiserFeatures[];
};

layout(std430, binding = 10) buffer ActivePathCounts {
  uint activePathCounts[];
};

float rayInfinity() {
  return uintBitsToFloat(0x7f800000u);
}

bool finiteFloat(float value) {
  return !isnan(value) && !isinf(value);
}

uint byteOffsetWords(uint byteOffset) {
  return byteOffset / 4u;
}

vec4 readVec4(uint wordOffset) {
  return vec4(uintBitsToFloat(sceneWords[wordOffset + 0u]),
              uintBitsToFloat(sceneWords[wordOffset + 1u]),
              uintBitsToFloat(sceneWords[wordOffset + 2u]),
              uintBitsToFloat(sceneWords[wordOffset + 3u]));
}

bool pathStateIsActive(GpuDiffusePathStateRecord path) {
  return (path.flags & gpuDiffusePathStateActiveFlag) != 0u &&
         (path.flags & gpuDiffusePathStateTerminatedFlag) == 0u;
}

uint activePathFlags(uint flags) {
  return (flags | gpuDiffusePathStateActiveFlag) & ~gpuDiffusePathStateTerminatedFlag;
}

uint terminatedPathFlags(uint flags) {
  return (flags | gpuDiffusePathStateTerminatedFlag) & ~gpuDiffusePathStateActiveFlag;
}

uint unsupportedPathFlags(uint flags) {
  return (flags | gpuDiffusePathStateUnsupportedFlag | gpuDiffusePathStateTerminatedFlag) &
         ~gpuDiffusePathStateActiveFlag;
}

uint sampleDimension(GpuDiffusePathStateRecord path, uint offset) {
  return path.sampleDimensionBase + path.depth * path.sampleDimensionStride + offset;
}

uint pcgHash32(uint inputValue) {
  const uint state = inputValue * 747796405u + 2891336453u;
  const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

uint mixSampleCoordinateWord(uint state, uint value) {
  return pcgHash32(state ^
                   (value + gpuSampleCoordinateStep + (state << 6u) + (state >> 2u)));
}

uint sampleHash(uint seed, uint pixelIndex, uint primarySampleIndex, uint dimension,
                uint component) {
  uint state = gpuSampleInitialCoordinateState;
  state = mixSampleCoordinateWord(state, seed);
  state = mixSampleCoordinateWord(state, pixelIndex);
  state = mixSampleCoordinateWord(state, primarySampleIndex);
  state = mixSampleCoordinateWord(state, dimension);
  state = mixSampleCoordinateWord(state, component);
  return pcgHash32(state);
}

float sample1D(GpuDiffusePathStateRecord path, uint dimension, uint component) {
  return float(sampleHash(path.sampleSeed, path.pixelIndex, path.primarySampleIndex,
                          dimension, component) >> 8u) *
         (1.0 / 16777216.0);
}

vec2 sample2D(GpuDiffusePathStateRecord path, uint dimension) {
  return vec2(sample1D(path, dimension, 0u), sample1D(path, dimension, 1u));
}

float sample1DCoordinate(uint seed, uint pixelIndex, uint primarySampleIndex, uint dimension,
                         uint component) {
  return float(sampleHash(seed, pixelIndex, primarySampleIndex, dimension, component) >> 8u) *
         (1.0 / 16777216.0);
}

vec2 sample2DCoordinate(uint seed, uint pixelIndex, uint primarySampleIndex, uint dimension) {
  return vec2(sample1DCoordinate(seed, pixelIndex, primarySampleIndex, dimension, 0u),
              sample1DCoordinate(seed, pixelIndex, primarySampleIndex, dimension, 1u));
}

vec2 concentricMapToDisc(vec2 sample) {
  const float a = 2.0 * sample.x - 1.0;
  const float b = 2.0 * sample.y - 1.0;
  if (a == 0.0 && b == 0.0) {
    return vec2(0.0, 0.0);
  }

  float r = 0.0;
  float phi = 0.0;
  if (a * a > b * b) {
    r = a;
    phi = (pathLoopTau / 8.0) * (b / a);
  } else {
    r = b;
    phi = (pathLoopTau / 4.0) - (pathLoopTau / 8.0) * (a / b);
  }
  return vec2(r * cos(phi), r * sin(phi));
}

GpuDiffusePathStateRecord makePinholePrimaryPath(uint pathIndex) {
  const uint sampleIndex = pathIndex % parameters.primaryPathSamplesPerPixel;
  const uint pixelOrdinal = pathIndex / parameters.primaryPathSamplesPerPixel;
  const uint localX = pixelOrdinal % parameters.primaryPathActualWidth;
  const uint localY = pixelOrdinal / parameters.primaryPathActualWidth;
  const int column = parameters.primaryPathActualLeft + int(localX);
  const int row = parameters.primaryPathActualTop + int(localY);
  const uint pixelIndex =
      uint((row - parameters.primaryPathRequestedTop) * int(parameters.primaryPathRequestedWidth) +
           (column - parameters.primaryPathRequestedLeft));
  const vec2 pixelSample =
      sample2DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 0u);
  const float timeSample =
      sample1DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 1u, 0u);
  const vec4 pixelPoint = parameters.primaryPathTopLeft +
                          parameters.primaryPathRight * (float(column) + pixelSample.x) +
                          parameters.primaryPathDown * (float(row) + pixelSample.y);

  GpuDiffusePathStateRecord path;
  path.ray.origin = parameters.primaryPathOrigin;
  path.ray.direction = vec4(normalize(pixelPoint.xyz - parameters.primaryPathOrigin.xyz), 0.0);
  path.ray.minDistance = 0.0;
  path.ray.maxDistance = uintBitsToFloat(0x7f800000u);
  path.ray.timeSample = timeSample;
  path.ray.flags = 0u;
  path.ray.rayIndex = pathIndex;
  path.ray.reserved0 = 0u;
  path.ray.reserved1 = 0u;
  path.ray.reserved2 = 0u;
  path.throughput = vec4(1.0, 1.0, 1.0, 0.0);
  path.accumulatedRadiance = vec4(0.0);
  path.pixelIndex = pixelIndex;
  path.primarySampleIndex = sampleIndex;
  path.depth = 0u;
  path.sampleSeed = parameters.primaryPathSampleSeed;
  path.sampleDimensionBase = gpuPrimaryPathSampleDimensionBase;
  path.sampleDimensionStride = gpuPrimaryPathSampleDimensionStride;
  path.flags = gpuDiffusePathStateActiveFlag;
  path.reserved0 = 0u;
  path.previousBsdfPdf = 0.0;
  path.previousLightPdf = 0.0;
  path.previousMaterial = 0u;
  path.previousEventFlags = 0u;
  path.reserved1 = 0u;
  path.reserved2 = 0u;
  path.reserved3 = 0u;
  path.reserved4 = 0u;
  return path;
}

GpuDiffusePathStateRecord makeOrthographicPrimaryPath(uint pathIndex) {
  const uint sampleIndex = pathIndex % parameters.primaryPathSamplesPerPixel;
  const uint pixelOrdinal = pathIndex / parameters.primaryPathSamplesPerPixel;
  const uint localX = pixelOrdinal % parameters.primaryPathActualWidth;
  const uint localY = pixelOrdinal / parameters.primaryPathActualWidth;
  const int column = parameters.primaryPathActualLeft + int(localX);
  const int row = parameters.primaryPathActualTop + int(localY);
  const uint pixelIndex =
      uint((row - parameters.primaryPathRequestedTop) * int(parameters.primaryPathRequestedWidth) +
           (column - parameters.primaryPathRequestedLeft));
  const vec2 pixelSample =
      sample2DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 0u);
  const float timeSample =
      sample1DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 1u, 0u);
  const vec4 pixelPoint = parameters.primaryPathTopLeft +
                          parameters.primaryPathRight * (float(column) + pixelSample.x) +
                          parameters.primaryPathDown * (float(row) + pixelSample.y);

  GpuDiffusePathStateRecord path;
  path.ray.origin = pixelPoint;
  path.ray.direction = vec4(normalize(parameters.primaryPathOrigin.xyz), 0.0);
  path.ray.minDistance = 0.0;
  path.ray.maxDistance = uintBitsToFloat(0x7f800000u);
  path.ray.timeSample = timeSample;
  path.ray.flags = 0u;
  path.ray.rayIndex = pathIndex;
  path.ray.reserved0 = 0u;
  path.ray.reserved1 = 0u;
  path.ray.reserved2 = 0u;
  path.throughput = vec4(1.0, 1.0, 1.0, 0.0);
  path.accumulatedRadiance = vec4(0.0);
  path.pixelIndex = pixelIndex;
  path.primarySampleIndex = sampleIndex;
  path.depth = 0u;
  path.sampleSeed = parameters.primaryPathSampleSeed;
  path.sampleDimensionBase = gpuPrimaryPathSampleDimensionBase;
  path.sampleDimensionStride = gpuPrimaryPathSampleDimensionStride;
  path.flags = gpuDiffusePathStateActiveFlag;
  path.reserved0 = 0u;
  path.previousBsdfPdf = 0.0;
  path.previousLightPdf = 0.0;
  path.previousMaterial = 0u;
  path.previousEventFlags = 0u;
  path.reserved1 = 0u;
  path.reserved2 = 0u;
  path.reserved3 = 0u;
  path.reserved4 = 0u;
  return path;
}

GpuDiffusePathStateRecord makeThinLensPrimaryPath(uint pathIndex) {
  const uint sampleIndex = pathIndex % parameters.primaryPathSamplesPerPixel;
  const uint pixelOrdinal = pathIndex / parameters.primaryPathSamplesPerPixel;
  const uint localX = pixelOrdinal % parameters.primaryPathActualWidth;
  const uint localY = pixelOrdinal / parameters.primaryPathActualWidth;
  const int column = parameters.primaryPathActualLeft + int(localX);
  const int row = parameters.primaryPathActualTop + int(localY);
  const uint pixelIndex =
      uint((row - parameters.primaryPathRequestedTop) * int(parameters.primaryPathRequestedWidth) +
           (column - parameters.primaryPathRequestedLeft));
  const vec2 pixelSample =
      sample2DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 0u);
  const float timeSample =
      sample1DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 1u, 0u);
  const vec2 lensSample = concentricMapToDisc(
      sample2DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 2u));
  const vec4 pixelPoint = parameters.primaryPathTopLeft +
                          parameters.primaryPathRight * (float(column) + pixelSample.x) +
                          parameters.primaryPathDown * (float(row) + pixelSample.y);
  const vec3 pinholeDirection = normalize(pixelPoint.xyz - parameters.primaryPathOrigin.xyz);
  const float t = parameters.primaryPathLensParameters.x /
                  dot(pinholeDirection, normalize(parameters.primaryPathForward.xyz));
  const vec3 focalPoint = parameters.primaryPathOrigin.xyz + pinholeDirection * t;
  const vec4 lensOrigin = parameters.primaryPathOrigin +
                          parameters.primaryPathLensRight * lensSample.x +
                          parameters.primaryPathLensUp * lensSample.y;

  GpuDiffusePathStateRecord path;
  path.ray.origin = lensOrigin;
  path.ray.direction = vec4(normalize(focalPoint - lensOrigin.xyz), 0.0);
  path.ray.minDistance = 0.0;
  path.ray.maxDistance = uintBitsToFloat(0x7f800000u);
  path.ray.timeSample = timeSample;
  path.ray.flags = 0u;
  path.ray.rayIndex = pathIndex;
  path.ray.reserved0 = 0u;
  path.ray.reserved1 = 0u;
  path.ray.reserved2 = 0u;
  path.throughput = vec4(1.0, 1.0, 1.0, 0.0);
  path.accumulatedRadiance = vec4(0.0);
  path.pixelIndex = pixelIndex;
  path.primarySampleIndex = sampleIndex;
  path.depth = 0u;
  path.sampleSeed = parameters.primaryPathSampleSeed;
  path.sampleDimensionBase = gpuPrimaryPathSampleDimensionBase;
  path.sampleDimensionStride = gpuPrimaryPathSampleDimensionStride;
  path.flags = gpuDiffusePathStateActiveFlag;
  path.reserved0 = 0u;
  path.previousBsdfPdf = 0.0;
  path.previousLightPdf = 0.0;
  path.previousMaterial = 0u;
  path.previousEventFlags = 0u;
  path.reserved1 = 0u;
  path.reserved2 = 0u;
  path.reserved3 = 0u;
  path.reserved4 = 0u;
  return path;
}

GpuDiffusePathStateRecord makeEquirectangularPrimaryPath(uint pathIndex) {
  const uint sampleIndex = pathIndex % parameters.primaryPathSamplesPerPixel;
  const uint pixelOrdinal = pathIndex / parameters.primaryPathSamplesPerPixel;
  const uint localX = pixelOrdinal % parameters.primaryPathActualWidth;
  const uint localY = pixelOrdinal / parameters.primaryPathActualWidth;
  const int column = parameters.primaryPathActualLeft + int(localX);
  const int row = parameters.primaryPathActualTop + int(localY);
  const uint pixelIndex =
    uint((row - parameters.primaryPathRequestedTop) * int(parameters.primaryPathRequestedWidth) +
         (column - parameters.primaryPathRequestedLeft));
  const vec2 pixelSample =
    sample2DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 0u);
  const float timeSample =
    sample1DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 1u, 0u);
  const float x = float(column) + pixelSample.x;
  const float y = float(row) + pixelSample.y;
  const float lon = (2.0 * x / parameters.primaryPathLensParameters.x - 1.0) * (pathLoopTau * 0.5);
  const float lat = (1.0 - 2.0 * y / parameters.primaryPathLensParameters.y) * (pathLoopTau * 0.25);
  const float cosLat = cos(lat);
  const vec3 local = vec3(cosLat * sin(lon), -sin(lat), cosLat * cos(lon));
  const vec3 direction =
    normalize(parameters.primaryPathRight.xyz * local.x + parameters.primaryPathDown.xyz * local.y +
              parameters.primaryPathForward.xyz * local.z);

  GpuDiffusePathStateRecord path;
  path.ray.origin = parameters.primaryPathOrigin;
  path.ray.direction = vec4(direction, 0.0);
  path.ray.minDistance = 0.0;
  path.ray.maxDistance = uintBitsToFloat(0x7f800000u);
  path.ray.timeSample = timeSample;
  path.ray.flags = 0u;
  path.ray.rayIndex = pathIndex;
  path.ray.reserved0 = 0u;
  path.ray.reserved1 = 0u;
  path.ray.reserved2 = 0u;
  path.throughput = vec4(1.0, 1.0, 1.0, 0.0);
  path.accumulatedRadiance = vec4(0.0);
  path.pixelIndex = pixelIndex;
  path.primarySampleIndex = sampleIndex;
  path.depth = 0u;
  path.sampleSeed = parameters.primaryPathSampleSeed;
  path.sampleDimensionBase = gpuPrimaryPathSampleDimensionBase;
  path.sampleDimensionStride = gpuPrimaryPathSampleDimensionStride;
  path.flags = gpuDiffusePathStateActiveFlag;
  path.reserved0 = 0u;
  path.previousBsdfPdf = 0.0;
  path.previousLightPdf = 0.0;
  path.previousMaterial = 0u;
  path.previousEventFlags = 0u;
  path.reserved1 = 0u;
  path.reserved2 = 0u;
  path.reserved3 = 0u;
  path.reserved4 = 0u;
  return path;
}

GpuDiffusePathStateRecord makeSphericalPrimaryPath(uint pathIndex) {
  const uint sampleIndex = pathIndex % parameters.primaryPathSamplesPerPixel;
  const uint pixelOrdinal = pathIndex / parameters.primaryPathSamplesPerPixel;
  const uint localX = pixelOrdinal % parameters.primaryPathActualWidth;
  const uint localY = pixelOrdinal / parameters.primaryPathActualWidth;
  const int column = parameters.primaryPathActualLeft + int(localX);
  const int row = parameters.primaryPathActualTop + int(localY);
  const uint pixelIndex =
    uint((row - parameters.primaryPathRequestedTop) * int(parameters.primaryPathRequestedWidth) +
         (column - parameters.primaryPathRequestedLeft));
  const vec2 pixelSample =
    sample2DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 0u);
  const float timeSample =
    sample1DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 1u, 0u);
  const float x = float(column) + pixelSample.x;
  const float y = float(row) + pixelSample.y;
  const float pointX = 2.0 / parameters.primaryPathLensParameters.x * x + 1.0;
  const float pointY = 2.0 / parameters.primaryPathLensParameters.y * y - 1.0;
  const float lambda = pointX * 0.5 * parameters.primaryPathLensParameters.z;
  const float psi = pointY * 0.5 * parameters.primaryPathLensParameters.w;
  const float phi = (pathLoopTau * 0.5) - lambda;
  const float theta = (pathLoopTau * 0.25) - psi;
  const float sinPhi = sin(phi);
  const float cosPhi = cos(phi);
  const float sinTheta = sin(theta);
  const float cosTheta = cos(theta);
  const vec3 local = vec3(sinTheta * sinPhi, cosTheta, sinTheta * cosPhi);
  const vec3 direction =
    normalize(parameters.primaryPathRight.xyz * local.x + parameters.primaryPathDown.xyz * local.y +
              parameters.primaryPathForward.xyz * local.z);

  GpuDiffusePathStateRecord path;
  path.ray.origin = parameters.primaryPathOrigin;
  path.ray.direction = vec4(direction, 0.0);
  path.ray.minDistance = 0.0;
  path.ray.maxDistance = uintBitsToFloat(0x7f800000u);
  path.ray.timeSample = timeSample;
  path.ray.flags = 0u;
  path.ray.rayIndex = pathIndex;
  path.ray.reserved0 = 0u;
  path.ray.reserved1 = 0u;
  path.ray.reserved2 = 0u;
  path.throughput = vec4(1.0, 1.0, 1.0, 0.0);
  path.accumulatedRadiance = vec4(0.0);
  path.pixelIndex = pixelIndex;
  path.primarySampleIndex = sampleIndex;
  path.depth = 0u;
  path.sampleSeed = parameters.primaryPathSampleSeed;
  path.sampleDimensionBase = gpuPrimaryPathSampleDimensionBase;
  path.sampleDimensionStride = gpuPrimaryPathSampleDimensionStride;
  path.flags = gpuDiffusePathStateActiveFlag;
  path.reserved0 = 0u;
  path.previousBsdfPdf = 0.0;
  path.previousLightPdf = 0.0;
  path.previousMaterial = 0u;
  path.previousEventFlags = 0u;
  path.reserved1 = 0u;
  path.reserved2 = 0u;
  path.reserved3 = 0u;
  path.reserved4 = 0u;
  return path;
}

GpuDiffusePathStateRecord makeFishEyePrimaryPath(uint pathIndex) {
  const uint sampleIndex = pathIndex % parameters.primaryPathSamplesPerPixel;
  const uint pixelOrdinal = pathIndex / parameters.primaryPathSamplesPerPixel;
  const uint localX = pixelOrdinal % parameters.primaryPathActualWidth;
  const uint localY = pixelOrdinal / parameters.primaryPathActualWidth;
  const int column = parameters.primaryPathActualLeft + int(localX);
  const int row = parameters.primaryPathActualTop + int(localY);
  const uint pixelIndex =
    uint((row - parameters.primaryPathRequestedTop) * int(parameters.primaryPathRequestedWidth) +
         (column - parameters.primaryPathRequestedLeft));
  const vec2 pixelSample =
    sample2DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 0u);
  const float timeSample =
    sample1DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 1u, 0u);
  const float x = float(column) + pixelSample.x;
  const float y = float(row) + pixelSample.y;
  const vec2 point = vec2(2.0 / parameters.primaryPathLensParameters.x * x - 1.0,
                          2.0 / parameters.primaryPathLensParameters.y * y - 1.0);
  const float r2 = dot(point, point);
  const bool valid = r2 <= 1.0 && r2 > 0.0;
  vec3 direction = normalize(parameters.primaryPathForward.xyz);
  if (valid) {
    const float r = sqrt(r2);
    const float psi = r * parameters.primaryPathLensParameters.z * 0.5;
    const float sinPsi = sin(psi);
    const float cosPsi = cos(psi);
    const float sinAlpha = point.y / r;
    const float cosAlpha = point.x / r;
    const vec3 local = vec3(sinPsi * cosAlpha, sinPsi * sinAlpha, cosPsi);
    direction =
      normalize(parameters.primaryPathRight.xyz * local.x + parameters.primaryPathDown.xyz * local.y +
                parameters.primaryPathForward.xyz * local.z);
  }

  GpuDiffusePathStateRecord path;
  path.ray.origin = parameters.primaryPathOrigin;
  path.ray.direction = vec4(direction, 0.0);
  path.ray.minDistance = 0.0;
  path.ray.maxDistance = uintBitsToFloat(0x7f800000u);
  path.ray.timeSample = timeSample;
  path.ray.flags = 0u;
  path.ray.rayIndex = pathIndex;
  path.ray.reserved0 = 0u;
  path.ray.reserved1 = 0u;
  path.ray.reserved2 = 0u;
  path.throughput = vec4(1.0, 1.0, 1.0, 0.0);
  path.accumulatedRadiance = vec4(0.0);
  path.pixelIndex = pixelIndex;
  path.primarySampleIndex = sampleIndex;
  path.depth = 0u;
  path.sampleSeed = parameters.primaryPathSampleSeed;
  path.sampleDimensionBase = gpuPrimaryPathSampleDimensionBase;
  path.sampleDimensionStride = gpuPrimaryPathSampleDimensionStride;
  path.flags = valid ? gpuDiffusePathStateActiveFlag : gpuDiffusePathStateTerminatedFlag;
  path.reserved0 = 0u;
  path.previousBsdfPdf = 0.0;
  path.previousLightPdf = 0.0;
  path.previousMaterial = 0u;
  path.previousEventFlags = 0u;
  path.reserved1 = 0u;
  path.reserved2 = 0u;
  path.reserved3 = 0u;
  path.reserved4 = 0u;
  return path;
}

GpuDiffusePathStateRecord makeTiltShiftPrimaryPath(uint pathIndex) {
  const uint sampleIndex = pathIndex % parameters.primaryPathSamplesPerPixel;
  const uint pixelOrdinal = pathIndex / parameters.primaryPathSamplesPerPixel;
  const uint localX = pixelOrdinal % parameters.primaryPathActualWidth;
  const uint localY = pixelOrdinal / parameters.primaryPathActualWidth;
  const int column = parameters.primaryPathActualLeft + int(localX);
  const int row = parameters.primaryPathActualTop + int(localY);
  const uint pixelIndex =
    uint((row - parameters.primaryPathRequestedTop) * int(parameters.primaryPathRequestedWidth) +
         (column - parameters.primaryPathRequestedLeft));
  const vec2 pixelSample =
    sample2DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 0u);
  const float timeSample =
    sample1DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 1u, 0u);
  const vec2 lensSample = concentricMapToDisc(
    sample2DCoordinate(parameters.primaryPathSampleSeed, pixelIndex, sampleIndex, 2u));
  const vec4 pixelPoint = parameters.primaryPathTopLeft +
                          parameters.primaryPathRight * (float(column) + pixelSample.x) +
                          parameters.primaryPathDown * (float(row) + pixelSample.y);
  const vec3 rightBasis = normalize(parameters.primaryPathRight.xyz);
  const vec3 upBasis = normalize(parameters.primaryPathDown.xyz);
  const vec3 focalForward = normalize(parameters.primaryPathForward.xyz);
  const vec3 shiftedPixelPoint =
    pixelPoint.xyz + rightBasis * parameters.primaryPathLensParameters.y +
    upBasis * parameters.primaryPathLensParameters.z;
  const vec3 pinholeDirection = normalize(shiftedPixelPoint - parameters.primaryPathOrigin.xyz);
  const float tilt = parameters.primaryPathLensParameters.w;
  const vec3 tiltedNormal =
    focalForward * cos(tilt) + cross(rightBasis, focalForward) * sin(tilt);
  const float denominator = dot(pinholeDirection, tiltedNormal);
  const bool valid = abs(denominator) > 1.0e-7;
  vec3 focalPoint = parameters.primaryPathOrigin.xyz;
  if (valid) {
    const float numerator = parameters.primaryPathLensParameters.x * dot(focalForward, tiltedNormal);
    focalPoint = parameters.primaryPathOrigin.xyz + pinholeDirection * (numerator / denominator);
  }
  const vec4 lensOrigin = parameters.primaryPathOrigin +
                          parameters.primaryPathLensRight * lensSample.x +
                          parameters.primaryPathLensUp * lensSample.y;
  const vec3 rayDirection = valid ? normalize(focalPoint - lensOrigin.xyz) : focalForward;

  GpuDiffusePathStateRecord path;
  path.ray.origin = lensOrigin;
  path.ray.direction = vec4(rayDirection, 0.0);
  path.ray.minDistance = 0.0;
  path.ray.maxDistance = uintBitsToFloat(0x7f800000u);
  path.ray.timeSample = timeSample;
  path.ray.flags = 0u;
  path.ray.rayIndex = pathIndex;
  path.ray.reserved0 = 0u;
  path.ray.reserved1 = 0u;
  path.ray.reserved2 = 0u;
  path.throughput = vec4(1.0, 1.0, 1.0, 0.0);
  path.accumulatedRadiance = vec4(0.0);
  path.pixelIndex = pixelIndex;
  path.primarySampleIndex = sampleIndex;
  path.depth = 0u;
  path.sampleSeed = parameters.primaryPathSampleSeed;
  path.sampleDimensionBase = gpuPrimaryPathSampleDimensionBase;
  path.sampleDimensionStride = gpuPrimaryPathSampleDimensionStride;
  path.flags = valid ? gpuDiffusePathStateActiveFlag : gpuDiffusePathStateTerminatedFlag;
  path.reserved0 = 0u;
  path.previousBsdfPdf = 0.0;
  path.previousLightPdf = 0.0;
  path.previousMaterial = 0u;
  path.previousEventFlags = 0u;
  path.reserved1 = 0u;
  path.reserved2 = 0u;
  path.reserved3 = 0u;
  path.reserved4 = 0u;
  return path;
}

GpuDiffusePathStateRecord initialPathFor(uint pathIndex) {
  if (parameters.primaryPathGenerationMode == gpuPrimaryPathGenerationModePinhole) {
    return makePinholePrimaryPath(pathIndex);
  }
  if (parameters.primaryPathGenerationMode == gpuPrimaryPathGenerationModeOrthographic) {
    return makeOrthographicPrimaryPath(pathIndex);
  }
  if (parameters.primaryPathGenerationMode == gpuPrimaryPathGenerationModeThinLens) {
    return makeThinLensPrimaryPath(pathIndex);
  }
  if (parameters.primaryPathGenerationMode == gpuPrimaryPathGenerationModeEquirectangular) {
    return makeEquirectangularPrimaryPath(pathIndex);
  }
  if (parameters.primaryPathGenerationMode == gpuPrimaryPathGenerationModeSpherical) {
    return makeSphericalPrimaryPath(pathIndex);
  }
  if (parameters.primaryPathGenerationMode == gpuPrimaryPathGenerationModeFishEye) {
    return makeFishEyePrimaryPath(pathIndex);
  }
  if (parameters.primaryPathGenerationMode == gpuPrimaryPathGenerationModeTiltShift) {
    return makeTiltShiftPrimaryPath(pathIndex);
  }
  return initialPathStates[pathIndex];
}

uint lightSelectionSampleIndex(uint bounce, uint directSampleIndex) {
  const uint sum = bounce + directSampleIndex;
  return sum * (sum + 1u) / 2u + directSampleIndex;
}

uint lightSampleIndex(uint bounce, uint lightIndex, uint directSampleIndex) {
  const uint effectiveBounce = lightSelectionSampleIndex(bounce, directSampleIndex);
  const uint sum = effectiveBounce + lightIndex;
  return sum * (sum + 1u) / 2u + lightIndex;
}

uint lightSelectionDimension(GpuDiffusePathStateRecord path, uint directSampleIndex) {
  return 5u + lightSelectionSampleIndex(path.depth, directSampleIndex) * 4u;
}

uint lightSurfaceDimension(GpuDiffusePathStateRecord path, uint lightIndex,
                           uint directSampleIndex) {
  return 4u + lightSampleIndex(path.depth, lightIndex, directSampleIndex) * 4u;
}

float maxColor(vec4 color) {
  return max(max(color.x, color.y), color.z);
}

vec3 tangentFor(vec3 normal) {
  const vec3 helper = abs(normal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) :
                                               vec3(1.0, 0.0, 0.0);
  return normalize(cross(helper, normal));
}

vec3 cosineHemisphereDirection(vec3 normal, vec2 sample) {
  const float u0 = clamp(sample.x, 0.0, 1.0);
  const float u1 = clamp(sample.y, 0.0, 1.0);
  const float r = sqrt(u0);
  const float phi = pathLoopTau * u1;
  const float x = r * cos(phi);
  const float y = r * sin(phi);
  const float z = sqrt(max(0.0, 1.0 - u0));
  const vec3 tangent = tangentFor(normal);
  const vec3 bitangent = cross(normal, tangent);
  return normalize(tangent * x + bitangent * y + normal * z);
}

vec3 phongLobeDirection(vec3 axis, vec2 sample, float exponent) {
  const float u0 = clamp(sample.x, 0.0, 1.0);
  const float u1 = clamp(sample.y, 0.0, 1.0);
  const float cosTheta = pow(u0, 1.0 / (exponent + 1.0));
  const float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
  const float phi = pathLoopTau * u1;
  const vec3 tangent = tangentFor(axis);
  const vec3 bitangent = cross(axis, tangent);
  return normalize(tangent * (sinTheta * cos(phi)) +
                   bitangent * (sinTheta * sin(phi)) + axis * cosTheta);
}

float cosineHemispherePdf(vec3 normal, vec3 direction) {
  const float normalDotDirection = dot(normal, direction);
  return normalDotDirection <= 0.0 ? 0.0 : normalDotDirection * pathLoopInvPi;
}

float continuationProbability(vec4 throughput) {
  const float maximum = maxColor(throughput);
  if (maximum <= 0.0) {
    return 0.0;
  }
  return clamp(maximum, pathLoopMinimumContinuationProbability, 1.0);
}

bool throughputIsBlack(vec4 throughput) {
  return throughput.x <= 0.0 && throughput.y <= 0.0 && throughput.z <= 0.0;
}

GpuIntersectionBounds readBounds(uint wordOffset) {
  GpuIntersectionBounds bounds;
  bounds.minimum = readVec4(wordOffset);
  bounds.maximum = readVec4(wordOffset + 4u);
  return bounds;
}

GpuIntersectionBvhNode readBvhNode(uint index) {
  const uint wordOffset = byteOffsetWords(parameters.bvhByteOffset) + index * 12u;
  GpuIntersectionBvhNode node;
  node.bounds = readBounds(wordOffset);
  node.leftOrFirstPrimitive = sceneWords[wordOffset + 8u];
  node.primitiveCount = sceneWords[wordOffset + 9u];
  node.flags = sceneWords[wordOffset + 10u];
  node.reserved = sceneWords[wordOffset + 11u];
  return node;
}

GpuIntersectionPrimitiveRecord readPrimitive(uint index) {
  const uint wordOffset = byteOffsetWords(parameters.primitiveByteOffset) + index * 16u;
  GpuIntersectionPrimitiveRecord primitive;
  primitive.bounds = readBounds(wordOffset);
  primitive.kind = sceneWords[wordOffset + 8u];
  primitive.material = sceneWords[wordOffset + 9u];
  primitive.object = sceneWords[wordOffset + 10u];
  primitive.transform = sceneWords[wordOffset + 11u];
  primitive.payloadOffset = sceneWords[wordOffset + 12u];
  primitive.payloadCount = sceneWords[wordOffset + 13u];
  primitive.reserved0 = sceneWords[wordOffset + 14u];
  primitive.reserved1 = sceneWords[wordOffset + 15u];
  return primitive;
}

GpuIntersectionTrianglePayload readTriangle(uint index) {
  const uint wordOffset = byteOffsetWords(parameters.triangleByteOffset) + index * 40u;
  GpuIntersectionTrianglePayload triangle;
  triangle.point0 = readVec4(wordOffset);
  triangle.point1 = readVec4(wordOffset + 4u);
  triangle.point2 = readVec4(wordOffset + 8u);
  triangle.normal0 = readVec4(wordOffset + 12u);
  triangle.normal1 = readVec4(wordOffset + 16u);
  triangle.normal2 = readVec4(wordOffset + 20u);
  triangle.uv0 = readVec4(wordOffset + 24u);
  triangle.uv1 = readVec4(wordOffset + 28u);
  triangle.uv2 = readVec4(wordOffset + 32u);
  triangle.minimumHitDistance = readVec4(wordOffset + 36u);
  return triangle;
}

GpuIntersectionSpherePayload readSphere(uint index) {
  const uint wordOffset = byteOffsetWords(parameters.sphereByteOffset) + index * 4u;
  GpuIntersectionSpherePayload sphere;
  sphere.centerRadius = readVec4(wordOffset);
  return sphere;
}

GpuIntersectionPlanePayload readPlane(uint index) {
  const uint wordOffset = byteOffsetWords(parameters.planeByteOffset) + index * 4u;
  GpuIntersectionPlanePayload plane;
  plane.normalDistance = readVec4(wordOffset);
  return plane;
}

GpuIntersectionRectanglePayload readRectangle(uint index) {
  const uint wordOffset = byteOffsetWords(parameters.rectangleByteOffset) + index * 16u;
  GpuIntersectionRectanglePayload rectangle;
  rectangle.corner = readVec4(wordOffset);
  rectangle.leg1 = readVec4(wordOffset + 4u);
  rectangle.leg2 = readVec4(wordOffset + 8u);
  rectangle.normal = readVec4(wordOffset + 12u);
  return rectangle;
}

GpuIntersectionDiskPayload readDisk(uint index) {
  const uint wordOffset = byteOffsetWords(parameters.diskByteOffset) + index * 8u;
  GpuIntersectionDiskPayload disk;
  disk.centerRadius = readVec4(wordOffset);
  disk.normalMinimumHitDistance = readVec4(wordOffset + 4u);
  return disk;
}

GpuIntersectionOpenCylinderPayload readOpenCylinder(uint index) {
  const uint wordOffset = byteOffsetWords(parameters.openCylinderByteOffset) + index * 4u;
  GpuIntersectionOpenCylinderPayload openCylinder;
  openCylinder.radiusHalfHeight = readVec4(wordOffset);
  return openCylinder;
}

GpuIntersectionTorusPayload readTorus(uint index) {
  const uint wordOffset = byteOffsetWords(parameters.torusByteOffset) + index * 4u;
  GpuIntersectionTorusPayload torus;
  torus.sweptTubeRadius = readVec4(wordOffset);
  return torus;
}

GpuIntersectionTransformPayload readTransform(uint index) {
  const uint wordOffset = byteOffsetWords(parameters.transformByteOffset) + index * 64u;
  GpuIntersectionTransformPayload transform;
  transform.pointMatrix0 = readVec4(wordOffset);
  transform.pointMatrix1 = readVec4(wordOffset + 4u);
  transform.pointMatrix2 = readVec4(wordOffset + 8u);
  transform.pointMatrix3 = readVec4(wordOffset + 12u);
  transform.normalMatrix0 = readVec4(wordOffset + 16u);
  transform.normalMatrix1 = readVec4(wordOffset + 20u);
  transform.normalMatrix2 = readVec4(wordOffset + 24u);
  transform.normalMatrix3 = readVec4(wordOffset + 28u);
  transform.inversePointMatrix0 = readVec4(wordOffset + 32u);
  transform.inversePointMatrix1 = readVec4(wordOffset + 36u);
  transform.inversePointMatrix2 = readVec4(wordOffset + 40u);
  transform.inversePointMatrix3 = readVec4(wordOffset + 44u);
  transform.inverseDirectionMatrix0 = readVec4(wordOffset + 48u);
  transform.inverseDirectionMatrix1 = readVec4(wordOffset + 52u);
  transform.inverseDirectionMatrix2 = readVec4(wordOffset + 56u);
  transform.inverseDirectionMatrix3 = readVec4(wordOffset + 60u);
  return transform;
}

GpuTracingMaterialRecord readMaterial(uint materialId) {
  const uint wordOffset = byteOffsetWords(parameters.materialByteOffset) + materialId * 48u;
  GpuTracingMaterialRecord material;
  material.kind = sceneWords[wordOffset + 0u];
  material.albedoTexture = sceneWords[wordOffset + 1u];
  material.emissionTexture = sceneWords[wordOffset + 2u];
  material.flags = sceneWords[wordOffset + 3u];
  material.parameters = readVec4(wordOffset + 4u);
  material.specularParameters = readVec4(wordOffset + 8u);
  material.continuationParameters = readVec4(wordOffset + 12u);
  material.transmissionParameters = readVec4(wordOffset + 16u);
  material.portalOriginMatrix0 = readVec4(wordOffset + 20u);
  material.portalOriginMatrix1 = readVec4(wordOffset + 24u);
  material.portalOriginMatrix2 = readVec4(wordOffset + 28u);
  material.portalOriginMatrix3 = readVec4(wordOffset + 32u);
  material.portalDirectionMatrix0 = readVec4(wordOffset + 36u);
  material.portalDirectionMatrix1 = readVec4(wordOffset + 40u);
  material.portalDirectionMatrix2 = readVec4(wordOffset + 44u);
  return material;
}

GpuTracingTextureRecord readTexture(uint textureId) {
  const uint wordOffset = byteOffsetWords(parameters.textureByteOffset) + textureId * 8u;
  GpuTracingTextureRecord texture;
  texture.kind = sceneWords[wordOffset + 0u];
  texture.payloadOffset = sceneWords[wordOffset + 1u];
  texture.payloadCount = sceneWords[wordOffset + 2u];
  texture.flags = sceneWords[wordOffset + 3u];
  texture.parameters = readVec4(wordOffset + 4u);
  return texture;
}

GpuTracingLightRecord readLight(uint lightIndex) {
  const uint wordOffset = byteOffsetWords(parameters.lightByteOffset) + lightIndex * 20u;
  GpuTracingLightRecord light;
  light.kind = sceneWords[wordOffset + 0u];
  light.emissionTexture = sceneWords[wordOffset + 1u];
  light.flags = sceneWords[wordOffset + 2u];
  light.object = sceneWords[wordOffset + 3u];
  light.positionOrDirection = readVec4(wordOffset + 4u);
  light.u = readVec4(wordOffset + 8u);
  light.v = readVec4(wordOffset + 12u);
  light.parameters = readVec4(wordOffset + 16u);
  return light;
}

bool boundsIntersectsRay(GpuIntersectionBounds bounds, GpuIntersectionRay ray,
                         float maxHitDistance) {
  float enter = ray.minDistance;
  float exit = min(ray.maxDistance, maxHitDistance);
  if (exit < enter) {
    return false;
  }
  for (uint axis = 0u; axis != 3u; ++axis) {
    const float origin = ray.origin[axis];
    const float direction = ray.direction[axis];
    const float minimum = bounds.minimum[axis];
    const float maximum = bounds.maximum[axis];
    if (abs(direction) <= 1.1920928955078125e-7) {
      if (origin < minimum || origin > maximum) {
        return false;
      }
      continue;
    }
    const float inverseDirection = 1.0 / direction;
    float nearDistance = (minimum - origin) * inverseDirection;
    float farDistance = (maximum - origin) * inverseDirection;
    if (nearDistance > farDistance) {
      const float temporary = nearDistance;
      nearDistance = farDistance;
      farDistance = temporary;
    }
    enter = max(enter, nearDistance);
    exit = min(exit, farDistance);
    if (exit < enter) {
      return false;
    }
  }
  return true;
}

vec4 normalize3(vec4 value) {
  const float lengthSquared = dot(value.xyz, value.xyz);
  if (lengthSquared <= pathLoopRayEpsilon) {
    return vec4(0.0);
  }
  return vec4(value.xyz * inversesqrt(lengthSquared), 0.0);
}

vec4 transformPoint(vec4 row0, vec4 row1, vec4 row2, vec4 row3, vec4 point) {
  return vec4(dot(row0, point), dot(row1, point), dot(row2, point), dot(row3, point));
}

vec4 transformDirection(vec4 row0, vec4 row1, vec4 row2, vec4 direction) {
  return vec4(dot(row0.xyz, direction.xyz), dot(row1.xyz, direction.xyz),
              dot(row2.xyz, direction.xyz), 0.0);
}

GpuIntersectionRay transformRay(GpuIntersectionRay ray,
                                GpuIntersectionTransformPayload transform) {
  GpuIntersectionRay result = ray;
  result.origin = transformPoint(transform.inversePointMatrix0,
                                 transform.inversePointMatrix1,
                                 transform.inversePointMatrix2,
                                 transform.inversePointMatrix3,
                                 ray.origin);
  result.direction = transformDirection(transform.inverseDirectionMatrix0,
                                        transform.inverseDirectionMatrix1,
                                        transform.inverseDirectionMatrix2,
                                        ray.direction);
  return result;
}

LocalPrimitiveHit transformHit(LocalPrimitiveHit hit,
                               GpuIntersectionTransformPayload transform) {
  if (!hit.hit) {
    return hit;
  }
  hit.point = transformPoint(transform.pointMatrix0, transform.pointMatrix1,
                             transform.pointMatrix2, transform.pointMatrix3, hit.point);
  hit.normal = normalize3(transformDirection(transform.normalMatrix0, transform.normalMatrix1,
                                             transform.normalMatrix2, hit.normal));
  return hit;
}

LocalPrimitiveHit emptyPrimitiveHit(float maxHitDistance) {
  LocalPrimitiveHit result;
  result.hit = false;
  result.distance = maxHitDistance;
  result.point = vec4(0.0);
  result.normal = vec4(0.0);
  result.uv = vec4(0.0);
  result.barycentric = vec4(0.0);
  return result;
}

LocalPrimitiveHit intersectTriangle(GpuIntersectionRay ray, GpuIntersectionTrianglePayload triangle,
                                    float maxHitDistance) {
  LocalPrimitiveHit result = emptyPrimitiveHit(maxHitDistance);
  const float a = triangle.point0.x - triangle.point1.x;
  const float b = triangle.point0.x - triangle.point2.x;
  const float c = ray.direction.x;
  const float d = triangle.point0.x - ray.origin.x;
  const float e = triangle.point0.y - triangle.point1.y;
  const float f = triangle.point0.y - triangle.point2.y;
  const float g = ray.direction.y;
  const float h = triangle.point0.y - ray.origin.y;
  const float i = triangle.point0.z - triangle.point1.z;
  const float j = triangle.point0.z - triangle.point2.z;
  const float k = ray.direction.z;
  const float l = triangle.point0.z - ray.origin.z;
  const float m = f * k - g * j;
  const float n = h * k - g * l;
  const float p = f * l - h * j;
  const float q = g * i - e * k;
  const float r = e * l - h * i;
  const float s = e * j - f * i;
  const float denominator = a * m + b * q + c * s;
  if (denominator == 0.0) {
    return result;
  }
  const float inverseDenominator = 1.0 / denominator;
  const float beta = (d * m - b * n - c * p) * inverseDenominator;
  if (beta < 0.0 || beta > 1.0) {
    return result;
  }
  const float gamma = (a * n + d * q + c * r) * inverseDenominator;
  if (gamma < 0.0 || gamma > 1.0 || beta + gamma > 1.0) {
    return result;
  }
  const float distance = (a * p - b * r + d * s) * inverseDenominator;
  const float minimumDistance = max(ray.minDistance, triangle.minimumHitDistance.x);
  if (distance < minimumDistance || distance > ray.maxDistance ||
      distance >= maxHitDistance) {
    return result;
  }
  const float alpha = 1.0 - beta - gamma;
  result.hit = true;
  result.distance = distance;
  result.point = vec4(ray.origin.xyz + ray.direction.xyz * distance, 1.0);
  result.normal = vec4(normalize(triangle.normal0.xyz * alpha +
                                 triangle.normal1.xyz * beta +
                                 triangle.normal2.xyz * gamma), 0.0);
  result.uv = triangle.uv0 * alpha + triangle.uv1 * beta + triangle.uv2 * gamma;
  result.barycentric = vec4(alpha, beta, gamma, 0.0);
  return result;
}

LocalPrimitiveHit intersectSphere(GpuIntersectionRay ray, GpuIntersectionSpherePayload sphere,
                                  float maxHitDistance) {
  LocalPrimitiveHit result = emptyPrimitiveHit(maxHitDistance);
  const vec3 center = sphere.centerRadius.xyz;
  const float radius = sphere.centerRadius.w;
  const vec3 origin = ray.origin.xyz - center;
  const vec3 direction = ray.direction.xyz;
  const float od = dot(origin, direction);
  const float dd = dot(direction, direction);
  if (dd <= 1.1920928955078125e-7) {
    return result;
  }
  const float discriminant = od * od - dd * (dot(origin, origin) - radius * radius);
  if (discriminant <= 0.0) {
    return result;
  }
  const float root = sqrt(discriminant);
  const float nearDistance = (-od - root) / dd;
  const float farDistance = (-od + root) / dd;
  if (nearDistance <= 0.0 && farDistance <= 0.0) {
    return result;
  }
  const float distance = nearDistance >= ray.minDistance ? nearDistance : farDistance;
  if (distance < ray.minDistance || distance > ray.maxDistance || distance >= maxHitDistance) {
    return result;
  }
  result.hit = true;
  result.distance = distance;
  const vec3 point = ray.origin.xyz + ray.direction.xyz * distance;
  result.point = vec4(point, 1.0);
  result.normal = vec4(normalize(point - center), 0.0);
  return result;
}

LocalPrimitiveHit intersectPlane(GpuIntersectionRay ray, GpuIntersectionPlanePayload plane,
                                 float maxHitDistance) {
  LocalPrimitiveHit result = emptyPrimitiveHit(maxHitDistance);
  const vec3 normal = plane.normalDistance.xyz;
  const float planeDistance = plane.normalDistance.w;
  const float angle = dot(normal, ray.direction.xyz);
  if (abs(angle) <= 1.1920928955078125e-7) {
    return result;
  }
  const float distance = -(dot(normal, ray.origin.xyz) + planeDistance) / angle;
  if (distance <= 0.0 || distance < ray.minDistance || distance > ray.maxDistance ||
      distance >= maxHitDistance) {
    return result;
  }
  result.hit = true;
  result.distance = distance;
  result.point = vec4(ray.origin.xyz + ray.direction.xyz * distance, 1.0);
  result.normal = vec4(normal, 0.0);
  return result;
}

LocalPrimitiveHit intersectRectangle(GpuIntersectionRay ray,
                                     GpuIntersectionRectanglePayload rectangle,
                                     float maxHitDistance) {
  LocalPrimitiveHit result = emptyPrimitiveHit(maxHitDistance);
  const vec3 normal = rectangle.normal.xyz;
  const float denominator = dot(ray.direction.xyz, normal);
  if (denominator == 0.0) {
    return result;
  }
  const float distance = dot(rectangle.corner.xyz - ray.origin.xyz, normal) / denominator;
  if (!finiteFloat(distance) || distance < 0.0 || distance < ray.minDistance ||
      distance > ray.maxDistance || distance >= maxHitDistance) {
    return result;
  }
  const vec3 point = ray.origin.xyz + ray.direction.xyz * distance;
  const vec3 difference = point - rectangle.corner.xyz;
  const float dot1 = dot(difference, rectangle.leg1.xyz);
  const float squaredLength1 = dot(rectangle.leg1.xyz, rectangle.leg1.xyz);
  if (dot1 < 0.0 || dot1 > squaredLength1) {
    return result;
  }
  const float dot2 = dot(difference, rectangle.leg2.xyz);
  const float squaredLength2 = dot(rectangle.leg2.xyz, rectangle.leg2.xyz);
  if (dot2 < 0.0 || dot2 > squaredLength2) {
    return result;
  }
  result.hit = true;
  result.distance = distance;
  result.point = vec4(point, 1.0);
  result.normal = vec4(normal, 0.0);
  return result;
}

LocalPrimitiveHit intersectDisk(GpuIntersectionRay ray, GpuIntersectionDiskPayload disk,
                                float maxHitDistance) {
  LocalPrimitiveHit result = emptyPrimitiveHit(maxHitDistance);
  const vec3 center = disk.centerRadius.xyz;
  const float radius = disk.centerRadius.w;
  const vec3 normal = disk.normalMinimumHitDistance.xyz;
  const float minimumHitDistance = disk.normalMinimumHitDistance.w;
  const float denominator = dot(ray.direction.xyz, normal);
  if (denominator == 0.0) {
    return result;
  }
  const float distance = dot(center - ray.origin.xyz, normal) / denominator;
  if (!finiteFloat(distance) || distance < minimumHitDistance || distance < ray.minDistance ||
      distance > ray.maxDistance || distance >= maxHitDistance) {
    return result;
  }
  const vec3 point = ray.origin.xyz + ray.direction.xyz * distance;
  const vec3 hitOffset = point - center;
  if (!(dot(hitOffset, hitOffset) < radius * radius)) {
    return result;
  }
  result.hit = true;
  result.distance = distance;
  result.point = vec4(point, 1.0);
  result.normal = vec4(normal, 0.0);
  return result;
}

LocalPrimitiveHit intersectOpenCylinder(GpuIntersectionRay ray,
                                        GpuIntersectionOpenCylinderPayload openCylinder,
                                        float maxHitDistance) {
  LocalPrimitiveHit result = emptyPrimitiveHit(maxHitDistance);
  const float radius = openCylinder.radiusHalfHeight.x;
  const float halfHeight = openCylinder.radiusHalfHeight.y;
  const float inverseRadius = openCylinder.radiusHalfHeight.z;
  const float a = ray.direction.x * ray.direction.x + ray.direction.z * ray.direction.z;
  if (abs(a) <= pathLoopRayEpsilon) {
    return result;
  }
  const float b = 2.0 * (ray.origin.x * ray.direction.x +
                         ray.origin.z * ray.direction.z);
  const float c = ray.origin.x * ray.origin.x + ray.origin.z * ray.origin.z -
                  radius * radius;
  const float determinant = b * b - 4.0 * a * c;
  if (determinant <= pathLoopRayEpsilon) {
    return result;
  }
  const float determinantRoot = sqrt(determinant);
  const float denominator = 2.0 * a;
  const float distances[2] = float[2]((-determinantRoot - b) / denominator,
                                      (determinantRoot - b) / denominator);
  float bestDistance = rayInfinity();
  for (uint index = 0u; index != 2u; ++index) {
    const float distance = distances[index];
    if (distance <= 0.0 || distance < ray.minDistance ||
        distance > ray.maxDistance || distance >= bestDistance ||
        distance >= maxHitDistance) {
      continue;
    }
    const float y = ray.origin.y + ray.direction.y * distance;
    if (y < -halfHeight || y > halfHeight) {
      continue;
    }
    bestDistance = distance;
  }
  if (!finiteFloat(bestDistance)) {
    return result;
  }
  result.hit = true;
  result.distance = bestDistance;
  result.point = vec4(ray.origin.xyz + ray.direction.xyz * bestDistance, 1.0);
  result.normal = vec4(result.point.x * inverseRadius, 0.0,
                       result.point.z * inverseRadius, 0.0);
  float u = atan(result.point.z, result.point.x) / pathLoopTau;
  if (u < 0.0) {
    u += 1.0;
  }
  const float height = 2.0 * halfHeight;
  const float v = height == 0.0 ? 0.0 : (result.point.y + halfHeight) / height;
  result.uv = vec4(u, v, 0.0, 0.0);
  return result;
}

bool almostZero(float value) {
  return abs(value) <= pathLoopRayEpsilon * 10.0;
}

float cubeRoot(float value) {
  return value < 0.0 ? -pow(-value, 1.0 / 3.0) : pow(value, 1.0 / 3.0);
}

uint solveQuadric(float a, float b, float c, inout float roots[4]) {
  if (almostZero(a)) {
    if (almostZero(b)) {
      return 0u;
    }
    roots[0] = -c / b;
    return 1u;
  }
  const float determinant = b * b - 4.0 * a * c;
  if (almostZero(determinant)) {
    roots[0] = -b / (2.0 * a);
    return 1u;
  }
  if (determinant > 0.0) {
    const float determinantRoot = sqrt(determinant);
    roots[0] = (-determinantRoot - b) / (2.0 * a);
    roots[1] = (determinantRoot - b) / (2.0 * a);
    return 2u;
  }
  return 0u;
}

uint solveCubic(float a, float b, float c, float d, inout float roots[4]) {
  if (almostZero(a)) {
    return solveQuadric(b, c, d, roots);
  }
  const float normA = b / a;
  const float normB = c / a;
  const float normC = d / a;
  const float normASquared = normA * normA;
  const float p = (-(1.0 / 3.0) * normASquared + normB) / 3.0;
  const float q = (2.0 / 27.0 * normA * normASquared -
                   (1.0 / 3.0) * normA * normB + normC) / 2.0;
  const float pCube = p * p * p;
  const float determinant = q * q + pCube;
  uint count = 0u;
  if (almostZero(determinant)) {
    if (almostZero(q)) {
      roots[0] = 0.0;
      count = 1u;
    } else {
      const float root = cubeRoot(-q);
      roots[0] = 2.0 * root;
      roots[1] = -root;
      count = 2u;
    }
  } else if (determinant < 0.0) {
    const float pi = 3.14159265358979323846;
    const float phi = acos(clamp(-q / sqrt(-pCube), -1.0, 1.0)) / 3.0;
    const float t = 2.0 * sqrt(-p);
    roots[0] = t * cos(phi);
    roots[1] = -t * cos(phi + pi / 3.0);
    roots[2] = -t * cos(phi - pi / 3.0);
    count = 3u;
  } else {
    const float determinantRoot = sqrt(determinant);
    roots[0] = cubeRoot(determinantRoot - q) - cubeRoot(determinantRoot + q);
    count = 1u;
  }
  const float sub = normA / 3.0;
  for (uint index = 0u; index != count; ++index) {
    roots[index] -= sub;
  }
  return count;
}

uint solveQuartic(float a, float b, float c, float d, float e, inout float roots[4]) {
  if (almostZero(a)) {
    return solveCubic(b, c, d, e, roots);
  }
  const float normA = b / a;
  const float normB = c / a;
  const float normC = d / a;
  const float normD = e / a;
  const float normASquared = normA * normA;
  const float p = -3.0 / 8.0 * normASquared + normB;
  const float q = 1.0 / 8.0 * normASquared * normA -
                  0.5 * normA * normB + normC;
  const float r = -3.0 / 256.0 * normASquared * normASquared +
                  1.0 / 16.0 * normASquared * normB -
                  0.25 * normA * normC + normD;
  uint count = 0u;
  if (almostZero(r)) {
    float cubicRoots[4];
    const uint cubicCount = solveCubic(1.0, 0.0, p, q, cubicRoots);
    for (uint index = 0u; index != cubicCount; ++index) {
      roots[count++] = cubicRoots[index];
    }
  } else {
    float cubicRoots[4];
    const uint cubicCount = solveCubic(1.0, -0.5 * p, -r,
                                       0.5 * r * p - 0.125 * q * q,
                                       cubicRoots);
    if (cubicCount == 0u) {
      return 0u;
    }
    const float z = cubicRoots[0];
    float u = z * z - r;
    float v = 2.0 * z - p;
    const float uTol = pathLoopRayEpsilon * 16.0 * (1.0 + abs(z * z) + abs(r));
    const float vTol = pathLoopRayEpsilon * 16.0 * (1.0 + abs(2.0 * z) + abs(p));
    if (u < -uTol || v < -vTol) {
      return 0u;
    }
    u = u <= 0.0 ? 0.0 : sqrt(u);
    v = v <= 0.0 ? 0.0 : sqrt(v);
    float quadRoots[4];
    uint quadCount = solveQuadric(1.0, q < 0.0 ? -v : v, z - u, quadRoots);
    for (uint index = 0u; index != quadCount; ++index) {
      roots[count++] = quadRoots[index];
    }
    quadCount = solveQuadric(1.0, q < 0.0 ? v : -v, z + u, quadRoots);
    for (uint index = 0u; index != quadCount; ++index) {
      roots[count++] = quadRoots[index];
    }
  }
  const float sub = 0.25 * normA;
  for (uint index = 0u; index != count; ++index) {
    roots[index] -= sub;
  }
  for (uint i = 1u; i < count; ++i) {
    const float value = roots[i];
    uint j = i;
    while (j > 0u && roots[j - 1u] > value) {
      roots[j] = roots[j - 1u];
      --j;
    }
    roots[j] = value;
  }
  return count;
}

LocalPrimitiveHit intersectTorus(GpuIntersectionRay ray,
                                 GpuIntersectionTorusPayload torus,
                                 float maxHitDistance) {
  LocalPrimitiveHit result = emptyPrimitiveHit(maxHitDistance);
  const float sweptRadius = torus.sweptTubeRadius.x;
  const float tubeRadius = torus.sweptTubeRadius.y;
  const float dd = dot(ray.direction.xyz, ray.direction.xyz);
  const float oorr = dot(ray.origin.xyz, ray.origin.xyz) -
                     sweptRadius * sweptRadius - tubeRadius * tubeRadius;
  const float od = dot(ray.origin.xyz, ray.direction.xyz);
  const float fourRR = 4.0 * sweptRadius * sweptRadius;
  float roots[4];
  const uint rootCount = solveQuartic(
    dd * dd,
    4.0 * dd * od,
    2.0 * dd * oorr + 4.0 * od * od + fourRR * ray.direction.y * ray.direction.y,
    4.0 * od * oorr + 2.0 * fourRR * ray.origin.y * ray.direction.y,
    oorr * oorr - fourRR * (tubeRadius * tubeRadius - ray.origin.y * ray.origin.y),
    roots);
  float bestDistance = rayInfinity();
  for (uint index = 0u; index != rootCount; ++index) {
    const float distance = roots[index];
    if (distance <= 0.0 || distance < ray.minDistance ||
        distance > ray.maxDistance || distance >= bestDistance ||
        distance >= maxHitDistance) {
      continue;
    }
    bestDistance = distance;
  }
  if (!finiteFloat(bestDistance)) {
    return result;
  }
  result.hit = true;
  result.distance = bestDistance;
  result.point = vec4(ray.origin.xyz + ray.direction.xyz * bestDistance, 1.0);
  const float paramSquared = sweptRadius * sweptRadius + tubeRadius * tubeRadius;
  const float sumSquared = dot(result.point.xyz, result.point.xyz);
  result.normal = vec4(normalize(vec3(
    4.0 * result.point.x * (sumSquared - paramSquared),
    4.0 * result.point.y *
      (sumSquared - paramSquared + 2.0 * sweptRadius * sweptRadius),
    4.0 * result.point.z * (sumSquared - paramSquared))), 0.0);
  return result;
}

LocalPrimitiveHit intersectSupportedPrimitive(GpuIntersectionRay ray,
                                              GpuIntersectionPrimitiveRecord primitive,
                                              float maxHitDistance) {
  GpuIntersectionRay primitiveRay = ray;
  bool hasTransform = primitive.transform != 0u;
  GpuIntersectionTransformPayload transform;
  if (hasTransform) {
    if (primitive.transform >= parameters.transformCount) {
      return emptyPrimitiveHit(maxHitDistance);
    }
    transform = readTransform(primitive.transform);
    primitiveRay = transformRay(ray, transform);
  }
  if (primitive.kind == gpuIntersectionTrianglePrimitiveKind) {
    if (primitive.payloadOffset >= parameters.triangleCount) {
      return emptyPrimitiveHit(maxHitDistance);
    }
    LocalPrimitiveHit triangleHit =
        intersectTriangle(primitiveRay, readTriangle(primitive.payloadOffset), maxHitDistance);
    if (hasTransform) {
      triangleHit = transformHit(triangleHit, transform);
    }
    return triangleHit;
  }
  if (primitive.kind == gpuIntersectionSpherePrimitiveKind) {
    if (primitive.payloadOffset >= parameters.sphereCount) {
      return emptyPrimitiveHit(maxHitDistance);
    }
    LocalPrimitiveHit sphereHit =
        intersectSphere(primitiveRay, readSphere(primitive.payloadOffset), maxHitDistance);
    if (hasTransform) {
      sphereHit = transformHit(sphereHit, transform);
    }
    return sphereHit;
  }
  if (primitive.kind == gpuIntersectionPlanePrimitiveKind) {
    if (primitive.payloadOffset >= parameters.planeCount) {
      return emptyPrimitiveHit(maxHitDistance);
    }
    LocalPrimitiveHit planeHit =
        intersectPlane(primitiveRay, readPlane(primitive.payloadOffset), maxHitDistance);
    if (hasTransform) {
      planeHit = transformHit(planeHit, transform);
    }
    return planeHit;
  }
  if (primitive.kind == gpuIntersectionRectanglePrimitiveKind) {
    if (primitive.payloadOffset >= parameters.rectangleCount) {
      return emptyPrimitiveHit(maxHitDistance);
    }
    LocalPrimitiveHit rectangleHit =
        intersectRectangle(primitiveRay, readRectangle(primitive.payloadOffset), maxHitDistance);
    if (hasTransform) {
      rectangleHit = transformHit(rectangleHit, transform);
    }
    return rectangleHit;
  }
  if (primitive.kind == gpuIntersectionDiskPrimitiveKind) {
    if (primitive.payloadOffset >= parameters.diskCount) {
      return emptyPrimitiveHit(maxHitDistance);
    }
    LocalPrimitiveHit diskHit =
        intersectDisk(primitiveRay, readDisk(primitive.payloadOffset), maxHitDistance);
    if (hasTransform) {
      diskHit = transformHit(diskHit, transform);
    }
    return diskHit;
  }
  if (primitive.kind == gpuIntersectionOpenCylinderPrimitiveKind) {
    if (primitive.payloadOffset >= parameters.openCylinderCount) {
      return emptyPrimitiveHit(maxHitDistance);
    }
    LocalPrimitiveHit openCylinderHit =
        intersectOpenCylinder(primitiveRay, readOpenCylinder(primitive.payloadOffset),
                              maxHitDistance);
    if (hasTransform) {
      openCylinderHit = transformHit(openCylinderHit, transform);
    }
    return openCylinderHit;
  }
  if (primitive.kind == gpuIntersectionTorusPrimitiveKind) {
    if (primitive.payloadOffset >= parameters.torusCount) {
      return emptyPrimitiveHit(maxHitDistance);
    }
    LocalPrimitiveHit torusHit =
        intersectTorus(primitiveRay, readTorus(primitive.payloadOffset), maxHitDistance);
    if (hasTransform) {
      torusHit = transformHit(torusHit, transform);
    }
    return torusHit;
  }
  return emptyPrimitiveHit(maxHitDistance);
}

GpuIntersectionHitRecord missHitRecord(GpuIntersectionRay ray) {
  GpuIntersectionHitRecord record;
  record.hit = 0u;
  record.material = 0u;
  record.object = 0u;
  record.primitiveRecord = 0u;
  record.rayIndex = ray.rayIndex;
  record.reservedId0 = 0u;
  record.reservedId1 = 0u;
  record.reservedId2 = 0u;
  record.distance = rayInfinity();
  record.reservedDistance0 = 0.0;
  record.reservedDistance1 = 0.0;
  record.reservedDistance2 = 0.0;
  record.point = vec4(0.0);
  record.normal = vec4(0.0);
  record.uv = vec4(0.0);
  record.barycentric = vec4(0.0);
  return record;
}

GpuIntersectionHitRecord hitRecordForPrimitive(GpuIntersectionRay ray,
                                                GpuIntersectionPrimitiveRecord primitive,
                                                uint primitiveIndex,
                                                LocalPrimitiveHit hit) {
  GpuIntersectionHitRecord record;
  record.hit = 1u;
  record.material = primitive.material;
  record.object = primitive.object;
  record.primitiveRecord = primitiveIndex;
  record.rayIndex = ray.rayIndex;
  record.reservedId0 = 0u;
  record.reservedId1 = 0u;
  record.reservedId2 = 0u;
  record.distance = hit.distance;
  record.reservedDistance0 = 0.0;
  record.reservedDistance1 = 0.0;
  record.reservedDistance2 = 0.0;
  record.point = hit.point;
  record.normal = hit.normal;
  record.uv = hit.uv;
  record.barycentric = hit.barycentric;
  return record;
}

GpuIntersectionHitRecord closestSupportedHit(GpuIntersectionRay ray) {
  GpuIntersectionHitRecord closest = missHitRecord(ray);
  if (parameters.primitiveCount == 0u ||
      (parameters.triangleCount == 0u && parameters.sphereCount == 0u &&
       parameters.planeCount == 0u && parameters.rectangleCount == 0u &&
       parameters.diskCount == 0u && parameters.openCylinderCount == 0u &&
       parameters.torusCount == 0u)) {
    return closest;
  }

  if (parameters.bvhNodeCount == 0u) {
    for (uint primitiveIndex = 0u; primitiveIndex != parameters.primitiveCount; ++primitiveIndex) {
      const GpuIntersectionPrimitiveRecord primitive = readPrimitive(primitiveIndex);
      if (!boundsIntersectsRay(primitive.bounds, ray, closest.distance)) {
        continue;
      }
      const LocalPrimitiveHit primitiveHit =
          intersectSupportedPrimitive(ray, primitive, closest.distance);
      if (primitiveHit.hit) {
        closest = hitRecordForPrimitive(ray, primitive, primitiveIndex, primitiveHit);
      }
    }
    return closest;
  }

  uint stack[64];
  uint stackSize = 0u;
  stack[stackSize++] = 0u;
  while (stackSize != 0u) {
    const uint nodeIndex = stack[--stackSize];
    if (nodeIndex >= parameters.bvhNodeCount) {
      continue;
    }
    const GpuIntersectionBvhNode node = readBvhNode(nodeIndex);
    if (!boundsIntersectsRay(node.bounds, ray, closest.distance)) {
      continue;
    }
    if ((node.flags & gpuIntersectionLeafNodeFlag) == 0u) {
      if (stackSize + 2u <= 64u) {
        stack[stackSize++] = node.primitiveCount;
        stack[stackSize++] = node.leftOrFirstPrimitive;
      }
      continue;
    }
    for (uint offset = 0u; offset != node.primitiveCount; ++offset) {
      const uint primitiveIndex = node.leftOrFirstPrimitive + offset;
      if (primitiveIndex >= parameters.primitiveCount) {
        continue;
      }
      const GpuIntersectionPrimitiveRecord primitive = readPrimitive(primitiveIndex);
      if (!boundsIntersectsRay(primitive.bounds, ray, closest.distance)) {
        continue;
      }
      const LocalPrimitiveHit primitiveHit =
          intersectSupportedPrimitive(ray, primitive, closest.distance);
      if (primitiveHit.hit) {
        closest = hitRecordForPrimitive(ray, primitive, primitiveIndex, primitiveHit);
      }
    }
  }
  return closest;
}

vec4 environmentColor(uint environmentIndex) {
  if (parameters.environmentCount == 0u || environmentIndex >= parameters.environmentCount) {
    return vec4(0.0);
  }
  const uint byteOffset = parameters.environmentByteOffset + environmentIndex * 32u;
  if (byteOffset + 32u > parameters.sceneUploadBytes) {
    return vec4(0.0);
  }
  const uint wordOffset = byteOffset / 4u;
  return readVec4(wordOffset + 4u);
}

bool usesAmbientEnvironmentLayout() {
  return parameters.environmentCount >= 3u;
}

vec4 sceneAmbientRadiance() {
  return usesAmbientEnvironmentLayout() ? environmentColor(0u) : vec4(0.0);
}

vec4 missRadiance(GpuDiffusePathStateRecord path) {
  const uint environmentIndex =
      path.depth == 0u ? (usesAmbientEnvironmentLayout() ? 1u : 0u) :
                         parameters.environmentCount - 1u;
  return environmentColor(environmentIndex);
}

bool constantTextureColor(uint textureId, out vec4 color) {
  color = vec4(0.0);
  if (textureId >= parameters.textureCount) {
    return false;
  }
  const GpuTracingTextureRecord texture = readTexture(textureId);
  if (texture.kind != gpuTracingConstantColorTextureKind) {
    return false;
  }
  color = texture.parameters;
  return true;
}

vec2 textureCoordinates(GpuTracingTextureRecord texture, GpuIntersectionHitRecord hit) {
  const uint mapping = texture.flags & gpuTracingTextureMappingMask;
  if (mapping == gpuTracingUvTextureMappingKind) {
    return vec2(hit.uv.x * texture.parameters.x, hit.uv.y * texture.parameters.y);
  }
  if (mapping == gpuTracingPlanarTextureMappingKind) {
    return vec2(hit.point.x, hit.point.z);
  }
  return vec2(0.0);
}

float normalizedTextureCoordinate(GpuTracingTextureRecord texture, float coordinate) {
  if ((texture.flags & gpuTracingTextureWrapClampFlag) != 0u) {
    return clamp(coordinate, 0.0, 1.0);
  }
  return coordinate - floor(coordinate);
}

int imageTextureWrappedCoordinate(GpuTracingTextureRecord texture, int coordinate, int size) {
  if ((texture.flags & gpuTracingTextureWrapClampFlag) != 0u) {
    return clamp(coordinate, 0, size - 1);
  }
  int wrapped = coordinate % size;
  return wrapped < 0 ? wrapped + size : wrapped;
}

int imageTextureCoordinate(GpuTracingTextureRecord texture, float coordinate, int size) {
  const int result = int(floor(normalizedTextureCoordinate(texture, coordinate) *
                               float(size)));
  return imageTextureWrappedCoordinate(texture, result, size);
}

bool imageTextureTexelColor(GpuTracingTextureRecord texture,
                            int x,
                            int y,
                            int width,
                            out vec4 color) {
  const uint texelTextureId = texture.payloadOffset + uint(y * width + x);
  return constantTextureColor(texelTextureId, color);
}

bool untintedTextureColor(uint textureId, GpuIntersectionHitRecord hit, out vec4 color) {
  color = vec4(0.0);
  if (textureId >= parameters.textureCount) {
    return false;
  }
  const GpuTracingTextureRecord texture = readTexture(textureId);
  if (texture.kind == gpuTracingConstantColorTextureKind) {
    color = texture.parameters;
    return true;
  }
  if (texture.kind == gpuTracingUvColorTextureKind) {
    color = vec4(hit.uv.x, hit.uv.y, 0.0, 1.0);
    return true;
  }
  if (texture.kind == gpuTracingCheckerBoardTextureKind) {
    const uint mapping = texture.flags & gpuTracingTextureMappingMask;
    if ((mapping != gpuTracingPlanarTextureMappingKind &&
         mapping != gpuTracingUvTextureMappingKind) ||
        texture.payloadOffset >= parameters.textureCount ||
        texture.payloadCount >= parameters.textureCount) {
      return false;
    }
    const vec2 st = textureCoordinates(texture, hit);
    const int parity = int(floor(st.x)) + int(floor(st.y));
    const uint childTextureId = parity % 2 == 0 ?
        texture.payloadOffset : texture.payloadCount;
    return constantTextureColor(childTextureId, color);
  }
  if (texture.kind == gpuTracingImageTextureKind) {
    const int width = int(round(texture.parameters.z));
    const int height = int(round(texture.parameters.w));
    if (width <= 0 || height <= 0 ||
        texture.payloadOffset >= parameters.textureCount ||
        texture.payloadCount != uint(width * height) ||
        texture.payloadOffset + texture.payloadCount > parameters.textureCount) {
      return false;
    }
    const vec2 st = textureCoordinates(texture, hit);
    if ((texture.flags & gpuTracingTextureFilterBilinearFlag) != 0u) {
      const float x = normalizedTextureCoordinate(texture, st.x) * float(width) - 0.5;
      const float y = normalizedTextureCoordinate(texture, st.y) * float(height) - 0.5;
      const int x0 = int(floor(x));
      const int y0 = int(floor(y));
      const float tx = x - float(x0);
      const float ty = y - float(y0);
      vec4 c00;
      vec4 c10;
      vec4 c01;
      vec4 c11;
      if (!imageTextureTexelColor(texture,
                                  imageTextureWrappedCoordinate(texture, x0, width),
                                  imageTextureWrappedCoordinate(texture, y0, height),
                                  width,
                                  c00) ||
          !imageTextureTexelColor(texture,
                                  imageTextureWrappedCoordinate(texture, x0 + 1, width),
                                  imageTextureWrappedCoordinate(texture, y0, height),
                                  width,
                                  c10) ||
          !imageTextureTexelColor(texture,
                                  imageTextureWrappedCoordinate(texture, x0, width),
                                  imageTextureWrappedCoordinate(texture, y0 + 1, height),
                                  width,
                                  c01) ||
          !imageTextureTexelColor(texture,
                                  imageTextureWrappedCoordinate(texture, x0 + 1, width),
                                  imageTextureWrappedCoordinate(texture, y0 + 1, height),
                                  width,
                                  c11)) {
        return false;
      }
      color = mix(mix(c00, c10, tx), mix(c01, c11, tx), ty);
      return true;
    }
    const int x = imageTextureCoordinate(texture, st.x, width);
    const int y = imageTextureCoordinate(texture, st.y, height);
    return imageTextureTexelColor(texture, x, y, width, color);
  }
  return false;
}

bool textureColor(uint textureId, GpuIntersectionHitRecord hit, out vec4 color) {
  vec4 tint = vec4(1.0);
  uint currentTexture = textureId;
  for (uint depth = 0u; depth != pathLoopMaxTextureEvaluationDepth; ++depth) {
    if (currentTexture >= parameters.textureCount) {
      color = vec4(0.0);
      return false;
    }
    const GpuTracingTextureRecord texture = readTexture(currentTexture);
    if (texture.kind == gpuTracingTintedTextureKind) {
      tint *= texture.parameters;
      currentTexture = texture.payloadOffset;
      continue;
    }
    if (texture.kind == gpuTracingCheckerBoardTextureKind) {
      const uint mapping = texture.flags & gpuTracingTextureMappingMask;
      if ((mapping != gpuTracingPlanarTextureMappingKind &&
           mapping != gpuTracingUvTextureMappingKind) ||
          texture.payloadOffset >= parameters.textureCount ||
          texture.payloadCount >= parameters.textureCount) {
        color = vec4(0.0);
        return false;
      }
      const vec2 st = textureCoordinates(texture, hit);
      const int parity = int(floor(st.x)) + int(floor(st.y));
      currentTexture = parity % 2 == 0 ? texture.payloadOffset : texture.payloadCount;
      continue;
    }
    vec4 base;
    if (!untintedTextureColor(currentTexture, hit, base)) {
      color = vec4(0.0);
      return false;
    }
    color = base * tint;
    return true;
  }
  color = vec4(0.0);
  return false;
}

bool surfaceMaterial(GpuIntersectionHitRecord hit, out GpuTracingMaterialRecord material) {
  material.kind = 0u;
  material.albedoTexture = 0u;
  material.emissionTexture = 0u;
  material.flags = 0u;
  material.parameters = vec4(0.0);
  material.specularParameters = vec4(0.0);
  material.continuationParameters = vec4(0.0);
  material.transmissionParameters = vec4(0.0);
  material.portalOriginMatrix0 = vec4(0.0);
  material.portalOriginMatrix1 = vec4(0.0);
  material.portalOriginMatrix2 = vec4(0.0);
  material.portalOriginMatrix3 = vec4(0.0);
  material.portalDirectionMatrix0 = vec4(0.0);
  material.portalDirectionMatrix1 = vec4(0.0);
  material.portalDirectionMatrix2 = vec4(0.0);
  if (hit.material >= parameters.materialCount) {
    return false;
  }
  material = readMaterial(hit.material);
  return material.kind == gpuTracingMatteMaterialKind ||
         material.kind == gpuTracingPhongMaterialKind ||
         material.kind == gpuTracingReflectiveMaterialKind ||
         material.kind == gpuTracingTransparentMaterialKind;
}

bool diffuseReflectance(GpuIntersectionHitRecord hit, out vec4 reflectance) {
  reflectance = vec4(0.0);
  GpuTracingMaterialRecord material;
  if (!surfaceMaterial(hit, material)) {
    return false;
  }
  vec4 albedo;
  if (!textureColor(material.albedoTexture, hit, albedo)) {
    return false;
  }
  reflectance = albedo * material.parameters.y;
  return true;
}

float diffuseSamplingWeight(GpuTracingMaterialRecord material) {
  const float diffuse = max(0.0, material.parameters.y);
  const float specular = max(0.0, material.parameters.z);
  const float total = diffuse + specular;
  return total <= 0.0 ? 1.0 : diffuse / total;
}

vec4 glossyPhongBsdf(GpuTracingMaterialRecord material, vec3 normal, vec3 wi, vec3 wo) {
  if (dot(normal, wi) < 0.0 || dot(normal, wo) < 0.0) {
    return vec4(0.0);
  }
  const vec3 lobeAxis = normalize(reflect(-wi, normal));
  const float lobeDotOut = dot(lobeAxis, normalize(wo));
  if (lobeDotOut <= 0.0) {
    return vec4(0.0);
  }
  return vec4(material.specularParameters.xyz * material.parameters.z *
              pow(lobeDotOut, material.parameters.w), 0.0);
}

vec4 finiteBsdf(vec4 diffuseReflectance, GpuTracingMaterialRecord material,
                vec3 normal, vec3 wi, vec3 wo) {
  vec4 value = diffuseReflectance * pathLoopInvPi;
  if (material.kind == gpuTracingPhongMaterialKind ||
      material.kind == gpuTracingReflectiveMaterialKind ||
      material.kind == gpuTracingTransparentMaterialKind) {
    value += glossyPhongBsdf(material, normal, wi, wo);
  }
  return value;
}

float phongLobePdf(GpuTracingMaterialRecord material, vec3 normal, vec3 wi, vec3 wo) {
  if (dot(normal, wi) < 0.0 || dot(normal, wo) < 0.0) {
    return 0.0;
  }
  const vec3 lobeAxis = normalize(reflect(-wi, normal));
  const float lobeDotOut = dot(lobeAxis, normalize(wo));
  if (lobeDotOut <= 0.0) {
    return 0.0;
  }
  return ((material.parameters.w + 1.0) / pathLoopTau) *
         pow(lobeDotOut, material.parameters.w);
}

float finiteBsdfPdf(GpuTracingMaterialRecord material, vec3 normal, vec3 wi, vec3 wo) {
  if (material.kind == gpuTracingMatteMaterialKind) {
    return cosineHemispherePdf(normal, wo);
  }
  if (material.kind != gpuTracingPhongMaterialKind) {
    return 0.0;
  }
  const float diffuseWeight = diffuseSamplingWeight(material);
  return diffuseWeight * cosineHemispherePdf(normal, wo) +
         (1.0 - diffuseWeight) * phongLobePdf(material, normal, wi, wo);
}

vec4 mirrorReflectance(GpuTracingMaterialRecord material) {
  return vec4(material.continuationParameters.xyz * material.continuationParameters.w, 0.0);
}

vec3 sampleFiniteBsdfDirection(GpuTracingMaterialRecord material, vec3 normal, vec3 wi,
                               vec2 sample) {
  if (material.kind != gpuTracingPhongMaterialKind) {
    return cosineHemisphereDirection(normal, sample);
  }
  const float diffuseWeight = diffuseSamplingWeight(material);
  const float selector = clamp(sample.x, 0.0, 1.0);
  const float y = clamp(sample.y, 0.0, 1.0);
  if (diffuseWeight >= 1.0 || selector < diffuseWeight) {
    const float remappedX = diffuseWeight > 0.0 ? selector / diffuseWeight : selector;
    return cosineHemisphereDirection(normal, vec2(remappedX, y));
  }
  const float specularWeight = 1.0 - diffuseWeight;
  const float remappedX =
      specularWeight > 0.0 ? (selector - diffuseWeight) / specularWeight : selector;
  return phongLobeDirection(normalize(reflect(-wi, normal)), vec2(remappedX, y),
                            material.parameters.w);
}

bool matteAmbientRadiance(GpuIntersectionHitRecord hit, GpuDiffusePathStateRecord path,
                          out vec4 ambient) {
  ambient = vec4(0.0);
  GpuTracingMaterialRecord material;
  if (!surfaceMaterial(hit, material)) {
    return false;
  }
  vec4 albedo;
  if (!textureColor(material.albedoTexture, hit, albedo)) {
    return false;
  }
  ambient = path.throughput * albedo * material.parameters.x * sceneAmbientRadiance();
  return true;
}

float powerHeuristic(float sampledPdf, float otherPdf) {
  const float sampled = max(0.0, sampledPdf);
  const float other = max(0.0, otherPdf);
  const float sampledSquared = sampled * sampled;
  const float otherSquared = other * other;
  const float denominator = sampledSquared + otherSquared;
  return denominator == 0.0 ? 0.0 : sampledSquared / denominator;
}

float emitterHitMisWeight(GpuDiffusePathStateRecord path) {
  const bool sampledFromBsdf =
      (path.previousEventFlags & gpuDiffusePathStateSampledFromBsdfFlag) != 0u;
  const bool sampledDelta =
      (path.previousEventFlags & gpuDiffusePathStateBsdfSampleDeltaFlag) != 0u;
  if (!sampledFromBsdf || sampledDelta) {
    return 1.0;
  }
  return powerHeuristic(path.previousBsdfPdf, path.previousLightPdf);
}

bool emissiveContribution(GpuIntersectionHitRecord hit, GpuDiffusePathStateRecord path,
                          out vec4 emitted) {
  emitted = vec4(0.0);
  if (hit.material >= parameters.materialCount) {
    return false;
  }
  const GpuTracingMaterialRecord material = readMaterial(hit.material);
  if (material.kind != gpuTracingEmissiveMaterialKind) {
    return false;
  }
  vec4 emission;
  if (!textureColor(material.emissionTexture, hit, emission)) {
    return false;
  }
  const vec3 normal = normalize(hit.normal.xyz);
  const vec3 incoming = -normalize(path.ray.direction.xyz);
  emitted = dot(normal, incoming) > 0.0 ?
      path.throughput * emission * emitterHitMisWeight(path) : vec4(0.0);
  return true;
}

bool denoiserAlbedo(GpuIntersectionHitRecord hit, GpuTracingMaterialRecord material,
                    out vec4 albedo) {
  albedo = vec4(0.0);
  if (material.kind == gpuTracingEmissiveMaterialKind) {
    return textureColor(material.emissionTexture, hit, albedo);
  }
  if (material.kind == gpuTracingMatteMaterialKind ||
      material.kind == gpuTracingPhongMaterialKind ||
      material.kind == gpuTracingReflectiveMaterialKind ||
      material.kind == gpuTracingTransparentMaterialKind) {
    return textureColor(material.albedoTexture, hit, albedo);
  }
  return false;
}

void writeDenoiserFeature(GpuDiffusePathStateRecord path, GpuIntersectionHitRecord hit) {
  const uint featurePixelCount = parameters.imageWidth * parameters.imageHeight;
  if (parameters.captureDenoiserFeatures == 0u || hit.hit == 0u || path.depth != 0u ||
      path.primarySampleIndex != 0u || path.pixelIndex >= featurePixelCount) {
    return;
  }
  if (hit.material >= parameters.materialCount) {
    return;
  }
  const GpuTracingMaterialRecord material = readMaterial(hit.material);
  vec4 albedo;
  if (!denoiserAlbedo(hit, material, albedo)) {
    return;
  }

  GpuDiffusePathDenoiserFeatureRecord record;
  record.pixelIndex = path.pixelIndex;
  record.primarySampleIndex = path.primarySampleIndex;
  record.flags = gpuDiffusePathDenoiserFeatureValidFlag;
  record.reserved = 0u;
  record.albedo = albedo;
  record.normal = vec4(normalize(hit.normal.xyz), 0.0);
  record.depth = hit.distance;
  record.reservedDepth0 = 0.0;
  record.reservedDepth1 = 0.0;
  record.reservedDepth2 = 0.0;
  denoiserFeatures[path.pixelIndex] = record;
}

GpuIntersectionRay continuationRay(GpuIntersectionHitRecord hit, GpuDiffusePathStateRecord path,
                                   vec3 direction) {
  GpuIntersectionRay ray = path.ray;
  ray.origin = vec4(hit.point.xyz + direction * pathLoopRayEpsilon, 1.0);
  ray.direction = vec4(direction, 0.0);
  ray.minDistance = pathLoopMinimumHitDistance;
  ray.maxDistance = rayInfinity();
  ray.timeSample = path.ray.timeSample;
  return ray;
}

DirectLightSample emptyDirectLightSample() {
  DirectLightSample sample;
  sample.valid = 0u;
  sample.delta = 0u;
  sample.direction = vec3(0.0);
  sample.radiance = vec4(0.0);
  sample.distance = 0.0;
  sample.pdf = 0.0;
  return sample;
}

DirectLightSelection emptyDirectLightSelection() {
  DirectLightSelection selection;
  selection.valid = 0u;
  selection.lightIndex = 0u;
  selection.pdf = 0.0;
  return selection;
}

float rectangularLightArea(GpuTracingLightRecord light) {
  return length(cross(light.u.xyz, light.v.xyz));
}

float compiledLightSelectionWeight(GpuTracingLightRecord light) {
  if (light.kind == gpuTracingPointLightKind || light.kind == gpuTracingDirectionalLightKind) {
    return maxColor(light.parameters);
  }
  if (light.kind == gpuTracingRectangularAreaLightKind) {
    return maxColor(light.parameters) * rectangularLightArea(light) * (pathLoopTau * 0.5);
  }
  return 0.0;
}

DirectLightSelection selectDirectLight(GpuDiffusePathStateRecord path, uint directSampleIndex) {
  DirectLightSelection selection = emptyDirectLightSelection();
  if (parameters.lightCount == 0u) {
    return selection;
  }

  float totalWeight = 0.0;
  for (uint lightIndex = 0u; lightIndex != parameters.lightCount; ++lightIndex) {
    totalWeight += compiledLightSelectionWeight(readLight(lightIndex));
  }
  const bool useUniformWeights = totalWeight <= 0.0;
  if (useUniformWeights) {
    totalWeight = float(parameters.lightCount);
  }

  const float unitSample = clamp(sample1D(path, lightSelectionDimension(path, directSampleIndex),
                                          0u),
                                 0.0, 0.9999999403953552);
  const float target = unitSample * totalWeight;
  float cumulative = 0.0;
  for (uint lightIndex = 0u; lightIndex != parameters.lightCount; ++lightIndex) {
    const float weight =
        useUniformWeights ? 1.0 : compiledLightSelectionWeight(readLight(lightIndex));
    cumulative += weight;
    if (target < cumulative) {
      selection.valid = 1u;
      selection.lightIndex = lightIndex;
      selection.pdf = weight / totalWeight;
      return selection;
    }
  }

  return selection;
}

float rectangularLightPdf(GpuTracingLightRecord light, vec3 point, vec3 direction) {
  const float area = rectangularLightArea(light);
  if (area <= pathLoopRayEpsilon) {
    return 0.0;
  }

  const vec3 lightNormal = cross(light.u.xyz, light.v.xyz) / area;
  const float normalDotDirection = dot(lightNormal, direction);
  if (abs(normalDotDirection) <= pathLoopRayEpsilon) {
    return 0.0;
  }

  const float distance =
      dot(light.positionOrDirection.xyz - point, lightNormal) / normalDotDirection;
  if (distance <= pathLoopRayEpsilon) {
    return 0.0;
  }

  const vec3 lightPoint = point + direction * distance;
  const vec3 local = lightPoint - light.positionOrDirection.xyz;
  const float uu = dot(light.u.xyz, light.u.xyz);
  const float uv = dot(light.u.xyz, light.v.xyz);
  const float vv = dot(light.v.xyz, light.v.xyz);
  const float lu = dot(local, light.u.xyz);
  const float lv = dot(local, light.v.xyz);
  const float determinant = uu * vv - uv * uv;
  if (abs(determinant) <= pathLoopRayEpsilon) {
    return 0.0;
  }

  const float localU = (vv * lu - uv * lv) / determinant;
  const float localV = (uu * lv - uv * lu) / determinant;
  if (localU < -0.5 - pathLoopRayEpsilon || localU > 0.5 + pathLoopRayEpsilon ||
      localV < -0.5 - pathLoopRayEpsilon || localV > 0.5 + pathLoopRayEpsilon) {
    return 0.0;
  }

  const float lightCosine = max(0.0, dot(lightNormal, -normalize(direction)));
  if (lightCosine <= pathLoopRayEpsilon) {
    return 0.0;
  }
  return (distance * distance) / (lightCosine * area);
}

float lightPdf(vec3 point, vec3 direction) {
  if (parameters.lightCount == 0u) {
    return 0.0;
  }

  float totalWeight = 0.0;
  for (uint lightIndex = 0u; lightIndex != parameters.lightCount; ++lightIndex) {
    totalWeight += compiledLightSelectionWeight(readLight(lightIndex));
  }
  const bool useUniformWeights = totalWeight <= 0.0;
  if (useUniformWeights) {
    totalWeight = float(parameters.lightCount);
  }

  float pdf = 0.0;
  for (uint lightIndex = 0u; lightIndex != parameters.lightCount; ++lightIndex) {
    const GpuTracingLightRecord light = readLight(lightIndex);
    const float selectionWeight =
        useUniformWeights ? 1.0 : compiledLightSelectionWeight(light);
    if (selectionWeight <= 0.0) {
      continue;
    }
    if (light.kind == gpuTracingRectangularAreaLightKind) {
      pdf += (selectionWeight / totalWeight) * rectangularLightPdf(light, point, direction);
    }
  }
  return pdf;
}

DirectLightSample sampleDirectLight(GpuTracingLightRecord light, vec3 point,
                                    vec2 surfaceSample) {
  DirectLightSample sample = emptyDirectLightSample();
  if (light.kind == gpuTracingPointLightKind) {
    const vec3 offset = light.positionOrDirection.xyz - point;
    const float distance = length(offset);
    if (distance <= pathLoopRayEpsilon) {
      return sample;
    }
    sample.valid = 1u;
    sample.delta = 1u;
    sample.direction = offset / distance;
    sample.radiance = light.parameters;
    sample.distance = distance;
    sample.pdf = 1.0;
    return sample;
  }
  if (light.kind == gpuTracingDirectionalLightKind) {
    const float lengthDirection = length(light.positionOrDirection.xyz);
    if (lengthDirection <= pathLoopRayEpsilon) {
      return sample;
    }
    sample.valid = 1u;
    sample.delta = 1u;
    sample.direction = light.positionOrDirection.xyz / lengthDirection;
    sample.radiance = light.parameters;
    sample.distance = rayInfinity();
    sample.pdf = 1.0;
    return sample;
  }
  if (light.kind == gpuTracingRectangularAreaLightKind) {
    const vec3 crossEdges = cross(light.u.xyz, light.v.xyz);
    const float area = length(crossEdges);
    if (area <= pathLoopRayEpsilon) {
      return sample;
    }
    const vec3 lightNormal = crossEdges / area;
    const vec3 lightPoint = light.positionOrDirection.xyz +
                            light.u.xyz * (surfaceSample.x - 0.5) +
                            light.v.xyz * (surfaceSample.y - 0.5);
    const vec3 offset = lightPoint - point;
    const float distance = length(offset);
    if (distance <= pathLoopRayEpsilon) {
      return sample;
    }
    const vec3 direction = offset / distance;
    const float lightCosine = max(0.0, dot(lightNormal, -direction));
    if (lightCosine <= pathLoopRayEpsilon) {
      return sample;
    }
    sample.valid = 1u;
    sample.delta = 0u;
    sample.direction = direction;
    sample.radiance = light.parameters;
    sample.distance = distance;
    sample.pdf = (distance * distance) / (lightCosine * area);
    return sample;
  }
  return sample;
}

GpuIntersectionRay shadowRayFor(vec3 point, DirectLightSample sample, float timeSample) {
  GpuIntersectionRay ray;
  ray.origin = vec4(point + sample.direction * pathLoopRayEpsilon, 1.0);
  ray.direction = vec4(sample.direction, 0.0);
  ray.minDistance = pathLoopMinimumHitDistance;
  ray.maxDistance = sample.distance;
  ray.timeSample = timeSample;
  ray.flags = 0u;
  ray.rayIndex = 0u;
  ray.reserved0 = 0u;
  ray.reserved1 = 0u;
  ray.reserved2 = 0u;
  return ray;
}

bool hitOccludesLight(GpuIntersectionHitRecord hit, float lightDistance) {
  if (hit.hit == 0u) {
    return false;
  }
  if (isinf(lightDistance)) {
    return true;
  }
  const float endpointTolerance = max(pathLoopMinimumHitDistance, lightDistance * 1.0e-5);
  const float occlusionLimit = max(0.0, lightDistance - endpointTolerance);
  return hit.distance < occlusionLimit;
}

DirectLightEstimate directLightEstimate(GpuIntersectionHitRecord hit, GpuDiffusePathStateRecord path,
                                        GpuTracingMaterialRecord material, vec4 reflectance) {
  DirectLightEstimate estimate;
  estimate.radiance = vec4(0.0);
  estimate.sampleCount = 0u;
  estimate.visibilityRayCount = 0u;
  estimate.contributingSampleCount = 0u;
  estimate.occludedSampleCount = 0u;
  if (parameters.lightCount == 0u) {
    return estimate;
  }
  const uint sampleCount = max(parameters.directLightSamples, 1u);
  const vec3 point = hit.point.xyz;
  const vec3 normal = normalize(hit.normal.xyz);
  const vec3 wi = -normalize(path.ray.direction.xyz);
  for (uint sampleIndex = 0u; sampleIndex != sampleCount; ++sampleIndex) {
    const DirectLightSelection selectedLight = selectDirectLight(path, sampleIndex);
    if (selectedLight.valid == 0u || selectedLight.lightIndex >= parameters.lightCount ||
        selectedLight.pdf <= 0.0) {
      continue;
    }
    const vec2 lightSample =
        sample2D(path, lightSurfaceDimension(path, selectedLight.lightIndex, sampleIndex));
    const GpuTracingLightRecord light = readLight(selectedLight.lightIndex);
    const DirectLightSample directLight = sampleDirectLight(light, point, lightSample);
    const float normalDotLight = dot(normal, directLight.direction);
    if (directLight.valid == 0u || directLight.pdf <= 0.0) {
      continue;
    }
    ++estimate.sampleCount;
    if (normalDotLight <= 0.0) {
      continue;
    }
    const GpuIntersectionRay visibilityRay =
        shadowRayFor(point, directLight, path.ray.timeSample);
    ++estimate.visibilityRayCount;
    const GpuIntersectionHitRecord visibilityHit = closestSupportedHit(visibilityRay);
    if (hitOccludesLight(visibilityHit, directLight.distance)) {
      ++estimate.occludedSampleCount;
      continue;
    }
    const vec4 bsdfValue = finiteBsdf(reflectance, material, normal, wi, directLight.direction);
    const float bsdfPdf = directLight.delta != 0u ? 0.0 :
        finiteBsdfPdf(material, normal, wi, directLight.direction);
    const float misWeight = directLight.delta != 0u ? 1.0 :
        (directLight.pdf * directLight.pdf) /
        (directLight.pdf * directLight.pdf + bsdfPdf * bsdfPdf);
    const vec4 contribution = path.throughput * bsdfValue * directLight.radiance *
                              (misWeight * normalDotLight /
                               (directLight.pdf * selectedLight.pdf));
    if (contribution.x != 0.0 || contribution.y != 0.0 || contribution.z != 0.0) {
      ++estimate.contributingSampleCount;
    }
    estimate.radiance += contribution;
  }
  estimate.radiance = estimate.radiance / float(sampleCount);
  return estimate;
}

GpuDiffusePathStateRecord portalContinuationPath(GpuIntersectionHitRecord hit,
                                                 GpuDiffusePathStateRecord path,
                                                 GpuTracingMaterialRecord material,
                                                 vec4 accumulatedRadiance,
                                                 out bool spawned) {
  spawned = false;
  GpuDiffusePathStateRecord next = path;
  next.accumulatedRadiance = accumulatedRadiance;
  const vec4 continuationThroughput = path.throughput * material.parameters;
  if (throughputIsBlack(continuationThroughput)) {
    next.flags = terminatedPathFlags(path.flags);
    next.throughput = vec4(0.0);
    return next;
  }

  const vec3 incoming = normalize(path.ray.direction.xyz);
  const vec4 shiftedOrigin = vec4(hit.point.xyz + incoming * pathLoopRayEpsilon, 1.0);
  const vec3 direction = normalize(transformDirection(
      material.portalDirectionMatrix0, material.portalDirectionMatrix1,
      material.portalDirectionMatrix2, vec4(incoming, 0.0)).xyz);

  next.ray = path.ray;
  next.ray.origin = transformPoint(material.portalOriginMatrix0,
                                   material.portalOriginMatrix1,
                                   material.portalOriginMatrix2,
                                   material.portalOriginMatrix3,
                                   shiftedOrigin);
  next.ray.direction = vec4(direction, 0.0);
  next.ray.minDistance = pathLoopMinimumHitDistance;
  next.ray.maxDistance = rayInfinity();
  next.ray.timeSample = path.ray.timeSample;
  next.throughput = continuationThroughput;
  next.depth = path.depth + 1u;
  next.previousBsdfPdf = 1.0;
  next.previousLightPdf = 0.0;
  next.previousMaterial = hit.material;
  next.previousEventFlags = gpuDiffusePathStateSampledFromBsdfFlag |
                            gpuDiffusePathStateBsdfSampleDeltaFlag;
  next.flags = activePathFlags(path.flags);
  spawned = true;
  return next;
}

GpuDiffusePathStateRecord matteContinuationPath(GpuIntersectionHitRecord hit,
                                                GpuDiffusePathStateRecord path,
                                                GpuTracingMaterialRecord material,
                                                vec4 reflectance,
                                                vec4 accumulatedRadiance,
                                                out bool spawned) {
  spawned = false;
  GpuDiffusePathStateRecord next = path;
  next.accumulatedRadiance = accumulatedRadiance;
  const vec3 normal = normalize(hit.normal.xyz);
  const vec3 wi = -normalize(path.ray.direction.xyz);
  const vec2 bsdfSample = sample2D(path, sampleDimension(path, 0u));
  const vec3 direction = sampleFiniteBsdfDirection(material, normal, wi, bsdfSample);
  const float pdf = finiteBsdfPdf(material, normal, wi, direction);
  const float normalDotOut = dot(normal, direction);
  vec4 continuationThroughput = vec4(0.0);
  if (pdf > 0.0 && normalDotOut > 0.0) {
    continuationThroughput =
        path.throughput * finiteBsdf(reflectance, material, normal, wi, direction) *
        (normalDotOut / pdf);
  }
  if (path.depth >= parameters.russianRouletteDepth) {
    const float probability = continuationProbability(continuationThroughput);
    const float roulette = sample1D(path, sampleDimension(path, 3u), 0u);
    continuationThroughput =
        roulette < probability && probability > 0.0 ? continuationThroughput / probability :
                                                      vec4(0.0);
  }
  if (throughputIsBlack(continuationThroughput)) {
    next.flags = terminatedPathFlags(path.flags);
    next.throughput = vec4(0.0);
    return next;
  }
  next.ray = continuationRay(hit, path, direction);
  next.throughput = continuationThroughput;
  next.depth = path.depth + 1u;
  next.previousBsdfPdf = pdf;
  next.previousLightPdf = lightPdf(hit.point.xyz, direction);
  next.previousMaterial = hit.material;
  next.previousEventFlags = gpuDiffusePathStateSampledFromBsdfFlag;
  next.flags = activePathFlags(path.flags);
  spawned = true;
  return next;
}

GpuDiffusePathStateRecord reflectiveContinuationPath(GpuIntersectionHitRecord hit,
                                                     GpuDiffusePathStateRecord path,
                                                     vec4 continuationThroughput,
                                                     vec4 accumulatedRadiance,
                                                     out bool spawned) {
  spawned = false;
  GpuDiffusePathStateRecord next = path;
  next.accumulatedRadiance = accumulatedRadiance;
  if (throughputIsBlack(continuationThroughput)) {
    next.flags = terminatedPathFlags(path.flags);
    next.throughput = vec4(0.0);
    return next;
  }
  const vec3 normal = normalize(hit.normal.xyz);
  const vec3 direction = normalize(reflect(normalize(path.ray.direction.xyz), normal));
  next.ray = continuationRay(hit, path, direction);
  next.throughput = continuationThroughput;
  next.depth = path.depth + 1u;
  next.previousBsdfPdf = 1.0;
  next.previousLightPdf = 0.0;
  next.previousMaterial = hit.material;
  next.previousEventFlags = gpuDiffusePathStateSampledFromBsdfFlag |
                            gpuDiffusePathStateBsdfSampleDeltaFlag;
  next.flags = activePathFlags(path.flags);
  spawned = true;
  return next;
}

float transparentEta(GpuTracingMaterialRecord material, vec3 wi, vec3 normal) {
  const float ior = material.transmissionParameters.y;
  return dot(normal, wi) < 0.0 ? 1.0 / ior : ior;
}

bool transparentTotalInternalReflection(GpuTracingMaterialRecord material, vec3 wi,
                                        vec3 normal) {
  const float cosTheta = dot(normal, wi);
  const float eta = transparentEta(material, wi, normal);
  return 1.0 - (1.0 - cosTheta * cosTheta) / (eta * eta) < 0.0;
}

vec3 transparentRefract(vec3 incident, vec3 normal, float eta) {
  const float cosTheta = dot(incident, normal);
  const float cosTheta2 =
      sqrt(max(0.0, 1.0 - (1.0 - cosTheta * cosTheta) / (eta * eta)));
  return normalize(-(incident / eta) - normal * (cosTheta2 - cosTheta / eta));
}

vec3 transparentTransmissionDirection(GpuTracingMaterialRecord material, vec3 wi,
                                      vec3 normal) {
  vec3 orientedNormal = normal;
  float eta = material.transmissionParameters.y;
  if (dot(orientedNormal, wi) < 0.0) {
    orientedNormal = -orientedNormal;
    eta = 1.0 / eta;
  }
  return transparentRefract(wi, orientedNormal, eta);
}

GpuDiffusePathStateRecord transparentContinuationPath(GpuIntersectionHitRecord hit,
                                                      GpuDiffusePathStateRecord path,
                                                      GpuTracingMaterialRecord material,
                                                      vec4 accumulatedRadiance,
                                                      out bool spawned) {
  spawned = false;
  GpuDiffusePathStateRecord next = path;
  next.accumulatedRadiance = accumulatedRadiance;
  const vec3 normal = normalize(hit.normal.xyz);
  const vec3 wi = -normalize(path.ray.direction.xyz);
  vec3 direction = normalize(reflect(normalize(path.ray.direction.xyz), normal));
  vec4 branchWeight = vec4(1.0);
  if (!transparentTotalInternalReflection(material, wi, normal)) {
    const float reflection = max(0.0, material.continuationParameters.w);
    const float transmission = max(0.0, material.transmissionParameters.x);
    const float total = reflection + transmission;
    if (total <= 0.0) {
      next.flags = terminatedPathFlags(path.flags);
      next.throughput = vec4(0.0);
      return next;
    }
    const float reflectionWeight = reflection / total;
    const float selector = clamp(sample2D(path, sampleDimension(path, 0u)).x, 0.0, 1.0);
    if (reflectionWeight > 0.0 && selector < reflectionWeight) {
      branchWeight = mirrorReflectance(material) * (1.0 / reflectionWeight);
    } else {
      const float transmissionWeight = 1.0 - reflectionWeight;
      if (transmissionWeight <= 0.0) {
        next.flags = terminatedPathFlags(path.flags);
        next.throughput = vec4(0.0);
        return next;
      }
      const float eta = transparentEta(material, wi, normal);
      direction = transparentTransmissionDirection(material, wi, normal);
      branchWeight = vec4(material.transmissionParameters.x /
                          (eta * eta * transmissionWeight));
    }
  }
  const vec4 continuationThroughput = path.throughput * branchWeight;
  if (throughputIsBlack(continuationThroughput)) {
    next.flags = terminatedPathFlags(path.flags);
    next.throughput = vec4(0.0);
    return next;
  }
  next.ray = continuationRay(hit, path, direction);
  next.throughput = continuationThroughput;
  next.depth = path.depth + 1u;
  next.previousBsdfPdf = 1.0;
  next.previousLightPdf = 0.0;
  next.previousMaterial = hit.material;
  next.previousEventFlags = gpuDiffusePathStateSampledFromBsdfFlag |
                            gpuDiffusePathStateBsdfSampleDeltaFlag;
  next.flags = activePathFlags(path.flags);
  spawned = true;
  return next;
}

GpuDiffusePathStepRecord inactiveStep(uint pathIndex, GpuDiffusePathStateRecord path) {
  GpuDiffusePathStepRecord step;
  step.event = gpuDiffusePathStepEventInactive;
  step.pathIndex = pathIndex;
  step.pixelIndex = path.pixelIndex;
  step.primarySampleIndex = path.primarySampleIndex;
  step.depth = path.depth;
  step.material = 0u;
  step.object = 0u;
  step.flags = path.flags;
  step.emittedRadiance = vec4(0.0);
  step.directLightRadiance = vec4(0.0);
  step.missRadiance = vec4(0.0);
  step.continuationThroughput = vec4(0.0);
  step.directLightSampleCount = 0u;
  step.directLightVisibilityRayCount = 0u;
  step.directLightContributingSampleCount = 0u;
  step.directLightOccludedSampleCount = 0u;
  return step;
}

GpuDiffusePathStateRecord terminatedPathWithAccumulatedRadiance(GpuDiffusePathStateRecord path,
                                                                vec4 accumulatedRadiance) {
  GpuDiffusePathStateRecord next = path;
  next.accumulatedRadiance = accumulatedRadiance;
  next.flags = terminatedPathFlags(path.flags);
  return next;
}

GpuDiffusePathStepRecord pathStep(uint pathIndex, GpuDiffusePathStateRecord path,
                                  out GpuIntersectionHitRecord hit,
                                  out GpuDiffusePathStateRecord next) {
  next = path;
  GpuDiffusePathStepRecord step = inactiveStep(pathIndex, path);
  hit = missHitRecord(path.ray);
  if (!pathStateIsActive(path)) {
    return step;
  }

  hit = closestSupportedHit(path.ray);
  if (hit.hit == 0u) {
    const vec4 contribution = path.throughput * missRadiance(path);
    next = terminatedPathWithAccumulatedRadiance(path, path.accumulatedRadiance + contribution);
    step.event = gpuDiffusePathStepEventMiss;
    step.missRadiance = contribution;
    step.flags = next.flags;
    return step;
  }

  step.event = gpuDiffusePathStepEventHit;
  step.material = hit.material;
  step.object = hit.object;

  if (hit.material < parameters.materialCount) {
    const GpuTracingMaterialRecord hitMaterial = readMaterial(hit.material);
    if (hitMaterial.kind == gpuTracingPortalMaterialKind) {
      bool spawned = false;
      next = portalContinuationPath(hit, path, hitMaterial, path.accumulatedRadiance, spawned);
      step.continuationThroughput = spawned ? next.throughput : vec4(0.0);
      step.flags = next.flags;
      return step;
    }
  }

  GpuTracingMaterialRecord material;
  vec4 reflectance;
  if (surfaceMaterial(hit, material) && diffuseReflectance(hit, reflectance)) {
    vec4 ambient;
    if (!matteAmbientRadiance(hit, path, ambient)) {
      step.event = gpuDiffusePathStepEventUnsupported;
      next.flags = unsupportedPathFlags(path.flags);
      step.flags = next.flags;
      return step;
    }
    const DirectLightEstimate directLight = directLightEstimate(hit, path, material, reflectance);
    const vec4 accumulatedRadiance =
        path.accumulatedRadiance + ambient + directLight.radiance;
    bool spawned = false;
    if (material.kind == gpuTracingReflectiveMaterialKind) {
      next = reflectiveContinuationPath(hit, path, path.throughput * mirrorReflectance(material),
                                        accumulatedRadiance, spawned);
    } else if (material.kind == gpuTracingTransparentMaterialKind) {
      next = transparentContinuationPath(hit, path, material, accumulatedRadiance, spawned);
    } else {
      next = matteContinuationPath(hit, path, material, reflectance, accumulatedRadiance, spawned);
    }
    step.directLightRadiance = directLight.radiance;
    step.directLightSampleCount = directLight.sampleCount;
    step.directLightVisibilityRayCount = directLight.visibilityRayCount;
    step.directLightContributingSampleCount = directLight.contributingSampleCount;
    step.directLightOccludedSampleCount = directLight.occludedSampleCount;
    step.continuationThroughput = spawned ? next.throughput : vec4(0.0);
    step.flags = next.flags;
    return step;
  }

  vec4 emitted;
  if (emissiveContribution(hit, path, emitted)) {
    next = terminatedPathWithAccumulatedRadiance(path, path.accumulatedRadiance + emitted);
    step.emittedRadiance = emitted;
    step.flags = next.flags;
    return step;
  }

  next.flags = unsupportedPathFlags(path.flags);
  step.event = gpuDiffusePathStepEventUnsupported;
  step.flags = next.flags;
  return step;
}

uint pixelCount() {
  return parameters.imageWidth * parameters.imageHeight;
}

uint accumulationIndexFor(uint pathIndex, GpuDiffusePathStateRecord path) {
  if (parameters.accumulationTargetMode == gpuDiffusePathLoopAccumulationTargetPath) {
    return pathIndex;
  }
  if (parameters.accumulationTargetMode == gpuDiffusePathLoopAccumulationTargetSampleSlot) {
    return path.pixelIndex * parameters.imageHeight + path.primarySampleIndex;
  }
  return path.pixelIndex;
}

void writeAccumulation(uint pathIndex, GpuDiffusePathStateRecord path) {
  const uint accumulationIndex = accumulationIndexFor(pathIndex, path);
  if (accumulationIndex >= pixelCount()) {
    return;
  }
  const vec4 color = path.accumulatedRadiance;
  const uint colorWordOffset = accumulationIndex * 4u;
  accumulationWords[colorWordOffset + 0u] = floatBitsToUint(color.x);
  accumulationWords[colorWordOffset + 1u] = floatBitsToUint(color.y);
  accumulationWords[colorWordOffset + 2u] = floatBitsToUint(color.z);
  accumulationWords[colorWordOffset + 3u] = floatBitsToUint(color.w);
  accumulationWords[pixelCount() * 4u + accumulationIndex] = 1u;
}

void recordRetainedActivePath(uint pathIndex, GpuDiffusePathStateRecord path) {
  if (!pathStateIsActive(path)) {
    return;
  }
  const uint slot = atomicAdd(retainedIndices[0], 1u);
  retainedIndices[slot + 1u] = pathIndex;
}
