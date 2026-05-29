#pragma once

/*
 * Canonical SIMD feature gates for raytracer.
 *
 * These macros describe compile-time availability of target intrinsics in the
 * current translation unit. They intentionally mirror the compiler feature
 * macros used before this header existed, so existing x86 SSE/SSE3 behavior is
 * unchanged while ARM NEON code can use the same project-level convention.
 */

#ifndef RAYTRACER_SIMD_SSE
#if defined(__SSE__)
#define RAYTRACER_SIMD_SSE 1
#else
#define RAYTRACER_SIMD_SSE 0
#endif
#endif

#ifndef RAYTRACER_SIMD_SSE2
#if defined(__SSE2__)
#define RAYTRACER_SIMD_SSE2 1
#else
#define RAYTRACER_SIMD_SSE2 0
#endif
#endif

#ifndef RAYTRACER_SIMD_SSE3
#if defined(__SSE3__)
#define RAYTRACER_SIMD_SSE3 1
#else
#define RAYTRACER_SIMD_SSE3 0
#endif
#endif

#ifndef RAYTRACER_SIMD_AVX
#if defined(__AVX__)
#define RAYTRACER_SIMD_AVX 1
#else
#define RAYTRACER_SIMD_AVX 0
#endif
#endif

#ifndef RAYTRACER_SIMD_NEON
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define RAYTRACER_SIMD_NEON 1
#else
#define RAYTRACER_SIMD_NEON 0
#endif
#endif
