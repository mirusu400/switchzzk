# Architecture

## 목표 범위

- 인기 라이브 목록 조회
- 선택한 채널의 라이브 상세 조회
- HLS/LL-HLS 마스터 플레이리스트 해석
- Switch에서 mpv로 실시간 스트리밍 재생

## 레이어

### 1. 공용 코어

위치: `include/chzzk/`, `src/common/`

- CHZZK API 호출 (라이브 목록, 상세, URL 해석)
- JSON 응답 파싱 (`nlohmann/json`)
- M3U8 variant 선택
- HTTP 클라이언트 (Switch: libcurl, Host: cpp-httplib)

### 2. Borealis UI

위치: `src/borealis/`, `include/activity/`, `include/tab/`, `resources/`

- Switch 네이티브 룩앤필 (Borealis 프레임워크)
- `MainActivity` → XML 레이아웃 (`activity/main.xml`)
- `LiveTab` — RecyclerFrame 기반 채널 목록
- `LiveCell` — 채널명, 제목, 시청자수, 카테고리 표시
- 컨트롤러 입력: A(선택), B(뒤로), X(새로고침), Y(LL-HLS 토글)

### 3. mpv 플레이어

위치: `src/switch/switch_player.cpp`

- SDL2 윈도우 + OpenGL ES 컨텍스트
- libmpv render API로 프레임 렌더링
- 오디오: SDL2 audio output
- A(일시정지), B(종료)

### 4. 호스트 검증 앱

위치: `src/host/main.cpp`

- 공용 코어를 Ubuntu에서 빠르게 검증
- fixture 기반 개발 + 실제 네트워크 smoke test

## 앱 실행 흐름

```
main()
  ├── while (true) {
  │     ├── Borealis::init() → createWindow → pushActivity(MainActivity)
  │     ├── mainLoop() — UI 렌더링 + 사용자 입력
  │     │     ├── X → fetchLives() → API 호출 → RecyclerView 갱신
  │     │     ├── A → playChannel() → 상세 조회 → URL 해석 → Borealis::quit()
  │     │     └── +/HOME → 앱 종료
  │     │
  │     ├── if (g_has_pending_playback)
  │     │     └── run_switch_player() — SDL2 + mpv 재생
  │     │           ├── init_sdl() → init_mpv() → loadfile()
  │     │           ├── loop() — 프레임 렌더링 + 이벤트 처리
  │     │           └── cleanup() → SDL_Quit()
  │     │
  │     └── continue (Borealis 재시작) or break (앱 종료)
  └── }
```

## 데이터 흐름

1. 라이브 목록: `GET /service/v1/lives?size=20&sortType=POPULAR`
2. 채널 상세: `GET /service/v3.1/channels/{channelId}/live-detail`
3. `livePlaybackJson.media`에서 HLS 또는 LLHLS 선택
4. 마스터 M3U8 다운로드 → variant 파싱
5. 목표 해상도(720p)에 맞는 chunklist URL 선택
6. mpv에 chunklist URL 전달 → HLS 재생

## 빌드 시스템

- **Switch**: CMake + Borealis + Docker named volume
  - 커스텀 ffmpeg (HTTPS/HLS 프로토콜 + DNS 패치)
  - stock mpv (ABI 호환)
  - `./build.sh` — ~25초 (ffmpeg 캐시 후)
- **Host**: Makefile + g++ + cpp-httplib
  - `make host`

## 리스크

### CHZZK API 변경
비공식 사용 경로라 응답 필드가 바뀔 수 있다. 파서 변경 시 레퍼런스와 실제 네트워크 둘 다 확인.

### 홈브류 환경 제약
- 메모리 여유가 작다.
- Borealis/mpv GL 컨텍스트 동시 사용 불가 → 종료/재시작 구조.
- 로그인 WebView는 난도가 높다.
