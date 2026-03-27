# AGENTS.MD

## 목적

이 저장소는 닌텐도 스위치 CFW 홈브류로 구동되는 치지직 앱을 만드는 프로젝트다. 기준 레퍼런스는 `reference/unofficial_chzzk_android_tv`이며, 현재 목표는 전체 기능 이식이 아니라 `실제 스위치 홈브류에서 안정적으로 굴러가는 single-view MVP`다.

## 현재 상태

- 공용 CHZZK API 클라이언트가 있다.
- 라이브 목록, 라이브 상세, HLS/LL-HLS URL 해석이 된다.
- 호스트 검증용 CLI가 있다.
- 스위치용 `libnx` 콘솔 엔트리포인트가 있다.
- Switch용 `libmpv + SDL2 + OpenGL ES` 재생 백엔드가 연결돼 있다.
- 앱 크래시와 UI 복귀 문제는 해결됐다.
- `stock devkitPro switch-libmpv/switch-ffmpeg`의 HLS 로드 실패를 해결하기 위해 TsVitch 기반 커스텀 ffmpeg/mpv 빌드를 도입했다.
- 커스텀 빌드 + 앱 빌드는 `scripts/switch/build_custom_portlibs.sh`로 오케스트레이션된다.

## 핵심 원칙

1. 최우선 타깃은 항상 실제 닌텐도 스위치 홈브류다.
2. 스위치 MVP는 당분간 `single-view`에 집중한다.
3. 호스트 경로는 개발 보조용이지 최종 제품이 아니다.
4. 로그인 WebView, 멀티뷰, 복잡한 채팅 UI는 우선순위가 낮다.
5. 레퍼런스 앱의 API 필드와 응답 형태를 임의로 바꾸지 않는다.
6. CHZZK API 동작을 추정하지 말고 가능하면 실제 네트워크로 확인한다.
7. 스위치 전용 변경을 해도 공용 파서와 호스트 검증 경로는 같이 유지한다.

## 작업 전 확인

- 프로젝트 개요: `README.md`
- 구조 설명: `docs/architecture.md`
- 빌드/툴체인: `docs/toolchain.md`
- 현재 handoff: `docs/handoff.md`
- 우선순위: `docs/roadmap.md`

## 저장소 구조

- `src/common`: 플랫폼 공용 로직
- `src/host`: Ubuntu 등 호스트에서 돌리는 검증용 앱
- `src/switch`: `libnx` 기반 스위치 엔트리포인트
- `include/chzzk`: 공용 헤더
- `fixtures`: 오프라인 개발용 샘플 응답
- `reference/unofficial_chzzk_android_tv`: 기능/응답 구조의 기준 레퍼런스

## 자주 쓰는 명령

```bash
make host
./build/host/chzzk_host --fixture
./build/host/chzzk_host --network
./scripts/fetch_vendor_headers.sh
# 커스텀 ffmpeg/mpv + 앱 전체 빌드 (Docker):
sg docker -c 'docker run --rm -v "$PWD:/work" -w /work devkitpro/devkita64 bash -lc "./scripts/switch/build_custom_portlibs.sh"'
```

## 에이전트가 먼저 해야 할 일

1. 스위치 툴체인과 포트 라이브러리 유무를 먼저 확인
2. 수정 범위가 공용인지, 스위치 전용인지 구분
3. 스위치 변경이면 필요한 포트 라이브러리와 빌드 절차를 문서까지 같이 갱신
4. 호스트 경로는 공용 로직을 빨리 검증할 때만 사용

## 우선순위 높은 다음 작업

1. 커스텀 빌드 후 실기 재생 성공 확인
3. 콘솔 UI를 SDL2 또는 Borealis 기반 UI로 교체
4. 설정/최근 시청/즐겨찾기 저장

## 당장 하지 말 것

- WebView 로그인 구현
- 멀티뷰 동시 재생
- 레퍼런스 Flutter 앱의 상태관리 구조를 그대로 옮기기
- 검증 없이 외부 서드파티 스위치 SDK/포크 도입

## 완료 기준

- 스위치 관련 작업이면 `.nro` 빌드 가능 상태까지 가는 것을 우선한다.
- 공용 변경 후 `make host`는 보조 검증으로 유지한다.
- 가능하면 실제 치지직 응답으로 검증한다.
- 스위치 타깃 변경이면 필요한 패키지와 빌드 절차를 `docs/toolchain.md`에 반영한다.
