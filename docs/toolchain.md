# Toolchain

## 지금 이 저장소에서 필요한 도구

### 실제 스위치 홈브류 빌드용

실제로 `.nro`를 만들려면 먼저 이것부터 있어야 한다.

- devkitPro pacman
- `switch-dev` 그룹
- `DEVKITPRO` 환경변수

공식 문서:

- devkitPro pacman: https://devkitpro.org/wiki/devkitPro_pacman
- Getting Started: https://devkitpro.org/wiki/Getting_Started/devkitPPC
- portlibs: https://devkitpro.org/wiki/portlibs

devkitPro 문서는 현재 Switch가 `devkitA64` 기반이라고 설명한다.

### 현재 프로젝트가 추가로 요구하는 것

이 저장소의 현재 `Makefile.switch`는 아래 라이브러리에 링크한다.

- `libnx`
- OpenSSL
- zlib

즉 `switch-dev` 설치 후에도 `-lssl`, `-lcrypto`, `-lz`가 안 잡히면 해당 switch portlibs를 추가 설치해야 한다.

이 환경에서는 devkitPro 패키지 서버 접근이 막혀서 정확한 패키지 설치 결과까지는 검증하지 못했다. 그래서 여기서는 “공식 `switch-dev` 설치 후, 부족한 switch portlibs를 추가”가 가장 안전한 표현이다.

### 호스트 검증용

현재 Ubuntu 호스트 빌드에 필요한 것은 다음이다.

- `bash`
- `make`
- `g++`
- OpenSSL 개발 라이브러리
- `curl`

이 환경에서는 이미 `make`, `g++`, OpenSSL 런타임이 있었고 `make host`는 성공했다.

## 권장 설치 순서

### Ubuntu / Debian / WSL

1. 공식 devkitPro pacman 설치
2. 쉘 재시작 또는 환경변수 반영
3. `switch-dev` 설치
4. 링크 에러가 나면 부족한 switch portlibs 추가
5. 이 저장소에서 vendor header 다운로드
6. 스위치 빌드 실행

예상 명령:

```bash
sudo dkp-pacman -Syu
sudo dkp-pacman -S switch-dev
./scripts/fetch_vendor_headers.sh
make -f Makefile.switch
```

## WSL 주의사항

공식 devkitPro 문서에는 WSL에서 `/etc/mtab` 심볼릭 링크가 필요할 수 있다고 적혀 있다.

```bash
sudo ln -s /proc/self/mounts /etc/mtab
```

이 저장소를 작업한 현재 WSL 환경에서는 추가로 devkitPro 패키지 서버가 Cloudflare에 막혀 설치를 진행하지 못했다. 같은 문제가 있으면 다음을 먼저 확인하는 게 맞다.

- 브라우저/호스트 OS에서 devkitPro 설치가 가능한지
- WSL 네트워크 정책/프록시
- Docker Desktop + devkitPro 이미지 사용 가능 여부

## 빠른 검증 순서

툴체인 설치 후 최소 검증은 이렇게 한다.

1. `sudo dkp-pacman -Syu`
2. `sudo dkp-pacman -S switch-dev`
3. `./scripts/fetch_vendor_headers.sh`
4. `make -f Makefile.switch`
5. 필요하면 그 뒤에 `make host`

## Docker 우회 빌드

Ubuntu 22.04 WSL에서 `glibc` 버전 때문에 최신 `devkita64-gcc`가 직접 실행되지 않을 수 있다. 이 저장소는 Docker Desktop + 공식 `devkitpro/devkita64` 이미지로 우회 빌드가 가능하다.

실제 성공한 명령:

```bash
sg docker -c 'docker run --rm -v "$PWD:/work" -w /work devkitpro/devkita64 bash -lc "make -f Makefile.switch"'
```

vendor 헤더까지 다시 받고 싶으면:

```bash
sg docker -c 'docker run --rm -v "$PWD:/work" -w /work devkitpro/devkita64 bash -lc "./scripts/fetch_vendor_headers.sh && make -f Makefile.switch"'
```

조건:

- Docker Desktop 실행
- Docker Desktop WSL integration 활성화
- 현재 사용자에게 Docker socket 접근 권한 반영

## 커스텀 portlibs 빌드

stock devkitPro `switch-ffmpeg/switch-libmpv`로는 HLS/HTTPS 스트리밍이 동작하지 않는다. 이 프로젝트는 `reference/TsVitch`를 기반으로 커스텀 빌드를 사용한다.

### 필요 의존성 (Docker 이미지 내 dkp-pacman으로 설치됨)

- `switch-zlib`, `switch-bzip2`
- `switch-mbedtls` (TLS)
- `switch-libass`, `switch-freetype`, `switch-harfbuzz`, `switch-libfribidi` (자막/폰트)
- `switch-mesa` (mpv video output)
- `switch-sdl2`, `switch-curl`
- `dkp-meson-scripts` (mpv meson 빌드)
- `meson` (apt로 설치)

### 빌드 방법

Docker 컨테이너 내에서 오케스트레이션 스크립트를 실행한다:

```bash
sg docker -c 'docker run --rm -v "$PWD:/work" -w /work devkitpro/devkita64 \
  bash -lc "./scripts/switch/build_custom_portlibs.sh"'
```

이 스크립트는 다음을 순서대로 수행한다:

1. dkp-pacman으로 의존성 설치
2. FFmpeg 7.1 다운로드 + 패치 + 빌드 + 설치 (→ `$PORTLIBS_PREFIX`)
3. mpv 0.36.0 다운로드 + 패치 + 빌드 + 설치 (→ `$PORTLIBS_PREFIX`)
4. `make -f Makefile.switch`로 `.nro` 빌드

stock portlibs를 커스텀 빌드로 덮어쓰므로 pkg-config가 자동으로 커스텀 라이브러리를 잡는다.

### 앱만 다시 빌드할 때

ffmpeg/mpv가 이미 빌드된 Docker 볼륨이 있으면:

```bash
sg docker -c 'docker run --rm -v "$PWD:/work" -w /work devkitpro/devkita64 \
  bash -lc "./scripts/switch/build_custom_portlibs.sh --skip-ffmpeg --skip-mpv"'
```

단, `--rm`으로 컨테이너가 삭제되면 커스텀 portlibs도 사라진다. 앱만 다시 빌드하려면 매번 ffmpeg/mpv도 다시 빌드해야 한다. 이를 피하려면 named volume 사용을 검토할 것.

### Makefile.switch 링크 구성

```makefile
LIBS := $(MPV_SDL_LIBS) $(FFMPEG_LIBS) -lGLESv2 -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lz -lnx
```

커스텀 ffmpeg가 `mbedtls`에 의존하므로 `-lmbedtls -lmbedx509 -lmbedcrypto`가 명시적으로 추가됐다.
