# AGENTS.MD

## 목적

이 저장소는 닌텐도 스위치 CFW 홈브류로 구동되는 치지직(CHZZK) 라이브 스트리밍 앱을 만드는 프로젝트다. 기준 레퍼런스는 `reference/unofficial_chzzk_android_tv`이며, 현재 목표는 전체 기능 이식이 아니라 `실제 스위치 홈브류에서 안정적으로 굴러가는 MVP`다.

## 현재 상태

- **UI**: Borealis 프레임워크 기반 Switch 네이티브 UI (사이드바 + 탭 + RecyclerView)
- **재생**: 커스텀 ffmpeg(HTTPS/HLS) + stock mpv + SDL2/OpenGL ES로 실기 720p 60fps 재생 성공
- **API**: 라이브 목록, 라이브 상세, HLS/LL-HLS URL 해석
- **빌드**: CMake + Borealis, Docker named volume으로 ffmpeg 캐싱 (~25초 앱 빌드)
- **흐름**: Borealis UI → 채널 선택 → Borealis 종료 → mpv 재생 → B로 나가기 → Borealis 재시작

## 핵심 원칙

1. 최우선 타깃은 항상 실제 닌텐도 스위치 홈브류다.
2. 호스트 경로는 개발 보조용이지 최종 제품이 아니다.
3. 로그인 WebView, 멀티뷰, 복잡한 채팅 UI는 우선순위가 낮다.
4. 레퍼런스 앱의 API 필드와 응답 형태를 임의로 바꾸지 않는다.
5. CHZZK API 동작을 추정하지 말고 가능하면 실제 네트워크로 확인한다.
6. 스위치 전용 변경을 해도 공용 파서와 호스트 검증 경로는 같이 유지한다.

## 작업 전 확인

- 프로젝트 개요: `README.md`
- 구조 설명: `docs/architecture.md`
- 빌드/툴체인: `docs/toolchain.md`
- 현재 handoff: `docs/handoff.md`
- 우선순위: `docs/roadmap.md`
- 재생 백엔드: `docs/playback.md`

## 저장소 구조

```
src/common/          # 플랫폼 공용 로직 (API, M3U8 파서, HTTP)
src/borealis/        # Borealis UI (main, activity, tab)
src/switch/          # Switch mpv 플레이어
src/host/            # 호스트 검증용 CLI
include/chzzk/       # 공용 헤더
include/activity/    # Borealis Activity 헤더
include/tab/         # Borealis Tab 헤더
library/borealis/    # Borealis 서브모듈
resources/           # romfs 번들 (XML 레이아웃, i18n, 아이콘)
scripts/switch/      # 커스텀 ffmpeg 빌드 스크립트 + 패치
fixtures/            # 오프라인 개발용 샘플 응답
```

## 빌드 명령

```bash
# Switch 전체 빌드 (커스텀 ffmpeg 캐싱됨, ~25초)
./build.sh

# 캐시 초기화 후 빌드
./build.sh --clean

# 호스트 검증
make host
./build/host/chzzk_host --network

# FTP 업로드
curl -T cmake-build-switch/switch_chzzk.nro ftp://192.168.1.16:5000/switch/switch_chzzk.nro
```

## 에이전트가 먼저 해야 할 일

1. 스위치 툴체인과 포트 라이브러리 유무를 먼저 확인
2. 수정 범위가 공용인지, 스위치 전용인지 구분
3. 스위치 변경이면 필요한 포트 라이브러리와 빌드 절차를 문서까지 같이 갱신
4. 호스트 경로는 공용 로직을 빨리 검증할 때만 사용

## 빌드 주의사항

- **커스텀 ffmpeg만 사용, 커스텀 mpv는 크래시**: `--skip-mpv` 필수
- **mpv log-file 옵션 금지**: `sdmc:/` 경로를 넣으면 크래시
- **Borealis와 SDL 윈도우 충돌**: 재생 시 Borealis 종료 → mpv → Borealis 재시작 구조

## 우선순위 높은 다음 작업

1. 카테고리/검색 기능
2. 재생 중 OSD (볼륨, 해상도 선택)
3. 설정/최근 시청/즐겨찾기 저장
4. 썸네일 로딩
5. 채팅 (읽기 전용)

## 당장 하지 말 것

- WebView 로그인 구현
- 멀티뷰 동시 재생
- 레퍼런스 앱의 상태관리 구조를 그대로 옮기기
- 검증 없이 외부 서드파티 스위치 SDK/포크 도입

## 완료 기준

- `.nro` 빌드 + 실기 동작까지 확인
- 공용 변경 후 `make host`는 보조 검증으로 유지
- 스위치 타깃 변경이면 `docs/toolchain.md`에 반영
