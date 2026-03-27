# Handoff

## 현재 상태 (2026-03-28)

**동작하는 것:**
- Borealis UI로 CHZZK 인기 라이브 20개 목록 표시 (Switch 네이티브 룩앤필)
- 채널 선택 → HLS URL 해석 → mpv로 720p 60fps + 오디오 재생
- B 버튼으로 재생 종료 후 Borealis UI 복귀
- X: 목록 새로고침, Y: LL-HLS/HLS 토글
- 빌드 캐싱으로 ~25초 앱 빌드

**아키텍처:**
```
Borealis UI (GLFW + nanovg)
    ↓ A 버튼 (채널 선택)
    ↓ Borealis::quit()
SDL2 + mpv 플레이어 (OpenGL ES)
    ↓ B 버튼 (재생 종료)
    ↓ SDL_Quit()
Borealis UI 재시작
```

Borealis와 SDL 윈도우가 GL 컨텍스트를 공유할 수 없어서, 재생 시 Borealis를 종료하고 mpv를 실행한 뒤 다시 Borealis를 시작하는 구조.

## 빌드 조합

**커스텀 ffmpeg + stock mpv** (커스텀 mpv는 크래시):
```bash
./build.sh          # named volume 캐싱, ~25초
./build.sh --clean  # 캐시 초기화
```

내부 동작:
1. Docker named volume `switch-chzzk-portlibs`에 커스텀 ffmpeg 캐싱
2. 마커 파일로 재빌드 스킵 판단
3. CMake로 Borealis + 앱 빌드 → .nro 생성

## 핵심 기술 판단 기록

### stock ffmpeg → HTTPS 프로토콜 없음
실기 mpv 로그: `No protocol handler found to open URL https://...`

### 커스텀 mpv → mpv_create 크래시
stock mpv (libmpv 2.3.0)와 Docker 이미지 portlibs ABI 불일치.
해결: stock mpv + 커스텀 ffmpeg만 사용.

### mpv log-file 옵션 → 크래시
mpv 내부 I/O가 `sdmc:/` 경로를 인식 못함. 이벤트 드레인으로만 로그 수집.

### Borealis + SDL 윈도우 충돌
Borealis가 GLFW로 윈도우를 관리하므로 SDL 윈도우를 동시에 열 수 없음.
해결: Borealis 종료 → SDL+mpv → Borealis 재시작 루프.

## 빌드 명령

```bash
# 전체 빌드
./build.sh

# FTP 업로드
curl -T cmake-build-switch/switch_chzzk.nro ftp://192.168.1.16:5000/switch/switch_chzzk.nro

# FTP 로그
curl ftp://192.168.1.16:5000/switch/switch_chzzk_borealis.log
```

## 다음 단계

1. 레퍼런스 앱 분석 후 기능 우선순위 확정
2. 카테고리별 라이브 / 검색
3. 재생 중 OSD (해상도, 볼륨)
4. 최근 시청 / 즐겨찾기
5. 썸네일 로딩
