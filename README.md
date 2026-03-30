# Pickpacker (UE5 Co-op Multiplayer) — Portfolio Repo

<p align="center">
  <a href="https://github.com/kimasill/Blaster"><img alt="GitHub Repo" src="https://img.shields.io/badge/GitHub-Blaster-181717?style=for-the-badge&logo=github&logoColor=white" /></a>
  <img alt="Unreal Engine 5" src="https://img.shields.io/badge/Unreal%20Engine-5-0E1128?style=for-the-badge&logo=unrealengine&logoColor=white" />
  <img alt="C++" src="https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" />
  <img alt="Multiplayer" src="https://img.shields.io/badge/Multiplayer-Online%20Subsystem-2EA44F?style=for-the-badge" />
</p>

> 협동 멀티플레이 물류/잠입 게임 **Pickpacker**의 핵심 구현을 정리한 저장소입니다.  
> “기능 나열”보다 **서버 권한(Authority)·Replication 기준으로 게임 루프를 일관되게 연결**한 구조를 보여주는 데 집중했습니다.

## Links

- **Portfolio (PDF/웹)**: `https://kimasill.github.io/`
- **Project Page**: `https://kimasill.github.io/projects/pickpacker.html`
- **Process / Devlog**: `https://kimasill.github.io/projects/pickpacker-process.html`

## 핵심 구현 (요약)

### 1) GameFlow / Orders (Server-authoritative)

- 주문 웨이브 → 제출 판정 → 크레딧 반영 → 종료 조건까지 서버 권한 기준으로 처리
- 패키징 빌드에서 스트리밍 레벨 참조 문제를 피하기 위해, 매치 시작 시 스트리밍 로딩을 먼저 고정

관련 코드:
- `Source/Blaster/GameMode/PickpackerGameMode.cpp`
- `Source/Blaster/GameState/PickpackerGameState.cpp`

### 2) Interaction + Inventory (RPC/Replication)

- 들고 있는 오브젝트가 시야를 가리는 상황 등 “실제 플레이”에서 발생하는 트레이스 예외를 처리
- 아이템 수집/슬롯 저장은 서버 RPC로 통일하고, 복제 콜백에서 UI를 갱신하는 형태로 일관화

관련 코드:
- `Source/Blaster/Components/InteractionComponent.cpp`
- `Source/Blaster/Components/PlayerInventoryComponent.cpp`

### 3) AI Architecture (Reusable Layer)

- 컨트롤러/Perception/유틸을 공통 레이어로 분리해 유닛 추가 시 중복 비용을 줄임
- BT/Blackboard 초기화 누락, 순찰 박스 이탈로 인한 내비 실패 같은 “확장 시 깨짐”을 구조로 차단

관련 코드:
- `Source/Blaster/AI/PPAIControllerBase.cpp`
- `Source/Blaster/AI/UPPSightPerceptionComponent.cpp`
- `Source/Blaster/AI/PPPatrolBoundsLibrary.cpp`

### 4) Escape / Ending (Data-driven)

- 월드 플래그/탈출 인원 변화 시 엔딩을 즉시 재평가
- 조건 분기(if-else) 대신 Data Asset 기반으로 확장 가능하게 구성

관련 코드:
- `Source/Blaster/Components/EscapeProgressComponent.cpp`

## Performance (Render Optimization)

프로파일링 기반 최적화로, 협동 플레이가 가능한 프레임 예산을 확보했습니다.

- Draw Calls: **~11,061 → ~3,200 (약 71% 감소)**
- FPS: **~25 → ~100**

시각 자료(웹과 동일 출처):
- `https://kimasill.github.io/images/Pickpacker/%EC%B5%9C%EC%A0%81%ED%99%94.png`
- `https://kimasill.github.io/images/Pickpacker/%EC%B5%9C%EC%A0%81%ED%99%943.png`

## Getting Started

이 레포는 UE5 프로젝트입니다.

- Unreal Editor에서 프로젝트를 열고 실행합니다.
- 온라인 세션은 Online Subsystem 설정(플러그인/플랫폼)에 따라 동작합니다.

> 실행 절차는 개발 환경(에디터/패키징/Steam)별로 달라, 추후 `docs/`로 분리해 보강할 예정입니다.
