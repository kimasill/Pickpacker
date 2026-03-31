# Pickpacker — UE5 Co-op Multiplayer

> 리네임 노트(신뢰성/재현성 목적): 초기 프로토타이핑은 UE 템플릿 기반 코드베이스에서 시작해 작업 디렉터리/모듈명이 `Blaster`로 남아 있었습니다.  
> 2026-03 기준으로 `.uproject/모듈명/Source 폴더/Target/Config(/Script)`를 **Pickpacker로 일괄 정리**했고, 플러그인 모듈 충돌도 함께 해소했습니다.  
> 이 레포의 구현 포인트는 README의 코드 링크 기준으로 유지되며, 필요 시 클래스 리다이렉트로 에셋 호환을 보장하는 방향으로 점진 정리합니다.

<p align="center">
  <a href="https://github.com/kimasill/Pickpacker"><img alt="GitHub Repo" src="https://img.shields.io/badge/GitHub-Pickpacker-181717?style=for-the-badge&logo=github&logoColor=white" /></a>
  <img alt="Unreal Engine 5" src="https://img.shields.io/badge/Unreal%20Engine-5-0E1128?style=for-the-badge&logo=unrealengine&logoColor=white" />
  <img alt="C++" src="https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" />
  <img alt="Multiplayer" src="https://img.shields.io/badge/Multiplayer-Online%20Subsystem-2EA44F?style=for-the-badge" />
</p>

<p align="center">
  <a href="https://kimasill.github.io/projects/pickpacker.html" title="Pickpacker 프로젝트 페이지" target="_blank" rel="noopener noreferrer">
    <img src="https://kimasill.github.io/images/Pickpacker/PickPackerCover.png" alt="Pickpacker 타이틀" width="640" />
  </a>
</p>

링크 · [Trailer (YouTube)](https://www.youtube.com/watch?v=jSH2DfqKqWA) · [프로젝트 페이지](https://kimasill.github.io/projects/pickpacker.html) · [진행·구조 (pickpacker-process)](https://kimasill.github.io/projects/pickpacker-process.html) · [웹 포트폴리오](https://kimasill.github.io/)

> UE5 기반 협동 멀티플레이 **Pickpacker** 소스 레포지토리입니다. 서버 권한·복제를 축으로 한 핵심 구현과 코드 위치를 아래에 개조식으로 정리합니다.

### Overview

| 항목 | 내용 |
| --- | --- |
| 장르 | 지하·열차 배경 협동 멀티플레이(물류·주문·탈출) |
| 엔진·스택 | Unreal Engine 5 · C++ · Online Subsystem |
| 기간·규모 | 1인 · 개발 중 · PC / Steam 목표 |

<p align="center">
<img src="https://kimasill.github.io/images/Pickpacker/%EB%A7%88%EB%8D%94_2.png" alt="Pickpacker 마더" width="320" />
<img src="https://kimasill.github.io/images/Pickpacker/%EC%84%A4%EB%B9%84%EC%8B%A4.png" alt="Pickpacker 설비실" width="320" />
<img src="https://kimasill.github.io/images/Pickpacker/%EC%83%81%ED%98%B8%EC%9E%91%EC%9A%A9.png" alt="Pickpacker 상호작용" width="320" />
<img src="https://kimasill.github.io/images/Pickpacker/%EC%A0%9C%EC%B6%9C%EB%B2%A8%ED%8A%B8.png" alt="Pickpacker 제출 벨트" width="320" /></p>


### Role

- 게임 플레이 루프·상호작용·시네마틱·인벤토리·AI·멀티플레이(Online Subsystem 세션) 담당
- 주문·레시피·배치 규칙을 데이터 중심으로 정리, 콘텐츠 확장 시 코드 수정 최소화

---

## Core Implementation

### 1. GameFlow – 게임 진행 흐름에서 스트리밍 먼저 고정

- **문제**: 패키지 빌드에서 스트리밍 레벨 지연 시 매치 시작 시점 액터 참조 공백·크래시 등 발생
- **대응**: `HasAuthority`에서 `SetShouldBeLoaded` / `SetShouldBeVisible` 후 `FlushLevelStreaming`으로 로드 완료 보장 뒤 후속 로직 진행

> 📄 [`Source/Blaster/GameMode/PickpackerGameMode.cpp`](https://github.com/kimasill/Pickpacker/blob/PickPacker-publish/Source/Blaster/GameMode/PickpackerGameMode.cpp#L61-L72) — 스트리밍 레벨 선행 로드

```cpp
if (LevelShortName.Equals(ToLoad.ToString(), ESearchCase::IgnoreCase) ||
    LevelName.Contains(ToLoad.ToString(), ESearchCase::IgnoreCase))
{
    StreamingLevel->SetShouldBeLoaded(true);
    StreamingLevel->SetShouldBeVisible(true);
    break;
}
UGameplayStatics::FlushLevelStreaming(World);
```

---

### 2. GameMode & GameState – 주문·크레딧 서버 권한

- **문제**: 주문·크레딧이 서버 `GameState`와 어긋나면 협동 목표·게임오버가 제대로 동작하지 않음
- **대응**: 주문 소모·완료는 서버 전용, `HandleOrderSuccess` → `SyncOrdersToGameState`, 크레딧·이력은 서버 델타·`CreditHistory` 등으로 일원화, 디버깅은 `GameState` 중심으로 수렴

> 📄 [`Source/Blaster/GameMode/PickpackerGameMode.cpp`](https://github.com/kimasill/Pickpacker/blob/PickPacker-publish/Source/Blaster/GameMode/PickpackerGameMode.cpp#L988-L998) — 주문 수량 판정 + 완료 처리

```cpp
if (Order.SubmittedQuantity >= Order.RequiredQuantity)
{
    Order.bCompleted = true;
    Order.ResolutionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    HandleOrderSuccess(Order);
}
// ...
SyncOrdersToGameState();
```

> 📄 [`Source/Blaster/GameMode/PickpackerGameMode.cpp`](https://github.com/kimasill/Pickpacker/blob/PickPacker-publish/Source/Blaster/GameMode/PickpackerGameMode.cpp#L1028-L1044) — `HandleOrderSuccess` + `SyncOrdersToGameState`

```cpp
void APickpackerGameMode::HandleOrderSuccess(FActiveOrderState& Order)
{
    if (Order.CreditReward != 0)
    {
        ApplyCreditDelta(Order.CreditReward,
            FString::Printf(TEXT("%s completed"), *Order.OrderName.ToString()));
    }
}

void APickpackerGameMode::SyncOrdersToGameState()
{
    if (PickpackerGameState)
    {
        PickpackerGameState->SetActiveOrders(ActiveOrders);
    }
}
```

---

### 3. Interaction & Inventory – 트레이스 보정·협동 인벤

<img src="https://kimasill.github.io/images/Pickpacker/상호작용.png" alt="Pickpacker 상호작용" width="640" />

- **문제**: Parcel을 들고 있으면 트레이스가 막히고, 클라이언트에서 먼저 상호작용하면 서버와 어긋남
- **대응**: `CarriedParcel`·부착 부모를 `AddIgnoredActor`로 트레이스 제외, 비권한 시 `Server_CollectItem`, `CollectedItems`는 서버에서만 갱신

> 📄 [`Source/Blaster/Components/InteractionComponent.cpp`](https://github.com/kimasill/Pickpacker/blob/PickPacker-publish/Source/Blaster/Components/InteractionComponent.cpp#L265-L278) — 트레이스 보정

```cpp
FCollisionQueryParams InteractionParams(SCENE_QUERY_STAT(InteractionTargetTrace), false);
InteractionParams.AddIgnoredActor(OwnerCharacter);

if (IsValid(CarriedParcel))
{
    InteractionParams.AddIgnoredActor(CarriedParcel);
    if (AActor* Carrier = CarriedParcel->GetAttachParentActor())
    {
        InteractionParams.AddIgnoredActor(Carrier);
    }
}
```

> 📄 [`Source/Blaster/Components/PlayerInventoryComponent.cpp`](https://github.com/kimasill/Pickpacker/blob/PickPacker-publish/Source/Blaster/Components/PlayerInventoryComponent.cpp#L69-L97) — 인벤 수집·서버 권한

```cpp
if (!GetOwner() || !GetOwner()->HasAuthority())
{
    Server_CollectItem(Item);
    return true;
}

// Server-side collection
CollectedItems.Add(Item);
Item->SetActorHiddenInGame(true);
```

---

### 4. AI – BT·블랙보드 일원화·순찰 클램프

- **문제**: BT/BB 초기화 분산 시 유닛 추가 시 세팅 누락, 순찰 박스 이탈 시 내비 실패·정체·프레임 저하
- **대응**: `RunBehaviorTreeWithBlackboard`로 BB 주입 후 BT 시작, 순찰은 박스 내 샘플·반경 클램프로 안정화

> 📄 [`Source/Blaster/AI/PPAIControllerBase.cpp`](https://github.com/kimasill/Pickpacker/blob/PickPacker-publish/Source/Blaster/AI/PPAIControllerBase.cpp#L21-L49) — `RunBehaviorTreeWithBlackboard`

```cpp
bool APPAIControllerBase::RunBehaviorTreeWithBlackboard(UBehaviorTree* BTAsset, UBlackboardData* BBOverride)
{
    if (!BTAsset) return false;

    UBlackboardData* BBToUse = BBOverride ? BBOverride : BTAsset->BlackboardAsset.Get();
    if (!BBToUse) return false;

    UBlackboardComponent* BlackboardComp = nullptr;
    if (!UseBlackboard(BBToUse, BlackboardComp)) return false;

    const bool bStarted = RunBehaviorTree(BTAsset);
    bPPBehaviorTreeRunning = bStarted;
    return bStarted;
}
```

<img src="https://kimasill.github.io/images/Pickpacker/Drone.png" alt="Pickpacker 드론" width="640" />

*Drone 정찰 / Perception 처리 테스트*

> 📄 AI 관련 전체 구조: [`Source/Blaster/AI/`](https://github.com/kimasill/Pickpacker/tree/PickPacker-publish/Source/Blaster/AI) — PPAIControllerBase, DroneActor, BruteActor, PPPatrolBoundsLibrary 등

---

### 5. Escape & Ending – 플래그 변동 시 엔딩 재평가

- **문제**: 월드 플래그·탈출 인원이 바뀐 뒤 엔딩을 다시 검사하지 않으면 조건을 채워도 엔딩이 안 됨
- **대응**: `SetWorldFlag`·승인 플레이어 갱신 시 `EvaluateEndings`, `UDA_EndingData`로 조건 분기·기획 변경 비용 감소

> 📄 [`Source/Blaster/Components/EscapeProgressComponent.cpp`](https://github.com/kimasill/Pickpacker/blob/PickPacker-publish/Source/Blaster/Components/EscapeProgressComponent.cpp#L79-L99) — `SetWorldFlag` → `EvaluateEndings`

```cpp
void UEscapeProgressComponent::SetWorldFlag(const FGameplayTag& Flag, int32 Value)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    if (FWorldFlagEntry* Entry = FindWorldFlagEntry(Flag))
    {
        Entry->Value = Value;
    }
    else
    {
        FWorldFlagEntry NewEntry;
        NewEntry.Flag = Flag;
        NewEntry.Value = Value;
        WorldFlags.Add(MoveTemp(NewEntry));
    }

    EvaluateEndings();
}
```

> 📄 [`Source/Blaster/Components/EscapeProgressComponent.cpp`](https://github.com/kimasill/Pickpacker/blob/PickPacker-publish/Source/Blaster/Components/EscapeProgressComponent.cpp#L145-L170) — `EvaluateEndings`

```cpp
void UEscapeProgressComponent::EvaluateEndings()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (!CurrentEndingId.IsNone()) return;  // 이미 엔딩이 시작된 경우

    for (const UDA_EndingData* EndingData : EndingDataAssets)
    {
        if (!EndingData) continue;

        if (IsEndingConditionMet(EndingData))
        {
            StartEnding(EndingData);
            return;
        }
    }
}
```

---

### 6. Multiplayer Sessions – LAN·온라인 세션 분기 한곳에

- **문제**: LAN/온라인 세션 설정·검색 쿼리 분기가 흩어져 있으면 방 검색이 안 되는 등 멀티가 동작하지 않음
- **대응**: `bIsLANMatch`·`bIsLanQuery`로 NULL 서브시스템 구분, `SEARCH_LOBBIES` / `SEARCH_PRESENCE` 전처리 분기를 세션 서브시스템에 집중

> 📄 [`Plugins/MultiplayerSessions/.../MultiplayerSessionsSubsystem.cpp`](https://github.com/kimasill/Pickpacker/blob/PickPacker-publish/Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/MultiplayerSessionsSubsystem.cpp#L77) — 세션 생성 LAN 분기

```cpp
LastSessionSettings->bIsLANMatch = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
```

> 📄 [`Plugins/MultiplayerSessions/.../MultiplayerSessionsSubsystem.cpp`](https://github.com/kimasill/Pickpacker/blob/PickPacker-publish/Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/MultiplayerSessionsSubsystem.cpp#L160-L175) — 세션 검색 쿼리 분기

```cpp
LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;

#if defined(SEARCH_LOBBIES)
    LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
#else
    LastSessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
#endif
```

---

## Problem Solving

### Rendering & Optimization

웹 [pickpacker-process](https://kimasill.github.io/projects/pickpacker-process.html)과 동일 출처.

<img src="https://kimasill.github.io/images/Pickpacker/%EC%B5%9C%EC%A0%81%ED%99%94.png" alt="Pickpacker 초기 프로파일링" width="640" />

*초기 프로파일링 — 드로우 콜·CPU 병목 확인*

<img src="https://kimasill.github.io/images/Pickpacker/%EC%B5%9C%EC%A0%81%ED%99%943.png" alt="Pickpacker 씬 캡처·드로우 콜 최적화 후" width="640" />

*씬 캡처 틱·빈도 조정 후*

- **프로파일링·측정**: GPU 약 **9 ms** 수준, 드로우 콜 과다 시 CPU 병목·FPS **약 25**까지 하락(개발 빌드·프로파일러)
- **HISM·씬 캡처**: HISM 병합으로 FPS **약 43**까지 상승 → Scene Capture 과다가 병목으로 판정 → 이동 시에만 캡처로 전환, 드로우 콜 **약 10,518 → 4,600**, FPS **약 94**
- **추가 튜닝**: 루멘·라이팅 조정 후 드로우 콜 **약 3,200**(초기 **약 11,061** 대비 **약 71%** 감소), Prims **약 400K**, FPS **약 100** 부근
- **측정 조건(템플릿)**: 빌드(Development/Shipping) · 해상도 · 맵/상황 · 측정 툴(`stat unit`, `stat scenerendering`, Unreal Insights) · HW(CPU/GPU/RAM) · 반복 측정 여부
- **상호작용**: Parcel 가림 등은 트레이스·채널 설계에 반영 (위 Interaction 섹션과 연계)

| 최적화 단계 (요약) | FPS (대략) | Draw Calls | Prims |
| --- | --- | --- | --- |
| 초기 (CPU 병목) | **~25** | **~11,061** | **~1,581K** |
| Step 3 (씬 캡처 틱 조정 후) | **~100** | **~3,200** | **~400K** |

---

## Result

- 서버 권한·복제 기준으로 루프가 연결된 협동 멀티플레이 구조를 코드로 확인 가능

---

## Getting Started

UE5 프로젝트입니다.

1. Unreal Editor에서 프로젝트를 연 뒤 실행합니다.
2. 온라인 세션은 Online Subsystem·플랫폼 설정에 따라 동작이 달라질 수 있습니다.

> 상세 실행 절차(패키징·Steam 등)는 환경별로 다르므로, 필요 시 `docs/`로 보강 예정입니다.
