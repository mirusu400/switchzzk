# Handoff

## 현재 결론

- 앱 크래시는 해결됐다.
- CHZZK API 호출은 Switch 실기에서 정상 동작한다.
- 라이브 상세 조회도 정상 동작한다.
- 마스터 M3U8 fetch도 정상 동작한다.
- 선택된 variant/chunklist URL preflight도 `libcurl`로 `200 OK`를 받는다.
- 즉 URL 해석 로직은 현재 정상이다.
- stock devkitPro `switch-libmpv/switch-ffmpeg`에서 `mpv loadfile`이 `loading failed`로 끝나던 문제를 해결하기 위해 TsVitch 기반 커스텀 ffmpeg/mpv 빌드를 도입했다.
- 커스텀 빌드는 DNS(getnameinfo) 수정, HLS/HTTPS 프로토콜 활성화, mbedtls TLS, HOS 오디오 드라이버를 포함한다.
- `.nro` 빌드까지 성공했고, FTP로 실기에 업로드된 상태다.

## 실기 로그로 확인된 사실

실기 로그에서 반복 확인된 패턴:

1. `http: GET ok status=200` 으로 API/playlist/chunklist 접근 성공
2. `player: loadfile ...chunklist.m3u8`
3. `player: mpv start-file event`
4. `player: mpv end-file event reason=error error=-13 error_text=loading failed`

즉 앱의 네트워크 경로와 URL 조합은 살아 있고, `mpv` 로드 경로만 실패한다.

## 이미 반영된 수정

- `console -> SDL/mpv -> console` 왕복 시 크래시 나던 문제 수정
- `m3u8` 상대 경로 해석 시 query 문자열을 잘못 붙이던 버그 수정
- Switch `libcurl` preflight fetch 추가
- `mpv`에 `referrer`, `http-header-fields`, `user-agent`, `tls-verify=no` 전달 추가
- 실기 로그를 `sdmc:/switch/switch_chzzk.log`로 상세 기록
- 최신 `.nro`는 FTP로도 업로드 가능하도록 작업 흐름 정리

## 이번에 반영된 변경

1. `scripts/switch/build_custom_portlibs.sh` — Docker 내 ffmpeg/mpv 커스텀 빌드 + 앱 빌드 오케스트레이션
2. `scripts/switch/ffmpeg/ffmpeg.patch` — Horizon OS 타깃 + nvtegra 하드웨어 가속 (TsVitch 기반)
3. `scripts/switch/ffmpeg/network.patch` — `ff_getnameinfo()` Switch DNS 수정
4. `scripts/switch/mpv/mpv.patch` — HOS 오디오, VIC 256B 정렬, mmap shim
5. `Makefile.switch` — `$(FFMPEG_LIBS)` + mbedtls 링크 플래그 추가

## 2026-03-27 재생 성공 확인

커스텀 ffmpeg(HTTPS/HLS 프로토콜 + DNS 패치) + stock mpv 조합으로 실기 재생 성공.

실기 로그에서 확인된 사항:
- HLS demuxer: `Found 'hls' at score=100`
- 비디오: `h264 1280x720 60fps`, VO: `[libmpv] 1280x720 yuv420p`
- 오디오: `aac 2ch 48000Hz`, AO: `[sdl] 48000Hz stereo s16`
- `first video frame after restart shown` + `audio ready`

### 왜 stock ffmpeg가 실패했는가

실기 mpv 로그에서 확인:
```
No protocol handler found to open URL https://...
The protocol is either unsupported, or was disabled at compile-time.
```

stock devkitPro switch-ffmpeg에 HTTPS 프로토콜이 컴파일 안 되어 있었다.

### 왜 커스텀 mpv는 크래시했는가

커스텀 mpv 0.36.0 (libmpv 2.1.0)이 stock mpv (libmpv 2.3.0)보다 구버전이고, Docker 이미지의 libplacebo/다른 portlibs와 ABI 불일치로 `mpv_create()`에서 크래시.

### 현재 작동하는 빌드 조합

**커스텀 ffmpeg + stock mpv** (`--skip-mpv`):
```bash
sg docker -c 'docker run --rm -v "$PWD:/work" -w /work devkitpro/devkita64 \
  bash -lc "./scripts/switch/build_custom_portlibs.sh --skip-mpv"'
```

## 다음 단계 판단

우선순위:

1. SDL2 기반 UI로 전환 (libnx 콘솔 제거, 한글 폰트 지원)
2. 채널 목록 UI 개선 (선택 하이라이트, 상태바)
3. 재생 중 OSD/컨트롤 UI
4. 썸네일 로딩
5. 설정/즐겨찾기 저장

## 레퍼런스 포인트

- `reference/TsVitch/scripts/switch/ffmpeg/PKGBUILD`
- `reference/TsVitch/scripts/switch/ffmpeg/network.patch`
- `reference/TsVitch/scripts/switch/mpv/PKGBUILD`
- `reference/TsVitch/scripts/switch/mpv/mpv.patch`
- `reference/TsVitch/tsvitch/source/view/mpv_core.cpp`

## 새 프롬프트에서 먼저 읽을 파일

1. `AGENTS.MD`
2. `docs/playback.md`
3. `docs/toolchain.md`
4. `docs/handoff.md`

## 새 프롬프트에서 바로 쓸 명령

프로젝트 루트:

```bash
cd /home/ubuntu/lab/switch-chzzk
```

호스트 검증:

```bash
make host
./build/host/chzzk_host --network
```

커스텀 portlibs + Switch Docker 빌드 (전체):

```bash
sg docker -c 'docker run --rm -v "$PWD:/work" -w /work devkitpro/devkita64 bash -lc "./scripts/switch/build_custom_portlibs.sh"'
```

앱만 빌드 (커스텀 portlibs 이미 설치된 컨테이너에서):

```bash
sg docker -c 'docker run --rm -v "$PWD:/work" -w /work devkitpro/devkita64 bash -lc "./scripts/switch/build_custom_portlibs.sh --skip-ffmpeg --skip-mpv"'
```

FTP 업로드:

```bash
curl --silent --show-error --ftp-create-dirs -T /home/ubuntu/lab/switch-chzzk/switch_chzzk.nro ftp://192.168.1.16:5000/switch/switch_chzzk.nro
```

FTP 로그 읽기:

```bash
curl --silent --show-error ftp://192.168.1.16:5000/switch/switch_chzzk.log
```

## 새 프롬프트용 바로붙 프롬프트

```text
/home/ubuntu/lab/switch-chzzk 작업 이어서 해줘.
먼저 AGENTS.MD, docs/playback.md, docs/toolchain.md, docs/handoff.md 읽고 시작해.

현재 상태:
- 앱 크래시는 해결됨
- CHZZK API / master m3u8 / selected chunklist preflight는 Switch 실기에서 200 OK
- 그런데 stock devkitPro switch-libmpv/switch-ffmpeg가 같은 chunklist URL을 mpv loadfile에서 error=-13 loading failed로 끝냄

다음 목표:
- reference/TsVitch/scripts/switch/ffmpeg/* 와 reference/TsVitch/scripts/switch/mpv/* 를 참고해서
  우리 프로젝트용 최소 커스텀 ffmpeg/mpv 빌드 경로를 만들고
  Makefile.switch가 그 결과물을 링크하도록 바꿔줘.

작업 중 알아낸 내용은 docs에 반드시 반영하고,
빌드 성공 후 필요하면 ftp://192.168.1.16:5000/switch/switch_chzzk.nro 로 업로드까지 해줘.
```
