# Switchzzk

닌텐도 스위치 CFW 홈브류용 치지직(CHZZK) 라이브 스트리밍 앱입니다.

## 기능

- **인기 라이브** — 실시간 인기 라이브 채널 목록
- **카테고리** — 게임/토크 등 카테고리별 라이브 탐색
- **검색** — 채널명 검색 (Switch 키보드 지원)
- **HLS 재생** — 720p 60fps 영상 + 오디오 실시간 재생
- **LL-HLS** — 저지연 모드 토글 지원
- **Borealis UI** — Switch 네이티브 룩앤필

## 스크린샷

_(추후 추가)_

## 빌드

Docker + devkitPro 이미지 기반입니다.

```bash
# 전체 빌드 (커스텀 ffmpeg 캐싱, ~25초)
./build.sh

# 캐시 초기화 후 빌드
./build.sh --clean
```

## 설치

1. Switch에 CFW (Atmosphere 등) 설치
2. `cmake-build-switch/switch_chzzk.nro`를 SD 카드 `/switch/` 폴더에 복사
3. 홈브류 메뉴에서 실행

## 조작

| 버튼 | 동작 |
|------|------|
| A | 채널 선택 / 재생 시작 |
| B | 뒤로 / 재생 종료 |
| X | 새로고침 / 검색 입력 |
| Y | LL-HLS 토글 |

## 기술 스택

- **UI**: [Borealis](https://github.com/xfangfang/borealis) (Switch 네이티브 UI 프레임워크)
- **재생**: libmpv + SDL2 + OpenGL ES
- **네트워크**: 커스텀 ffmpeg (HTTPS/HLS 프로토콜 + DNS 패치) + libcurl
- **빌드**: CMake + Docker (devkitpro/devkita64)

## 문서

- [아키텍처](docs/architecture.md)
- [빌드/툴체인](docs/toolchain.md)
- [재생 백엔드](docs/playback.md)
- [로드맵](docs/roadmap.md)
- [핸드오프](docs/handoff.md)

## 레퍼런스

- [unofficial_chzzk_android_tv](https://github.com/) — API/응답 구조 참고
- [TsVitch](https://github.com/) — Switch ffmpeg/mpv 패치 참고

## 라이선스

GPL v3.0