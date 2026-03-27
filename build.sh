#!/bin/bash
# Switch 빌드 (커스텀 ffmpeg + stock mpv + 앱)
sg docker -c 'docker run --rm -v "$PWD:/work" -w /work devkitpro/devkita64 bash -lc "./scripts/switch/build_custom_portlibs.sh --skip-mpv"'
