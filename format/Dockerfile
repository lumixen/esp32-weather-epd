FROM alpine@sha256:48b0309ca019d89d40f670aa1bc06e426dc0931948452e8491e3d65087abc07d

ARG CLANG_FORMAT_PACKAGE=18.1.8-r2

RUN apk add --no-cache "clang18-extra-tools=${CLANG_FORMAT_PACKAGE}" \
    && /usr/lib/llvm18/bin/clang-format --version

WORKDIR /project
