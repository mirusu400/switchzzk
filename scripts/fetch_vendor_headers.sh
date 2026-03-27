#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

mkdir -p "$ROOT_DIR/third_party/httplib" "$ROOT_DIR/third_party/nlohmann"

curl -fsSL \
  https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.18.5/httplib.h \
  -o "$ROOT_DIR/third_party/httplib/httplib.h"

curl -fsSL \
  https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp \
  -o "$ROOT_DIR/third_party/nlohmann/json.hpp"
