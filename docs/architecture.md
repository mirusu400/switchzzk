# Architecture

## 목표 범위

현재 앱 범위는 다음에 맞춘다.

- 인기 라이브 목록 조회
- 선택한 채널의 라이브 상세 조회
- HLS/LL-HLS 마스터 플레이리스트 해석
- 스위치에서 재생기로 넘길 URL 결정

## 레이어

### 1. 공용 코어

위치:

- `include/chzzk`
- `src/common`

역할:

- CHZZK API 호출
- JSON 응답 파싱
- `content` envelope 해제
- `livePlaybackJson` 해석
- M3U8 variant 선택

### 2. 호스트 검증 앱

위치:

- `src/host/main.cpp`

역할:

- 공용 코어를 Ubuntu에서 빠르게 검증
- fixture 기반 개발
- 실제 네트워크 경로 smoke test

이 경로는 스위치 툴체인이 없을 때 공용 로직을 잠깐 검증하는 보조 수단이다. 제품 타깃은 아니다.

### 3. 스위치 프론트엔드

위치:

- `src/switch/main.cpp`

역할:

- `libnx` 초기화
- 패드 입력 처리
- 간단한 목록/상태 화면 출력
- 공용 코어 호출

현재는 콘솔 UI 수준이며, 실제 제품으로 가려면 SDL2 또는 Borealis 기반 UI와 재생 백엔드가 붙어야 한다.

## 데이터 흐름

1. 라이브 목록 요청: `/service/v1/lives`
2. 선택한 채널 상세 요청: `/service/v3.1/channels/{channelId}/live-detail`
3. `livePlaybackJson.media`에서 `HLS` 또는 `LLHLS` 선택
4. 마스터 M3U8 다운로드
5. 목표 해상도에 맞는 variant 선택
6. 최종 chunklist URL을 재생 백엔드에 전달

## 레퍼런스와의 관계

레퍼런스 앱은 Flutter 기반이지만, 이 저장소는 구조를 그대로 이식하지 않는다.

- 참고할 것
  - API endpoint
  - 응답 필드
  - `livePlaybackJson` 의미
  - low latency / resolution 선택 정책
- 그대로 옮기지 않을 것
  - Flutter widget tree
  - Riverpod/Freezed 구조
  - Android TV 전용 입력/UI 패턴

## 리스크

### 1. 재생 백엔드

가장 큰 공백이다. URL 해석은 되어도 실제 영상 디코딩은 아직 없다.

### 2. CHZZK API 변경

비공식 사용 경로라 응답 필드가 바뀔 수 있다. 파서 변경 시 레퍼런스와 실제 네트워크 둘 다 확인한다.

### 3. 홈브류 환경 제약

- 메모리 여유가 작다.
- 멀티뷰는 비용이 크다.
- 로그인 WebView는 난도가 높다.

## 추천 구현 순서

1. 스위치 `.nro` 빌드 환경 고정
2. 재생기 백엔드 연결
3. 이미지/폰트 포함 UI로 교체
4. 설정 저장 추가
5. 채팅 뷰 추가
