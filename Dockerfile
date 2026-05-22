# syntax=docker/dockerfile:1.7
#
# Two-stage build: a fat builder image with cmake / clang / Qt 6 dev headers
# produces the rendercli binary, then we copy it into a distroless runtime.
# The Modeler is intentionally excluded because it needs a display server and
# isn't suitable for headless cloud workloads (modernize.md §3.9).

ARG UBUNTU_VERSION=24.04

FROM ubuntu:${UBUNTU_VERSION} AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      ca-certificates \
      cmake \
      ninja-build \
      git \
      g++-13 \
      qt6-base-dev \
      qt6-declarative-dev \
      libgl1-mesa-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Build only the rendercli tool. The Modeler and tests are out of scope for the
# runtime image, so we skip the GUI executable and the GoogleTest fetch.
RUN cmake -S . -B build \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=g++-13 \
      -DRAYTRACER_ENABLE_NATIVE_TUNING=OFF \
      -DRAYTRACER_BUILD_TESTS=OFF \
      -DRAYTRACER_BUILD_MODELER=OFF \
      -DRAYTRACER_BUILD_TOOLS=ON && \
    cmake --build build --target rendercli --parallel

# Runtime: distroless gives us libc + libstdc++ but no shell, no apt, no
# package manager. Qt's runtime libs come along for the ride because the
# library is dynamically linked.
FROM gcr.io/distroless/cc-debian12:nonroot AS runtime

# Qt runtime shared objects from the builder. The exact set comes from the
# rendercli ldd output; if rendercli's link surface grows, expand this list.
COPY --from=builder \
  /usr/lib/x86_64-linux-gnu/libQt6Core.so.6 \
  /usr/lib/x86_64-linux-gnu/libQt6Gui.so.6 \
  /usr/lib/x86_64-linux-gnu/libQt6Qml.so.6 \
  /usr/lib/x86_64-linux-gnu/libQt6Widgets.so.6 \
  /usr/lib/x86_64-linux-gnu/

COPY --from=builder /src/build/tools/rendercli/rendercli /usr/local/bin/rendercli

ENTRYPOINT ["/usr/local/bin/rendercli"]
