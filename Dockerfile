# syntax=docker/dockerfile:1
# SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
ARG NATIVE_IMAGE=ghcr.io/ekmett/native:latest
FROM ${NATIVE_IMAGE} AS build
WORKDIR /src/ftz
COPY . .
RUN cmake -S . -B /tmp/ftz-build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/ftz \
      -DFTZ_BUILD_TESTS=OFF \
 && cmake --build /tmp/ftz-build --parallel \
 && cmake --install /tmp/ftz-build

FROM ${NATIVE_IMAGE}
COPY --from=build /opt/ftz /opt/ftz
ENV CMAKE_PREFIX_PATH="/opt/ftz:/opt/native"
RUN --mount=type=bind,source=tests,target=/tmp/ftz-tests \
    cmake -S /tmp/ftz-tests/api -B /tmp/ftz-check -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build /tmp/ftz-check --parallel \
 && ctest --test-dir /tmp/ftz-check --output-on-failure \
 && rm -rf /tmp/ftz-check
WORKDIR /workspace
LABEL org.opencontainers.image.source="https://github.com/ekmett/ftz" \
      org.opencontainers.image.title="ftz" \
      org.opencontainers.image.description="FTZ on Native's LLVM 23 development image" \
      org.opencontainers.image.licenses="BSD-2-Clause OR Apache-2.0"
