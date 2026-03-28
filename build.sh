#!/bin/bash
# Switch 빌드 (커스텀 ffmpeg + Borealis 앱)
# named volume으로 portlibs를 캐싱하여 ffmpeg 재빌드를 방지합니다.
#
# 사용법:
#   ./build.sh              # 전체 빌드 (ffmpeg 캐시 있으면 건너뜀)
#   ./build.sh --clean      # portlibs 캐시 삭제 후 전체 빌드
#   ./build.sh --app-only   # 앱만 빌드 (ffmpeg 이미 캐시된 경우)

set -e

VOLUME_NAME="switch-chzzk-portlibs"

if [ "$1" = "--clean" ]; then
    echo "Removing portlibs cache volume..."
    sg docker -c "docker volume rm $VOLUME_NAME 2>/dev/null || true"
    shift
fi

# named volume 생성 (없으면)
sg docker -c "docker volume create $VOLUME_NAME 2>/dev/null || true"

echo "=== Building switchzzk (portlibs cached in volume: $VOLUME_NAME) ==="

sg docker -c "docker run --rm \
  -v \"\$PWD:/work\" \
  -v $VOLUME_NAME:/opt/devkitpro/portlibs/switch \
  -w /work \
  devkitpro/devkita64 \
  bash -lc '
    # 커스텀 ffmpeg (캐시 있으면 자동 건너뜀)
    ./scripts/switch/build_custom_portlibs.sh --skip-mpv --skip-app

    # Borealis 앱 빌드
    source /opt/devkitpro/switchvars.sh
    export PATH=/opt/devkitpro/devkitA64/bin:\$PATH
    cmake -B cmake-build-switch \
      -DCMAKE_BUILD_TYPE=Release \
      -DPLATFORM_SWITCH=ON \
      -DUSE_DEKO3D=OFF \
      2>&1 | tail -5
    cmake --build cmake-build-switch -j\$(nproc) --target switchzzk.nro 2>&1 | tail -10
  '
"

echo ""
echo "=== Build complete ==="
ls -la cmake-build-switch/switchzzk.nro 2>/dev/null
