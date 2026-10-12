FROM 331hen-builder
USER root
RUN apt-get update && apt-get install -y --no-install-recommends \
    glslang-tools spirv-tools python3-yaml python3-mako wget \
    && rm -rf /var/lib/apt/lists/*
