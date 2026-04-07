# `development` 브랜치 구현 정리 문서

## 1. 브랜치로 옮겨진 작업 범위
이번에 `development` 브랜치에서 관리하도록 옮겨진 내용은 **커밋 단위가 아니라 현재 작업 트리 전체**입니다. 즉, 브랜치 생성 시점에 열려 있던 수정 파일과 신규 파일들이 그대로 `development` 브랜치에서 이어지도록 이동되었습니다.

### 포함된 대표 파일
- 수정
  - `Source/Pickpacker/GameMode/PickpackerGameMode.cpp`
  - `Source/Pickpacker/GameState/PickpackerGameState.cpp`
  - `Source/Pickpacker/GameState/PickpackerGameState.h`
  - `Source/Pickpacker/Pickpacker.Build.cs`
- 신규
  - `Source/Pickpacker/PickpackerTypes/CoreLoopTypes.h`
  - `Source/Pickpacker/Subsystem/CoreLoopSubsystem.h`
  - `Source/Pickpacker/Subsystem/CoreLoopSubsystem.cpp`
  - `Source/Pickpacker/DataAssets/DA_NPCData.h`
  - `Source/Pickpacker/DataAssets/DA_NPCData.cpp`
  - `Source/Pickpacker/DataAssets/DA_TrainDestinationData.h`
  - `Source/Pickpacker/DataAssets/DA_TrainDestinationData.cpp`
  - `Source/Pickpacker/Components/NPCDialogueComponent.h`
  - `Source/Pickpacker/Components/NPCDialogueComponent.cpp`
  - `Source/Pickpacker/Components/NPCCombatComponent.h`
  - `Source/Pickpacker/Components/NPCCombatComponent.cpp`
  - `Source/Pickpacker/Components/NPCLootTradeComponent.h`
  - `Source/Pickpacker/Components/NPCLootTradeComponent.cpp`
  - `Source/Pickpacker/NPC/ModularNPCActor.h`
  - `Source/Pickpacker/NPC/ModularNPCActor.cpp`

> 요청하신 `Source/Pickpacker/DataAssets/DA_TrainDestinationData.cpp` 도 포함되어 있습니다.

---

## 2. 이번 구현의 핵심 방향
이번 작업은 단순히 NPC를 추가하는 수준이 아니라, **런 기반 코어 루프 + 데이터 주도형 NPC + 월드 플래그 기반 서사 변화**를 하나의 흐름으로 묶는 방향으로 설계되었습니다.

핵심 목표는 다음 4가지입니다.

1. **코어 루프를 명시적인 상태 머신으로 분리**
2. **NPC를 역할별 전용 클래스가 아니라 모듈 조합형 구조로 전환**
3. **콘텐츠를 코드 하드코딩보다 데이터 에셋 중심으로 확장 가능하게 구성**
4. **GameState와 연동해 멀티플레이/복제 친화적으로 정리**

---

## 3. 가장 고민한 구현 1: 코어 루프 상태 체계 분리

### 관련 파일
- `Source/Pickpacker/PickpackerTypes/CoreLoopTypes.h`
- `Source/Pickpacker/Subsystem/CoreLoopSubsystem.h`
- `Source/Pickpacker/Subsystem/CoreLoopSubsystem.cpp`
- `Source/Pickpacker/GameState/PickpackerGameState.h`
- `Source/Pickpacker/GameState/PickpackerGameState.cpp`
- `Source/Pickpacker/GameMode/PickpackerGameMode.cpp`

### 설계 의도
게임 진행이 `Base -> Train -> Underground -> Escape` 로 반복/전환되는 구조이기 때문에, 이를 임시 bool 조합으로 관리하면 분기 폭이 급격히 커집니다. 그래서 별도 타입 `ECoreLoopPhase` 와 `FRunState` 를 두고, 전체 흐름을 `UCoreLoopSubsystem` 이 소유하게 했습니다.

### 왜 `Subsystem` 으로 분리했는가
- `GameMode` 는 이미 주문, 게임오버, 플레이 흐름을 많이 갖고 있어 책임이 무거움
- 코어 루프는 특정 액터보다 **월드 단위 시스템**에 가까움
- `WorldSubsystem` 으로 두면 레벨/월드 컨텍스트에서 직접 접근 가능
- 서버 권한 체크를 한 곳에 집중시킬 수 있음

### 핵심 타입 예시
```cpp
UENUM(BlueprintType)
enum class ECoreLoopPhase : uint8
{
	None,
	Base,
	Train,
	Underground,
	Escape
};

USTRUCT(BlueprintType)
struct PICKPACKER_API FRunState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRunActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECoreLoopPhase CurrentPhase = ECoreLoopPhase::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TeamCredits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CompletedTrips = 0;
};
```

### 구현 포인트
- `StartRun()` 에서 런 상태 초기화
- `SetPhase()` 에서 단계 전환과 이벤트 방송 일원화
- `SyncToGameState()` 로 `GameState` 에 복제용 값 반영
- `ReturnToBase()` 에서 왕복 횟수 증가
- `SelectTrainDestination()` 에서 목적지 선택과 현재 타겟 존 반영

### 대표 코드
```cpp
void UCoreLoopSubsystem::SetPhase(ECoreLoopPhase NewPhase)
{
	if (RunState.CurrentPhase == NewPhase)
	{
		return;
	}

	const ECoreLoopPhase OldPhase = RunState.CurrentPhase;
	RunState.CurrentPhase = NewPhase;

	SyncToGameState();
	OnPhaseChanged.Broadcast(OldPhase, NewPhase);
}
```

### 개선된 점
기존에는 게임 흐름이 `GameMode` 내부 로직에 흩어질 가능성이 컸는데, 지금은 **단계 전환의 중심점이 하나** 생겼습니다. 이후 Base 상점, Train 투표, Underground 결과 집계도 같은 축에 연결하기 쉬워졌습니다.

---

## 4. 가장 고민한 구현 2: NPC를 클래스 분기 대신 모듈 조합으로 구성

### 관련 파일
- `Source/Pickpacker/NPC/ModularNPCActor.h`
- `Source/Pickpacker/NPC/ModularNPCActor.cpp`
- `Source/Pickpacker/Components/NPCDialogueComponent.*`
- `Source/Pickpacker/Components/NPCCombatComponent.*`
- `Source/Pickpacker/Components/NPCLootTradeComponent.*`

### 기존 방식의 문제
NPC를 상인, 퀘스트 NPC, 적대 NPC, 사라지는 NPC 등으로 각각 클래스를 늘리면 조합 수가 폭증합니다.
예:
- 대화만 하는 NPC
- 대화 + 거래 NPC
- 대화 중 선택에 따라 적대화되는 NPC
- 월드 상태에 따라 사라졌다가 돌아오는 NPC

이 경우 상속 계층보다 **모듈 조합**이 더 적합합니다.

### 현재 구조
`AModularNPCActor` 하나를 기반으로 두고, 기능은 컴포넌트로 분리했습니다.
- `UNPCDialogueComponent`: 대화 트리 진행
- `UNPCCombatComponent`: 체력/시야/공격/타겟팅
- `UNPCLootTradeComponent`: 상점/루트/재고 처리

### 핵심 설계 포인트
- 액터는 상태와 상호작용 진입점 담당
- 실제 기능은 컴포넌트가 담당
- 데이터 에셋이 기본값을 공급
- 월드 플래그 변화가 disposition/role 을 재평가

### 대표 코드
```cpp
DialogueComponent = CreateDefaultSubobject<UNPCDialogueComponent>(TEXT("DialogueComponent"));
CombatComponent = CreateDefaultSubobject<UNPCCombatComponent>(TEXT("CombatComponent"));
LootTradeComponent = CreateDefaultSubobject<UNPCLootTradeComponent>(TEXT("LootTradeComponent"));
```

### 상호작용 우선순위 고민
`OnInteract_Implementation()` 에서는 다음 순서로 진입시켰습니다.
1. 실종 상태면 상호작용 차단
2. 대화 가능하면 대화 우선
3. 대화가 없고 Friendly 면 거래 진입

이렇게 한 이유는 거래형 NPC라도 서사 대화가 우선되는 경우가 많기 때문입니다.

### 대표 코드
```cpp
if (DialogueComponent && DialogueComponent->DialogueNodes.Num() > 0 && Disposition != ENPCDisposition::Hostile)
{
	if (!DialogueComponent->bInConversation)
	{
		DialogueComponent->StartDialogue(Interactor);
	}
	return;
}

if (LootTradeComponent && LootTradeComponent->TradeInventory.Num() > 0 && Disposition == ENPCDisposition::Friendly)
{
	if (!LootTradeComponent->bTradeOpen)
	{
		LootTradeComponent->OpenTrade(Interactor);
	}
	return;
}
```

---

## 5. 가장 고민한 구현 3: 월드 플래그 기반 서사 변화

### 관련 파일
- `Source/Pickpacker/PickpackerTypes/CoreLoopTypes.h`
- `Source/Pickpacker/Components/NPCDialogueComponent.cpp`
- `Source/Pickpacker/Components/NPCLootTradeComponent.cpp`
- `Source/Pickpacker/NPC/ModularNPCActor.cpp`
- `Source/Pickpacker/GameState/PickpackerGameState.h`

### 핵심 아이디어
NPC의 상태를 단순 enum 만으로 고정하지 않고, `EscapeProgressComponent` 의 월드 플래그를 읽어 런타임에서 변하게 했습니다.

예:
- 특정 플래그가 켜지면 NPC가 사라짐
- 특정 플래그가 켜지면 적대화
- 특정 선택지가 특정 플래그가 있어야만 보임
- 특정 거래 아이템이 플래그 충족 시에만 등장

### 대화 조건 분기
`FDialogueChoice`, `FDialogueNode`, `FDialogueOutcome` 구조체에 플래그/결과를 넣어 데이터 기반으로 분기하게 했습니다.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
FGameplayTag RequiredWorldFlag;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
FGameplayTag BlockingWorldFlag;
```

### 대화 결과 적용 예시
```cpp
case EDialogueOutcomeType::SetWorldFlag:
	if (EscapeProgress && Outcome.WorldFlag.IsValid())
	{
		EscapeProgress->SetWorldFlag(Outcome.WorldFlag, Outcome.IntValue != 0 ? Outcome.IntValue : 1);
	}
	break;
```

### NPC 상태 재평가 흐름
`AModularNPCActor::EvaluateNarrativeState()` 에서 일정 주기마다:
- `DisappearFlag` 확인
- `HostileFlag` 확인
- 플래그가 해제되면 기본 disposition 으로 복귀

이 부분이 중요한 이유는 **스토리 진행과 NPC 상태를 느슨하게 결합**하기 때문입니다. 레벨 블루프린트나 전용 스크립트에 하드코딩하지 않아도 됩니다.

---

## 6. 가장 고민한 구현 4: 데이터 에셋 기반 확장성 확보

### 관련 파일
- `Source/Pickpacker/DataAssets/DA_NPCData.h`
- `Source/Pickpacker/DataAssets/DA_NPCData.cpp`
- `Source/Pickpacker/DataAssets/DA_TrainDestinationData.h`
- `Source/Pickpacker/DataAssets/DA_TrainDestinationData.cpp`

### `DA_NPCData`
NPC 1개를 코드 수정 없이 설정 가능하도록 구성했습니다.

포함 정보:
- `FNPCProfile Profile`
- 메쉬/애님 오버라이드
- 전투용 AI 리소스 포인터

`FNPCProfile` 안에는 다음이 들어갑니다.
- NPC 기본 ID / 이름
- 기본 disposition / role
- 대화 노드 배열
- 거래 인벤토리
- 사라짐/적대화 플래그

### `DA_TrainDestinationData`
열차 목적지 목록을 데이터화하기 위한 에셋입니다.

```cpp
UCLASS(BlueprintType)
class PICKPACKER_API UDA_TrainDestinationData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train")
	TArray<FTrainDestination> Destinations;
};
```

### 왜 중요한가
특히 `DA_TrainDestinationData.cpp` 는 구현이 매우 작아 보이지만, 구조상 의미는 큽니다.
이 파일이 들어간다는 것은 앞으로 목적지 확장을:
- 코드 수정 없이 데이터 추가로 처리하고
- 난이도/언락 플래그/드랍 태그/레벨 경로를 에디터에서 관리하고
- `CoreLoopSubsystem` 과 연결해 목적지 선택 흐름을 확장할 수 있다는 뜻입니다.

즉, 작은 `.cpp` 파일이지만 **시스템 분리의 접점**입니다.

---

## 7. 가장 고민한 구현 5: 멀티플레이/복제 친화성

### 관련 파일
- `Source/Pickpacker/GameState/PickpackerGameState.h`
- `Source/Pickpacker/GameState/PickpackerGameState.cpp`
- `Source/Pickpacker/NPC/ModularNPCActor.cpp`
- 각 NPC 컴포넌트 생성자

### 방향
코어 루프, NPC disposition, trip count 같은 상태는 싱글플레이 전용 값이 아니라 **클라이언트 UI/연출도 알아야 하는 값**입니다.
그래서 `GameState` 와 replicated property 를 활용했습니다.

### 추가된 주요 복제 상태
- `CoreLoopPhase`
- `CompletedTrips`
- 기존 주문/크레딧/의심도 흐름과 연동
- NPC의 `Disposition`, `NPCRole`

### 대표 코드
```cpp
UPROPERTY(ReplicatedUsing = OnRep_CoreLoopPhase)
ECoreLoopPhase CoreLoopPhase = ECoreLoopPhase::None;

UPROPERTY(Replicated)
int32 CompletedTrips = 0;
```

```cpp
DOREPLIFETIME(APickpackerGameState, CoreLoopPhase);
DOREPLIFETIME(APickpackerGameState, CompletedTrips);
```

### 개선 포인트
- 서버는 `Subsystem` 과 `GameMode` 에서 authoritative 하게 상태 변경
- 클라이언트는 `GameState` 복제를 통해 반응
- 블루프린트는 delegate 로 UI 갱신 가능

이 방식은 이후 HUD, 미니맵, 열차 목적지 UI, NPC 상태 연출 연결에 유리합니다.

---

## 8. 시스템별 세부 정리

### 8-1. 대화 시스템
`UNPCDialogueComponent` 는 조건부 노드 스킵과 선택지 필터링을 지원합니다.

주요 포인트:
- 대화 시작/종료 이벤트 제공
- 선택지 표시 전 `RequiredWorldFlag`, `BlockingWorldFlag` 검사
- 결과로 월드 플래그 변경, 실종, 거래 오픈, 적대 전환 연계 가능
- 무한 루프 방지를 위해 노드 이동 시 safety loop 사용

### 8-2. 전투 시스템
`UNPCCombatComponent` 는 최소 전투 루프를 제공합니다.

포인트:
- 체력/사망 상태
- 시야 범위, 시야각 기반 타겟 탐지
- 공격 쿨다운
- 피격 시 공격자에게 어그로 전환

현재는 단순하지만, 이후 BT/AIController 연동 전의 베이스 레이어 역할을 합니다.

### 8-3. 거래 시스템
`UNPCLootTradeComponent` 는 태그 기반 재고 시스템입니다.

포인트:
- `FLootTradeEntry` 배열 사용
- 월드 플래그 조건부 노출
- 팀 크레딧 차감은 `PickpackerGameState` 와 연동
- 구매 성공 시 이벤트 방송

### 8-4. 목적지 시스템
`FTrainDestination` 구조는 다음 정보를 묶습니다.
- 목적지 ID
- 표시 이름
- 난이도
- 획득 가능한 아이템 태그 집합
- 언락 플래그
- 이동할 레벨 경로

이는 Train phase 를 실제 콘텐츠와 연결하는 최소 단위입니다.

---

## 9. 개선 과정 요약

### 1단계: 공통 타입 먼저 정리
처음부터 액터 구현으로 들어가면 구조가 금방 흔들릴 수 있어, `CoreLoopTypes.h` 로 enum/struct 를 먼저 고정했습니다.

효과:
- 서브시스템, 데이터 에셋, 컴포넌트가 같은 타입을 공유
- 블루프린트 노출 구조가 일관됨

### 2단계: 코어 루프 시스템 분리
`GameMode` 안에 직접 넣지 않고 `UCoreLoopSubsystem` 으로 뺐습니다.

효과:
- 게임 흐름 책임 분산
- 테스트 및 확장 포인트 명확화

### 3단계: NPC를 조합형으로 재구성
역할별 액터 분리 대신 `AModularNPCActor + 컴포넌트` 구조를 채택했습니다.

효과:
- 조합 수 폭발 억제
- 블루프린트 확장 단순화

### 4단계: 월드 상태 연동
단순 대화창 구현에 그치지 않고 `EscapeProgressComponent` 와 연결해 서사 상태를 시스템화했습니다.

효과:
- 선택지/거래/NPC 상태를 같은 플래그 체계로 통합

### 5단계: GameState 복제 연동
멀티플레이에서 상태를 UI에 안정적으로 노출하도록 `GameState` 복제를 정리했습니다.

효과:
- 클라이언트는 이벤트 수신만으로 표시 가능

---

## 10. 이번 작업에서 의미가 큰 코드 조각들

### 코어 루프 시작
```cpp
if (UCoreLoopSubsystem* CoreLoop = World->GetSubsystem<UCoreLoopSubsystem>())
{
	CoreLoop->StartRun(StartingTeamCredits);
}
```

### 목적지 등록/조회 기반 설계
```cpp
void UCoreLoopSubsystem::RegisterDestination(const FTrainDestination& Destination)
{
	for (const FTrainDestination& Existing : RegisteredDestinations)
	{
		if (Existing.DestinationId == Destination.DestinationId)
		{
			return;
		}
	}
	RegisteredDestinations.Add(Destination);
}
```

### 대화 결과로 NPC 적대화
```cpp
case EDialogueOutcomeType::AttackPlayer:
	SetDisposition(ENPCDisposition::Hostile);
	if (CombatComponent)
	{
		CombatComponent->SetTarget(Interactor);
	}
	break;
```

### 거래에서 팀 크레딧 차감
```cpp
GS->ApplyCreditDelta(-(Entry->CreditCost * Quantity), FString::Printf(TEXT("NPC Trade: %s x%d"), *ItemTag.ToString(), Quantity));
```

---

## 11. 현재 상태에서의 한계와 다음 개선 후보

### 현재 한계
- `DA_TrainDestinationData.cpp` 는 아직 보조 함수 구현이 거의 없음
- 목적지 데이터가 `CoreLoopSubsystem` 에 자동 등록되는 흐름은 아직 수동
- `NPCCombatComponent` 는 AIController/BehaviorTree 연동 전의 경량 버전
- 대화 `AutoNextNodeIndex` 자동 진행은 UI/타이머 레이어와 추가 연결 필요
- 거래는 재고/크레딧 처리 중심이고 실제 인벤토리 지급 연계는 후속 필요

### 다음 개선 후보
1. `GameMode` 또는 초기화 매니저에서 `DA_TrainDestinationData` 일괄 등록
2. `ModularNPCActor` 에 메쉬/애님 오버라이드 실제 적용
3. 대화 결과에 퀘스트/아이템 지급 실 구현 연결
4. 목적지 선택 UI와 `OnTrainDestinationSelected` 연결
5. NPC 전투를 AIController/BT 와 결합

---

## 12. 최종 요약
이번 구현의 핵심은 다음입니다.

- **코어 루프를 상태 기반 시스템으로 독립시켰다.**
- **NPC를 데이터 + 모듈 조합형 구조로 바꿨다.**
- **월드 플래그를 중심으로 대화, 거래, 적대화, 실종을 연결했다.**
- **열차 목적지와 NPC 설정을 데이터 에셋으로 빼서 확장성을 만들었다.**
- **`GameState` 복제를 통해 멀티플레이 반응 구조를 마련했다.**

특히 이번 작업은 단일 기능 추가보다, 앞으로 Pickpacker의 콘텐츠를 늘릴 때 재사용 가능한 **기반 프레임**을 만드는 성격이 강합니다.

---

## 13. 참고: `DA_TrainDestinationData.cpp` 포함 여부 재확인
포함되어 있습니다.

- `Source/Pickpacker/DataAssets/DA_TrainDestinationData.h`
- `Source/Pickpacker/DataAssets/DA_TrainDestinationData.cpp`

현재 `.cpp` 내용은 최소 구성이지만, 시스템 구조상 Train 목적지 데이터 에셋 계층의 일부로 정상 포함되어 있습니다.
