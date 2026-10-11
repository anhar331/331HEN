#!/usr/bin/env bash
# 331HEN reproducible Linux build environment. Keeps upstream credits/licenses.
set -euo pipefail
app_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
app_env="${app_root}/.docker/331hen"
mkdir -p "${app_env}" "${app_root}/.cache/dependencies"
fetch_verified() {
  local url="$1" dest="$2" expected="$3"
  if [[ ! -f "${dest}" ]] || [[ "$(sha256sum "${dest}" | cut -d ' ' -f1)" != "${expected}" ]]; then
    curl --fail --location --retry 3 --output "${dest}.tmp" "${url}"
    printf '%s  %s\n' "${expected}" "${dest}.tmp" | sha256sum --check
    mv "${dest}.tmp" "${dest}"
  fi
}
fetch_verified 'https://github.com/ps5-payload-dev/sdk/releases/download/v0.44/ps5-payload-sdk.zip' \
  "${app_env}/sdk.zip" '4d06eab777bb135705b5a6816688c9132efb246093fe606ac488ab6cf65ca5b0'
fetch_verified 'https://github.com/illusionyy/ps5-pacbrew-repo/releases/download/15413830176/ps5-payload-libs.tar.gz' \
  "${app_env}/libs.tar.gz" '8d1ce2dfb5aacec65af28fbf01c20fe0940a86187439b9f63adf9d8972b1ead0'
fetch_verified 'https://github.com/EchoStretch/kstuff-lite/releases/download/v1.10/kstuff.elf' \
  "${app_root}/.cache/dependencies/kstuff.elf" '82cbb1e70f8a562317d6b65269cf6aa0d7d94e22df4f1ac89a260ca0ec261252'
mkdir -p "${app_env}/sdk" "${app_env}/libs"
unzip -q -o "${app_env}/sdk.zip" -d "${app_env}/sdk"
tar -xzf "${app_env}/libs.tar.gz" -C "${app_env}/libs"
app_ca="${CODEX_PROXY_CERT:-/etc/ssl/certs/ca-certificates.crt}"
BUILDX_CONFIG="${app_env}/buildx" docker build \
  --secret "id=proxy_ca,src=${app_ca}" \
  -f "${app_root}/Dockerfile" -t 331hen-builder "${app_env}"
docker run --rm --user "$(id -u):$(id -g)" \
  --mount "type=bind,src=${app_root},dst=/workspace" \
  -e GIT_CONFIG_COUNT=1 -e GIT_CONFIG_KEY_0=safe.directory -e GIT_CONFIG_VALUE_0=/workspace \
  331hen-builder bash scripts/build.sh --release --jobs "${JOBS:-3}" --skip-dependency-sync
mkdir -p "${app_root}/Release"
cp "${app_root}/build/bin/331HEN.elf" "${app_root}/Release/331HEN.elf"
(cd "${app_root}/Release" && sha256sum 331HEN.elf > SHA256.txt)
