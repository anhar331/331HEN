# Build context: .docker/331hen, prepared by scripts/build-container.sh.
# syntax=docker/dockerfile:1
FROM ubuntu:24.04@sha256:534baea6a22c03a63003dbc8dbe78fe34bc0d7e595d9a9dc9834884ff530eb55
ENV DEBIAN_FRONTEND=noninteractive
RUN --mount=type=secret,id=proxy_ca,required=true export SSL_CERT_FILE=/run/secrets/proxy_ca; apt-get update && apt-get install -y --no-install-recommends build-essential ca-certificates clang-18 cmake curl lld-18 llvm-18 ninja-build python3 python3-pyelftools librsvg2-bin git unzip xz-utils && rm -rf /var/lib/apt/lists/*
ENV PATH="/usr/lib/llvm-18/bin:/opt/ps5-payload-sdk/bin:${PATH}" PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk LLVM_CONFIG=/usr/lib/llvm-18/bin/llvm-config
COPY sdk/ /opt/
COPY libs/opt/ps5-payload-sdk/target/user/homebrew/ /opt/ps5-payload-sdk/target/user/homebrew/
WORKDIR /workspace
RUN chmod -R a+rX /opt/ps5-payload-sdk && chmod a+rx /opt/ps5-payload-sdk/bin/*
