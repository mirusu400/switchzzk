# Playback

## 현재 조사 결과

실제 Switch 빌드 환경인 `devkitpro/devkita64` 컨테이너 기준으로 다음 포트 라이브러리가 이미 들어 있다.

- `switch-ffmpeg`
- `switch-libmpv`
- `switch-sdl2`
- `switch-curl`

즉 재생기를 새로 포팅해야 하는 상태는 아니고, 현재 저장소에서 가장 현실적인 경로는 `libmpv + SDL2 + OpenGL ES`다.

## 왜 이 경로를 선택했는가

- `libmpv`는 이미 FFmpeg, 네트워크 스트리밍, HLS 처리를 포함한 상위 플레이어 레이어를 제공한다.
- 현재 앱은 이미 CHZZK HLS/LL-HLS URL 해석까지 구현되어 있다.
- 따라서 앱이 해야 할 일은 “최종 URL을 재생기에 넘기고, Switch 화면에 렌더링하는 것”이다.
- Switch 포트 라이브러리에 `SDL2`, `EGL`, `libmpv`가 함께 있으므로 최소 구현으로 붙이기 좋다.

## 구현 전략

1. 기존 콘솔 UI는 목록/선택용으로 유지
2. 사용자가 라이브를 선택하면 `resolved.selected_url`을 얻음
3. Switch 전용 코드에서 SDL2 전체화면 창 + GLES2 컨텍스트 생성
4. `libmpv` render API로 기본 framebuffer에 렌더링
5. `B` 버튼으로 재생 종료 후 앱으로 복귀
6. `A` 버튼으로 pause toggle

## 현재 구현 상태

- `src/switch/switch_player.cpp`에 Switch 전용 SDL2/mpv 플레이어가 추가됐다.
- 목록 화면에서 라이브 선택 후 재생 URL 해석까지 성공하면 이 플레이어로 진입한다.
- 실기 크래시 원인 파악을 위해 `sdmc:/switch/switch_chzzk.log`에 단계별 로그를 남긴다.
- `mpv` 로그와 `MPV_EVENT_END_FILE` reason/error도 `sdmc:/switch/switch_chzzk.log`에 남긴다.
- 현재 조작은 다음 두 개만 있다.
  - `A`: pause toggle
  - `B`: 재생 종료 후 앱으로 복귀

## 빌드 상태

다음 Docker 명령으로 `.nro` 빌드가 성공했다.

```bash
sg docker -c 'docker run --rm -v "$PWD:/work" -w /work devkitpro/devkita64 bash -lc "make -f Makefile.switch"'
```

이 시점의 의미는 다음과 같다.

- Switch 포트 라이브러리와 링크까지는 성공
- `libmpv + SDL2 + OpenGL ES` 통합 코드는 컴파일됨
- 아직 실기에서 실제 영상이 출력되는지는 미확인

## 2026-03-27 실기 조사 메모

- 첫 실기 테스트에서는 라이브 목록은 뜨지만 한글 문자열이 콘솔 폰트 한계로 깨져 보였다.
- `A` 입력 후에는 `SDL`/`mpv` 초기화 자체는 성공했다.
- 당시 로그는 `loadfile -> entering loop -> mpv end/shutdown`까지 기록됐고, 즉시 홈브류 에러 화면으로 빠졌다.
- 이 패턴상 첫 의심 지점은 `consoleExit -> SDL/mpv -> consoleInit` 전환과 `mpv` 종료 cleanup 경로다.
- 그래서 현재 코드는 콘솔 재초기화 왕복을 제거했고, `mpv` 종료 reason/error와 cleanup 단계를 추가 기록한다.
- 이후 실기 로그에서 `SDL_CreateWindow failed: Could not set NWindow dimensions: 0xf59`가 확인됐다.
- 따라서 현재 플레이어는 `appletGetOperationMode()` 기준으로 도킹이면 `1920x1080`, 휴대 모드면 `1280x720`을 우선 사용하고, 실패 시 `1280x720`으로 한 번 더 폴백한다.
- `reference/TsVitch`를 보면 Switch 쪽은 raw 콘솔 UI 대신 `borealis`의 `Application::createWindow()`로 앱 창을 먼저 잡고 그 위에 `libmpv`를 붙인다.
- 즉 현재 MVP처럼 `libnx console -> SDL 창`을 그때그때 왕복하는 구조는 본질적으로 불안정하고, 장기적으로는 SDL/Borealis 기반 상시 UI로 옮겨야 한다.
- 단기적으로는 실기 로그상 `console`이 살아 있는 동안 `SDL_CreateWindow`가 실패하므로, 재생 진입 전에 `consoleExit()`가 필요하다.
- 최신 실기 로그에서는 `player cleanup done -> console reinit done -> player returned ok=true`까지 기록됐다.
- 즉 홈브류 크래시 지점은 이제 플레이어 내부가 아니라 `console` 복귀 이후 메인 루프다.
- 따라서 현재는 `consoleSelect`, `padConfigureInput`, `padInitializeDefault`를 복귀 직후 다시 호출하고, 메인 루프의 `print`/`consoleUpdate` 지점까지 추가 로그를 남긴다.
- 그 다음 로그에서는 복귀 이후 메인 루프도 정상적으로 계속 돌았다.
- 즉 현재 실기 문제는 “앱 크래시”가 아니라 `mpv`의 `END_FILE reason=error error=-13 loading failed` 하나로 좁혀졌다.
- 이 상태는 `reference/TsVitch`가 Switch용 `ffmpeg` 네트워크 패치를 별도로 들고 있는 점과도 맞물린다. stock devkitPro `switch-ffmpeg/libmpv` 조합이 HTTPS HLS 로드에서 제한될 가능성이 있다.
- 그래서 현재 코드는 `mpv`에 넘기기 전에 같은 HLS URL을 앱의 `libcurl` 경로로 preflight GET 해서, CDN 접근 가능 여부를 로그로 먼저 남긴다.
- 최신 실기 로그에서는 이 preflight 단계도 실패했다. 즉 현재는 `mpv` 이전에 앱의 Switch `libcurl`이 해당 HLS CDN URL을 못 받고 있다.
- 그래서 `reference/TsVitch`의 Switch 다운로드 경로를 참고해 `User-Agent`, `Referer`, `Accept-Encoding: identity`, `AUTOREFERER`, 보수적 timeout/buffer/TCP 옵션, TLS verify off를 `HttpsHttpClient`에 반영했다.
- 이제 `sdmc:/switch/switch_chzzk.log`에는 `http: GET ok ...` 또는 `http: GET failed result=... status=... error_buffer=...`가 같이 남는다.
- 이후 원격 로그를 확인한 결과, CHZZK API와 마스터 플레이리스트 fetch는 `200 OK`인데 선택된 variant URL만 `403/400`이었다.
- 원인은 `m3u8` 상대 경로 해석 버그였다. 기존 코드는 `master_url`의 `?query`를 제거하지 않고 상대 경로를 붙여서, 실제 variant/chunklist URL이 query 문자열 뒤에 잘못 이어졌다.
- 따라서 `resolve_relative_url()`는 이제 base URL의 query/fragment를 먼저 제거한 뒤 상대 경로를 붙인다.
- 그 다음 원격 로그에서는 selected chunklist URL preflight도 `200 OK`였다.
- 즉 남은 문제는 `mpv`가 같은 URL을 열 때만 `loading failed`가 나는 점이다.
- 현재는 앱이 preflight에 사용한 것과 맞추기 위해 `mpv`에도 `referrer`와 `http-header-fields`를 명시적으로 넣는다.
- 최신 실기 로그에서도 결과는 동일했다. `libcurl` preflight는 selected chunklist URL에 대해 반복적으로 `200 OK`를 받지만, 같은 URL을 `mpv`에 넘기면 항상 `MPV_END_FILE reason=error error=-13 loading failed`로 끝난다.
- 즉 현재 병목은 앱 로직이 아니라 `stock devkitPro switch-libmpv/switch-ffmpeg` 조합의 HLS/HTTPS 로드 경로다.
- 실질적인 다음 선택지는 두 가지다.
  1. `TsVitch`처럼 Switch용 커스텀 `ffmpeg/mpv` 패치 빌드를 도입한다.
  2. 앱이 직접 `m3u8`/segment를 `libcurl`로 받아서 별도 재생 백엔드로 넘기는 구조로 바꾼다.
- 현재 기준으로는 1번이 더 현실적이다. 이미 `reference/TsVitch/scripts/switch/ffmpeg/*` 와 `reference/TsVitch/scripts/switch/mpv/*` 에 필요한 패치와 PKGBUILD가 있다.

## 커스텀 ffmpeg/mpv 빌드

stock devkitPro `switch-ffmpeg/switch-libmpv`로는 CHZZK HLS URL을 `mpv`에서 열 수 없었다. 원인은 두 가지다:

1. **DNS 해석 누락**: Switch(libnx)에 `getaddrinfo`는 있지만 `getnameinfo`가 없어서, ffmpeg의 네트워크 스택이 호스트 이름 해석에 실패한다.
2. **HLS/HTTPS 프로토콜 비활성화**: stock 빌드에서 HLS, HTTPS, TLS 프로토콜이 활성화되어 있지 않을 수 있다.

`reference/TsVitch`의 빌드 스크립트를 참고해 커스텀 빌드를 도입했다.

### ffmpeg 7.1 커스텀 빌드

- **ffmpeg.patch**: Horizon OS 타깃 인식 + nvtegra 하드웨어 가속 (H.264, H.265, VP9 등)
- **network.patch**: `ff_getnameinfo()` 커스텀 구현으로 Switch DNS 해석 수정
- 프로토콜: `file,http,tcp,udp,rtmp,hls,https,tls,ftp,rtp,crypto,httpproxy` 활성화
- TLS: `mbedtls` 사용 (OpenSSL 대신)
- 의존성: `switch-zlib`, `switch-bzip2`, `switch-mbedtls`, `switch-libass`, `switch-freetype`, `switch-harfbuzz`, `switch-libfribidi`

### mpv 0.36.0 커스텀 빌드

- **mpv.patch** 적용:
  - `ao_hos`: Horizon OS 오디오 드라이버 (libnx audren, S16 48kHz, 최대 5.1ch)
  - VIC 정렬: 비디오 버퍼 256바이트 정렬 (`MP_IMAGE_BYTE_ALIGN` 64→256)
  - `mmap` shim: `malloc/free`로 대체
  - FFmpeg 61+ API 호환성 처리
- meson 빌드: `-Dhos=enabled -Dhos-audio=enabled -Dlibmpv=true -Dcplayer=false`

### 빌드 명령

```bash
sg docker -c 'docker run --rm -v "$PWD:/work" -w /work devkitpro/devkita64 \
  bash -lc "./scripts/switch/build_custom_portlibs.sh"'
```

옵션:
- `--with-nvtegra`: nvtegra 하드웨어 가속 활성화 (기본 비활성)
- `--skip-ffmpeg`: ffmpeg 빌드 건너뛰기
- `--skip-mpv`: mpv 빌드 건너뛰기
- `--skip-app`: 앱 빌드 건너뛰기

### 빌드 구조

```
scripts/switch/
├── build_custom_portlibs.sh   # 오케스트레이션 스크립트
├── ffmpeg/
│   ├── ffmpeg.patch           # Horizon OS + nvtegra (TsVitch 기반)
│   └── network.patch          # DNS getnameinfo 수정
└── mpv/
    └── mpv.patch              # HOS audio + VIC alignment + mmap shim
```

## 현재 한계

- 아직 OSD/컨트롤 UI가 없다.
- 탐색, 볼륨, 해상도 변경 UI는 없다.
- 재생 실패 원인을 화면에 자세히 노출하지 않는다.
- 커스텀 ffmpeg + stock mpv 조합으로 실기 재생 성공 확인 (2026-03-27).
- 커스텀 mpv는 stock portlibs와 ABI 불일치로 크래시하므로 사용하지 않는다.
- host 경로는 `httplib`, switch 경로는 `libcurl`로 분기되어 있어 플레이어 검증은 Switch 빌드 기준이 더 중요하다.

## 참고 근거

- `reference/TsVitch/scripts/switch/ffmpeg/PKGBUILD` — ffmpeg 빌드 레시피
- `reference/TsVitch/scripts/switch/mpv/PKGBUILD` — mpv 빌드 레시피
- `reference/TsVitch/scripts/switch/ffmpeg/network.patch` — DNS 수정 원본
- `reference/TsVitch/scripts/switch/mpv/mpv.patch` — HOS 오디오/정렬 원본

## 다음 체크포인트

1. 실기에서 영상이 실제로 출력되는지 확인
2. 오디오가 정상 출력되는지 확인
3. 종료 후 원래 앱 UI로 복귀되는지 확인
4. nvtegra 활성화 빌드로 하드웨어 가속 성능 비교
5. 필요하면 해상도/LL-HLS/HLS 선택 UI를 재생 화면으로 옮김
