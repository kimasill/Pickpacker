# 🎯 PCG 앵커시스템 - UE 5.6 최종 솔루션

## 📋 **핵심 문제와 해결**

### ❌ **작동하지 않는 방법들**

1. **GeneratedResources 직접 접근**
   ```cpp
   // ❌ private 멤버 - 컴파일 에러
   InPCG->GeneratedResources
   ```

2. **GetGeneratedActors() 메서드**
   ```cpp
   // ❌ UE 5.6에는 이 메서드가 없음
   InPCG->GetGeneratedActors()
   ```

3. **GetOwner()->GetAttachedActors()**
   ```cpp
   // ❌ 런타임 PCG 생성 액터들이 Owner 아래에 없음
   PCGOwner->GetAttachedActors(AttachedActors)
   ```

4. **리플렉션 방식 (복잡하고 불안정)**
   ```cpp
   // ❌ 너무 복잡하고 for 루프에서 스킵됨
   FScriptSetHelper SetHelper(SetProp, SetPtr);
   for (int32 Index = 0; Index < SetHelper.GetMaxIndex(); ++Index) // 작동 안 함
   ```

### ✅ **올바른 해결 방법 (UE 5.6)**

```cpp
void UPCGDungeonSubSystem::NotifyPCGGenerationComplete(UPCGComponent* InPCG)
{
    if (InPCG)
    {
        TArray<AActor*> GeneratedActors;

        // ForEachManagedResource로 PCG 관리 리소스 순회
        InPCG->ForEachManagedResource([&GeneratedActors, this](UPCGManagedResource* Resource)
        {
            if (!Resource) return;

            // UPCGManagedActors로 캐스팅
            if (UPCGManagedActors* ManagedActors = Cast<UPCGManagedActors>(Resource))
            {
                TArray<AActor*> ResourceActors;
                
                // GetActors() public API 사용
                if (ManagedActors->GetActors(ResourceActors))
                {
                    for (AActor* Actor : ResourceActors)
                    {
                        if (Actor && IsValid(Actor))
                        {
                            GeneratedActors.Add(Actor);
                            CachedGeneratedActors.Add(Actor);
                        }
                    }
                }
            }
        });

        // 태그로 스폰 포인트 필터링
        for (AActor* Actor : GeneratedActors)
        {
            if (Actor->ActorHasTag(FName("PlayerSpawnPoint")))
            {
                // PlayerSpawnPoint 처리
            }
            else if (Actor->ActorHasTag(FName("EnemySpawnPoint")))
            {
                // EnemySpawnPoint 처리
            }
        }
    }
}
```

## 🔑 **핵심 API**

### **ForEachManagedResource**
```cpp
// UPCGComponent의 모든 관리 리소스를 순회하는 public 메서드
InPCG->ForEachManagedResource([](UPCGManagedResource* Resource)
{
    // 각 리소스 처리
});
```

### **UPCGManagedActors::GetActors**
```cpp
// UPCGManagedActors에서 생성된 액터 배열을 가져오는 public 메서드
TArray<AActor*> ResourceActors;
if (ManagedActors->GetActors(ResourceActors))
{
    // 액터 처리
}
```

## 🔄 **완전한 플로우**

```
1. PCG 던전 생성 완료
   ↓
2. BP_Dungeon에서 NotifyPCGGenerationComplete(PCGComponent) 호출
   ↓
3. ForEachManagedResource()로 PCG 관리 리소스 순회
   ├── UPCGManagedResource* Resource 획득
   └── UPCGManagedActors로 캐스팅 시도
   ↓
4. ManagedActors->GetActors(ResourceActors) 호출
   ├── PCG가 생성한 모든 액터 반환
   └── 아웃라이너에 표시되지 않는 런타임 액터 포함
   ↓
5. 태그 기반 필터링
   ├── "PlayerSpawnPoint" → Extract 앵커
   ├── "EnemySpawnPoint" → EnemySpawn 앵커
   ├── "ObjectiveSpawnPoint" → Objective 앵커
   └── "HazardSpawnPoint" → HazardSpawn 앵커
   ↓
6. CachedGeneratedActors에 저장
   ↓
7. GeneratePCGAnchors() 호출
   ↓
8. 앵커 데이터 생성
   ↓
9. ProcessAnchors()로 게임플레이 액터 스폰
   ├── BP_ExtractionPortal (PlayerSpawnPoint)
   ├── BP_EnemySpawner (EnemySpawnPoint)
   ├── BP_ParcelActor (ObjectiveSpawnPoint)
   └── BP_ElectricFloorHazard (HazardSpawnPoint)
```

## 🏷️ **PCG 태그 시스템**

### **PCG 그래프에서 설정할 태그**
```
PCG_MultiFloorDungeon 그래프의 Actor Spawner 노드:
├── Actor Tags 속성에 추가
│   ├── PlayerSpawnPoint (플레이어 스폰/추출 지점)
│   ├── EnemySpawnPoint (적 스폰 지점)
│   ├── ObjectiveSpawnPoint (목표 오브젝트 지점)
│   └── HazardSpawnPoint (함정 배치 지점)
```

### **앵커 타입 매핑**
| PCG 태그 | 앵커 타입 | 용도 | 생성될 액터 |
|---------|----------|------|-----------|
| PlayerSpawnPoint | Extract | 플레이어 추출 포인트 | BP_ExtractionPortal |
| EnemySpawnPoint | EnemySpawn | 적 스폰 포인트 | BP_EnemySpawner |
| ObjectiveSpawnPoint | Objective | 목표 오브젝트 | BP_ParcelActor |
| HazardSpawnPoint | HazardSpawn | 함정 | BP_ElectricFloorHazard |

## 📝 **필요한 헤더**

```cpp
#include "PCGComponent.h"          // UPCGComponent
#include "PCGManagedResource.h"    // UPCGManagedResource, UPCGManagedActors
#include "Kismet/GameplayStatics.h" // 기타 유틸리티
```

## ⚠️ **중요 사항**

1. **런타임 PCG 생성 액터의 특성**
   - 아웃라이너에 표시되지 않음
   - `GetAllActorsOfClass()`, `GetAllActorsWithTag()` 등으로 검색 불가
   - PCG Owner의 자식 액터가 아님
   - **오직 `ForEachManagedResource()`와 `GetActors()`로만 접근 가능**

2. **UPCGManagedActors**
   - PCG가 생성한 액터들을 관리하는 리소스 타입
   - `GetActors()` public 메서드로 액터 배열 반환
   - 내부적으로 `TSet<TObjectPtr<AActor>>` 사용

3. **ForEachManagedResource**
   - PCG 컴포넌트의 모든 관리 리소스를 순회
   - 람다 함수로 각 리소스 처리
   - 리소스 타입별로 캐스팅하여 사용

## 🎮 **BP_Dungeon 이벤트 그래프 설정**

```
Event Graph:
├── Event BeginPlay
│   └── PCG Component Generate
└── Event PCG Generation Complete
    ├── Get Game Instance Subsystem (PCGDungeonSubSystem)
    ├── NotifyPCGGenerationComplete (PCG Component 전달)
    └── [게임 시작 로직]
```

## 🎯 **결론**

**UE 5.6에서 PCG 생성 액터를 가져오는 유일한 방법:**
1. `ForEachManagedResource()`로 PCG 관리 리소스 순회
2. `UPCGManagedActors`로 캐스팅
3. `GetActors()` public API로 액터 배열 획득

이 방법이 **가장 간단하고 안정적이며 공식적인 UE 5.6 PCG API**입니다!


