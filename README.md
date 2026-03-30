# Pickpacker — UE5 Co-op Multiplayer

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

링크 · [프로젝트 페이지](https://kimasill.github.io/projects/pickpacker.html) · [진행·구조 (pickpacker-process)](https://kimasill.github.io/projects/pickpacker-process.html) · [웹 포트폴리오](https://kimasill.github.io/)

### Overview

| 항목 | 내용 |
| --- | --- |
| 장르 | 지하·열차 배경 협동 멀티플레이(물류·주문·탈출) |
| 엔진·스택 | Unreal Engine 5 · C++ · Online Subsystem |
| 기간·규모 | 1인 · 개발 중 · PC / Steam 목표 |

### Role

- 게임 플레이 루프·상호작용·시네마틱·인벤토리·AI·멀티플레이(Online Subsystem 세션).
- 주문·레시피·배치 규칙을 데이터 중심으로 정리, 콘텐츠 확장 시 코드 수정 최소화.

---

## Core Implementation

### 1. GameFlow – 게임 진행 흐름에서 스트리밍 먼저 고정

에디터와 달리 패키지 빌드에서 스트리밍 레벨이 늦게 붙으면 매치 시작 시점에 액터 참조가 비어 크래시 등 각종 문제가 발생했다. 서버 권한(`HasAuthority`)에서 `SetShouldBeLoaded`·`SetShouldBeVisible` 후 `FlushLevelStreaming`으로 반영해, 레벨이 실제로 붙은 뒤에만 이후 로직을 시작하게 했다.

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

태그·수량·포장 조건과 서버 `GameState`가 어긋나면 협동 주문이 클라 추측에만 의존한다.

크레딧·이력이 팀원마다 다르면 협동 목표와 게임오버 판정도 함께 흔들린다.

활성 주문 소모는 서버에서만 적용하고 완료 시 `HandleOrderSuccess` 후 `SyncOrdersToGameState`로 반영했다. 크레딧·이력도 서버 델타만 `CreditHistory`·`OnCreditsChanged`로 맞춘다.

멀티플레이 구현에서 각자 상태를 처리하고 동기화 하는것보다 서버에 요청하는 구조로 작동하는게 디버깅 비용이 적었다. 팀원 간 주문·잔액 불일치도 결국 `GameState` 한곳만 보면 원인이 드러났다.

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

Parcel이 카메라 앞을 가리면 타깃이 안 잡히고, 클라이언트를 기반으로 상호작용을 진행하면 서버와 어긋나 유실·중복처럼 보인다. 이런식으로 특히 상호작용 부분에서 서버와 클라이언트 상태가 다르게 동작하는 부분이 많았다.

`CarriedParcel`과 부착 부모를 `AddIgnoredActor`로 트레이스에서 빼고, 권한이 없으면 `Server_CollectItem`으로 보내 `CollectedItems`는 서버에서만 갱신하게 했다.

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

BT·블랙보드 초기화가 제각각이면 AI를 추가할 때마다 세팅 누락으로 깨진다. 초기화 문제로 순찰 지점이 지정된 순찰 박스 밖으로 나가면 내비 실패 → 유닛 정체 → 프레임 저하로 이어진다. `RunBehaviorTreeWithBlackboard`로 블랙보드를 주입한 뒤 BT를 시작하고, 순찰은 박스 내 랜덤 샘플 후 최소·최대 반경으로 클램프해서 이 두 가지를 같이 최적화했다.

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

월드 플래그나 탈출 인원이 바뀐 뒤 엔딩을 다시 평가하지 않으면 조건을 채워도 분기가 빠진다.

탈출 조건을 만족해도 엔딩 실행이 되지 않으면 세션 전체가 마비된다.

`SetWorldFlag`·승인 플레이어 갱신 시 `EvaluateEndings`를 호출하고 `EndingData`를 `IsEndingConditionMet`로 순회했다. 엔딩을 데이터 에셋으로 빼 두면 기획·밸런스 수정 때 코드 재컴파일을 줄이기 편할것이라고 생각했다.

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

LAN·온라인에서 세션 설정·검색 쿼리가 달라 분기가 흩어지면, 친구 방이 안 보이는 식으로 멀티 자체가 동작하지 않았다.

`bIsLANMatch`·`bIsLanQuery`로 로컬 서브시스템을 구분하고 `SEARCH_LOBBIES` / `SEARCH_PRESENCE`를 전처리 분기로 맞춰 플랫폼별 예외를 세션 모듈 한곳에 모았다.

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

- **프로파일링·측정** — GPU 약 **9 ms** 수준. 드로우 콜 과다로 CPU 병목 시 FPS **약 25**까지 하락(개발 빌드·프로파일러).
- **HISM·씬 캡처** — 정적 메시 HISM 병합으로 평균 FPS **약 43**까지 상승. 병목은 Scene Capture 과다 호출로 판정.  
  매 프레임 캡처 → 이동 시만 캡처로 전환, 드로우 콜 **약 10,518 → 4,600**, FPS **약 94**.  
  루멘 설정과 라이팅 최적화 조정 후 드로우 콜 **약 3,200**(초기 **약 11,061** 대비 **약 71%** 감소), Prims **약 400K**, FPS **약 100** 부근.  
- **상호작용** — Parcel이 시야를 가리는 등 예외를 트레이스·채널 설계에 반영.

| 최적화 단계 (요약) | FPS (대략) | Draw Calls | Prims |
| --- | --- | --- | --- |
| 초기 (CPU 병목) | **~25** | **~11,061** | **~1,581K** |
| Step 3 (씬 캡처 틱 조정 후) | **~100** | **~3,200** | **~400K** |

---

## Result

- 서버 권한·복제 기준으로 루프 전체가 연결된 협동 게임임을 보여 줌.

---

## Getting Started

이 레포는 UE5 프로젝트입니다.

1. Unreal Editor에서 프로젝트를 열고 실행합니다.
2. 온라인 세션은 Online Subsystem 설정(플러그인/플랫폼)에 따라 동작합니다.

> 실행 절차는 개발 환경(에디터/패키징/Steam)별로 달라, 추후 `docs/`로 분리해 보강할 예정입니다.
