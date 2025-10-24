# 🎯 PCG 앵커시스템 최종 구조 정리

## 📋 **핵심 플로우**

### 1. **PCG 던전 생성 (BP_Dungeon)**
```
레벨 로드
├── BP_Dungeon 액터 생성
├── PCG 컴포넌트 자동 실행
├── PCG_MultiFloorDungeon 그래프 실행
└── 던전 구조 생성 (방, 계단, 벽, 스폰 포인트 등)
```

### 2. **PCG 생성 완료 후 앵커 추출**
```cpp
// BP_Dungeon에서 PCG 생성 완료 후 호출
PCGDungeonSubSystem->NotifyPCGGenerationComplete(PCGComponent)
├── PCG 컴포넌트의 GeneratedResources에서 액터 추출
│   └── UPCGManagedActors를 통해 생성된 액터들 접근
├── 태그 기반 필터링
│   ├── "PlayerSpawnPoint" → Extract 앵커 (플레이어 추출 포인트)
│   ├── "EnemySpawnPoint" → EnemySpawn 앵커 (적 스폰 포인트)
│   ├── "ObjectiveSpawnPoint" → Objective 앵커 (목표 지점)
│   └── "HazardSpawnPoint" → HazardSpawn 앵커 (함정 지점)
└── CachedGeneratedActors에 저장
```

### 3. **앵커 기반 게임플레이 액터 스폰**
```cpp
GeneratePCGAnchors(SeedSet)
├── CachedGeneratedActors 순회
├── 태그 기반 FPCGAnchorData 생성
│   ├── PlayerSpawnPoint → Extract 앵커
│   ├── EnemySpawnPoint → EnemySpawn 앵커
│   ├── ObjectiveSpawnPoint → Objective 앵커
│   └── HazardSpawnPoint → HazardSpawn 앵커
└── PCGAnchorSystem::ProcessAnchors()
    ├── 데이터 테이블 조회
    ├── 실제 게임플레이 액터 스폰
    │   ├── BP_ExtractionPortal (PlayerSpawnPoint에서)
    │   ├── BP_EnemySpawner (EnemySpawnPoint에서)
    │   ├── BP_ParcelActor (ObjectiveSpawnPoint에서)
    │   └── BP_ElectricFloorHazard (HazardSpawnPoint에서)
    └── 복제 설정 (SetReplicates, SetReplicateMovement)
```

## 🔧 **PCG 컴포넌트 액터 추출 방법 (UE 5.6)**

```cpp
void UPCGDungeonSubSystem::NotifyPCGGenerationComplete(UPCGComponent* InPCG)
{
    if (InPCG)
    {
        TArray<AActor*> GeneratedActors;
        
        // ForEachManagedResource를 사용하여 PCG 관리 리소스에서 액터 추출
        InPCG->ForEachManagedResource([&GeneratedActors, this](UPCGManagedResource* InResource)
        {
            // UPCGManagedActors로 캐스팅
            if (UPCGManagedActors* ManagedActors = Cast<UPCGManagedActors>(InResource))
            {
                // GeneratedActors 세트에서 액터들 가져오기
                const TSet<TObjectPtr<AActor>>& Actors = ManagedActors->GeneratedActors;
                for (const TObjectPtr<AActor>& ActorPtr : Actors)
                {
                    if (AActor* Actor = ActorPtr.Get())
                    {
                        GeneratedActors.Add(Actor);
                        CachedGeneratedActors.Add(Actor);
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

**⚠️ 중요:** 
- UE 5.6에서는 `GeneratedResources`가 private이고 `GetGeneratedActors()` 메서드도 없음
- **반드시 `ForEachManagedResource()` 메서드를 사용해야 함**
- 런타임에 생성된 액터들은 `UPCGManagedActors`의 `GeneratedActors` 세트에 저장됨
- `GetOwner()->GetAttachedActors()` 방식은 런타임 생성 액터를 찾지 못함

## 🏷️ **PCG 태그 시스템**

### **PCG 그래프에서 설정할 태그**
```
PCG 그래프 노드에서 Actor Tags 설정:
├── PCG.Generated - PCG가 자동으로 추가 (수동 설정 불필요)
├── PlayerSpawnPoint - 플레이어 스폰/추출 지점
├── EnemySpawnPoint - 적 스폰 지점
├── ObjectiveSpawnPoint - 목표 오브젝트 지점 (옵션)
└── HazardSpawnPoint - 함정 배치 지점 (옵션)
```

**⚠️ 주의:**
- `PCG.Generated` 태그는 PCG 시스템이 자동으로 추가하므로 수동으로 설정할 필요 없음
- 나머지 태그들은 PCG 그래프의 Static Mesh Spawner 또는 Actor Spawner 노드에서 설정

### **앵커 타입 매핑**
| PCG 태그 | 앵커 타입 | 용도 | 생성될 액터 |
|---------|----------|------|-----------|
| PlayerSpawnPoint | Extract | 플레이어 추출 포인트 | BP_ExtractionPortal |
| EnemySpawnPoint | EnemySpawn | 적 스폰 포인트 | BP_EnemySpawner |
| ObjectiveSpawnPoint | Objective | 목표 오브젝트 | BP_ParcelActor |
| HazardSpawnPoint | HazardSpawn | 함정 | BP_ElectricFloorHazard |

## 📂 **필요한 블루프린트 준비**

### 1. **데이터 테이블**
```
Content/Pickpacker/Data/
├── DT_Objectives.uasset (FObjectiveRow)
│   ├── Extract_PlayerSpawn → BP_ExtractionPortal
│   └── Objective_1 → BP_ParcelActor
└── DT_Spawners.uasset (FSpawnerRow)
    ├── EnemySpawn_1 → BP_EnemySpawner
    └── HazardSpawn_1 → BP_ElectricFloorHazard
```

### 2. **게임플레이 액터 블루프린트**
```
Content/Pickpacker/Blueprints/
├── Objectives/
│   ├── BP_ExtractionPortal (AGameplayAbilityTargetActor 기반)
│   └── BP_ParcelActor (AParcelActor 기반)
├── Enemies/
│   ├── BP_EnemySpawner (AActor 기반)
│   ├── BP_Stalker (ACharacter 기반)
│   └── BP_Watcher (ACharacter 기반)
└── Hazards/
    └── BP_ElectricFloorHazard (AActor 기반)
```

## 🎮 **BP_Dungeon 이벤트 그래프 설정**

```
Event Graph:
├── Event BeginPlay
│   └── [기존 PCG 생성 로직]
└── Event PCG Generation Complete (커스텀 이벤트)
    ├── Get Subsystem (PCGDungeonSubSystem)
    ├── NotifyPCGGenerationComplete(PCG Component)
    └── [게임 시작 로직]
```

## 🔄 **전체 흐름도**

```
1. 레벨 로드
   ↓
2. BP_Dungeon 생성
   ↓
3. PCG 컴포넌트 실행
   ↓
4. PCG_MultiFloorDungeon 그래프 실행
   ├── 던전 구조 생성
   └── 스폰 포인트 생성 (태그 설정)
   ↓
5. PCG 생성 완료 이벤트
   ↓
6. NotifyPCGGenerationComplete(PCGComponent)
   ├── GeneratedResources에서 액터 추출
   ├── 태그 기반 필터링
   │   ├── PlayerSpawnPoint
   │   ├── EnemySpawnPoint
   │   ├── ObjectiveSpawnPoint
   │   └── HazardSpawnPoint
   └── CachedGeneratedActors에 저장
   ↓
7. GeneratePCGAnchors(SeedSet)
   ├── CachedGeneratedActors 순회
   └── FPCGAnchorData 생성
   ↓
8. ProcessAnchors(GameplayAnchors)
   ├── 데이터 테이블 조회
   ├── 게임플레이 액터 스폰
   │   ├── BP_ExtractionPortal
   │   ├── BP_EnemySpawner
   │   ├── BP_ParcelActor
   │   └── BP_ElectricFloorHazard
   └── 복제 설정
   ↓
9. 게임 시작
```

## ⚠️ **중요 사항**

1. **PCG 생성 액터는 아웃라이너에 표시되지 않음**
   - `UPCGManagedActors`를 통해서만 접근 가능
   - `GetAttachedActors()`로는 가져올 수 없음

2. **태그 기반 시스템**
   - PCG 그래프에서 생성되는 액터에 태그 설정 필수
   - 태그로 스폰 포인트 타입 구분

3. **서버 전용 생성**
   - PCG 생성 및 앵커 처리는 서버에서만 실행
   - 클라이언트는 복제된 액터만 수신

4. **데이터 테이블 연동**
   - 앵커 태그와 데이터 테이블 Row Name 매칭
   - 실제 스폰될 액터 클래스 지정

## 🎯 **다음 단계**

1. PCG 그래프에서 스폰 포인트에 태그 설정
2. BP_Dungeon에서 PCG 생성 완료 이벤트 연결
3. 데이터 테이블 및 블루프린트 생성
4. 테스트 및 검증

이제 PCG 던전 생성과 게임플레이 요소 스폰이 완전히 통합되었습니다!
