# Roadmap

## 완료된 것

- [x] 공용 CHZZK API 클라이언트 (라이브 목록, 상세, URL 해석)
- [x] HLS/LL-HLS 선택 + 720p variant 선택
- [x] 호스트 검증 앱
- [x] 커스텀 ffmpeg 빌드 (HTTPS/HLS 프로토콜 + DNS 패치)
- [x] mpv + SDL2 + OpenGL ES 재생 백엔드 (실기 720p 60fps 확인)
- [x] Borealis UI 프레임워크 통합
- [x] RecyclerView 기반 채널 목록
- [x] Borealis ↔ mpv 플레이어 전환 구조
- [x] Docker named volume 빌드 캐싱 (~25초)

## Phase 1: 완성도 개선 (현재)

### Quick wins
- [ ] 앱 시작 시 자동으로 라이브 목록 로드 (현재 X 수동)
- [ ] 시청자수 포맷팅 (1.2만 등)
- [ ] 채널 목록 셀 디자인 개선
- [ ] 로딩 중 스피너 표시
- [ ] 재생 실패 시 에러 다이얼로그

### Medium effort
- [ ] 카테고리별 라이브 목록 (탭 또는 필터)
- [ ] 채널 검색
- [ ] 재생 중 OSD (해상도 선택, 볼륨)
- [ ] 썸네일 이미지 로딩

## Phase 2: 앱다운 기능

- [ ] 최근 시청 채널 저장 (`sdmc:/` JSON)
- [ ] 즐겨찾기
- [ ] 설정 화면 (LL-HLS 기본값, 화질 기본값)
- [ ] 페이지네이션 (20개 이상 로드)

## Phase 3: 고급 기능

- [ ] 채팅 읽기 전용 (웹소켓)
- [ ] VOD 목록 / 재생
- [ ] 다시보기 (타임시프트)

## Phase 4: 후순위

- [ ] 로그인 (팔로우 채널 조회)
- [ ] 멀티뷰
- [ ] 클립 재생
