# switch-chzzk

닌텐도 스위치 CFW 홈브류용 치지직 앱 MVP입니다. 기준 레퍼런스는 [`reference/unofficial_chzzk_android_tv`](./reference/unofficial_chzzk_android_tv)이며, 전체 기능 이식이 아니라 스위치에서 현실적인 범위인 `비로그인 라이브 목록 + 상세 조회 + 재생 URL 해석`에 초점을 맞췄습니다.

## 판단

가능합니다. 다만 범위를 잘라야 합니다.

- 가능한 범위
  - 인기 라이브 목록 조회
  - 채널별 라이브 상세 조회
  - HLS/LL-HLS 마스터 플레이리스트 파싱
  - 단일 스트림 재생으로 이어질 URL 선택
- 어려운 범위
  - Flutter 앱 수준의 전체 UI/상태관리 이식
  - WebView 기반 로그인
  - 멀티뷰 동시 재생
  - 채팅/오버레이를 포함한 무거운 조합

## 하드웨어 관점

- Switch는 Tegra X1 + 4GB 공유 메모리 환경이라 단일 스트림 위주의 HLS 시청 앱은 충분히 현실적입니다.
- 반대로 멀티뷰, 고해상도 소프트웨어 디코딩, 복잡한 웹뷰 로그인 흐름은 홈브류 환경에서 비용이 큽니다.
- 따라서 이 저장소의 MVP는 `single-view` 기준으로 설계했습니다. 720p 우선 선택이 기본이고, 1080p는 플레이어 백엔드 상태에 따라 선택적으로 여는 것이 맞습니다.

## 현재 구현 상태

- 공용 CHZZK API 클라이언트
- 라이브 목록/상세 모델 파서
- 마스터 M3U8 파서 및 해상도 선택기
- 호스트 검증용 CLI 앱
- 스위치용 `libnx` 콘솔 엔트리포인트

관련 문서:

- `AGENTS.MD`
- `docs/architecture.md`
- `docs/handoff.md`
- `docs/playback.md`
- `docs/toolchain.md`
- `docs/roadmap.md`

현재 스위치 쪽에는 `libmpv + SDL2 + OpenGL ES` 재생 백엔드가 연결돼 있습니다. 다만 실기 검증 결과, 앱 로직과 URL 해석은 정상인데 `stock devkitPro switch-libmpv/switch-ffmpeg` 조합이 실제 CHZZK HLS chunklist URL을 `mpv loadfile`에서 열지 못하고 있습니다. 따라서 다음 단계는 커스텀 `ffmpeg/mpv` 패치 빌드 경로를 도입하는 것입니다.

## 호스트 빌드

```bash
make host
./build/host/chzzk_host --fixture
./build/host/chzzk_host --network
```

- `--fixture`: 저장소에 포함된 fixture JSON/M3U8로 동작
- `--network`: 실제 CHZZK 엔드포인트 시도 후 실패 시 fixture 사용

## 스위치 빌드

`devkitPro/devkitA64/libnx`가 준비된 환경에서:

```bash
./scripts/fetch_vendor_headers.sh
make -f Makefile.switch
```

추가로 HTTPS 네트워크를 위해 스위치용 OpenSSL 포트 라이브러리가 필요합니다.

## 다음 단계

1. `TsVitch` 기준 Switch용 커스텀 `ffmpeg/mpv` 빌드 경로 도입
2. 실기에서 실제 영상 재생 성공 확인
3. 썸네일/텍스트 기반 UI를 Borealis 또는 SDL2 UI로 교체
4. 즐겨찾기/최근 본 채널 등 로컬 상태 저장
