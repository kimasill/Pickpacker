# 🎯 PCG 앵커시스템 사용처 및 구조 정리

## 📋 **현재 상황 분석**

### ✅ **이미 구성된 것들**
```
Plugins/PCGDungeonGenerator/PCGDungeonGenerator/Content/
├── PCG/
│   ├── PCG_MultiFloorDungeon.uasset          # 메인 PCG 그래프
│   ├── PCG_SingleFloor.uasset                 # 단층 PCG 그래프
│   ├── PCG_FloorLoop.uasset                   # 바닥 루프
│   ├── PCG_Stairs.uasset                      # 계단
│   ├── PCG_Walls.uasset                       # 벽
│   ├── PCG_Corners.uasset                     # 모서리
│   ├── PCG_Rooms.uasset                       # 방
│   └── Subgraphs/                             # PCG 서브그래프들
├── DataAssets/
│   ├── DA_Object.uasset                       # 오브젝트 데이터 에셋
│   ├── DA_RegularBuiling.uasset               # 일반 건물 데이터 에셋
│   └── Data/                                  # 데이터 테이블들
└── Maps/
    ├── PCG_Mension.umap                       # PCG 맨션 맵
    └── PCG_MultiFloorDungeon.umap             # PCG 다층 던전 맵
```

### 🔄 **현재 PCG 생성 플로우**
```
레벨 로드 → BP_Dungeon 블루프린트 활성화
├── PCG 컴포넌트 자동 실행
├── PCG_MultiFloorDungeon 그래프 실행
├── 던전 구조 자동 생성 (방, 계단, 벽 등)
└── 완성된 던전 환경 제공
```

## 🎯 **앵커시스템의 실제 사용처**

### 1. **PCG 생성 후 게임플레이 요소 배치**
```
PCG 던전 생성 완료 → 앵커시스템 작동
├── PCG가 생성한 던전 내부에서
├── 게임플레이 요소들을 적절한 위치에 배치
│   ├── 목표 오브젝트 (택배물)
│   ├── 추출 포인트
│   ├── 적 스폰 포인트
│   ├── 함정 배치
│   └── 아이템 스폰 포인트
└── 멀티플레이어 복제 설정
```

### 2. **앵커시스템의 구체적 역할**

#### **A. PCG 생성 완료 감지**
```cpp
// PCGDungeonSubSystem에서 PCG 생성 완료를 감지
void UPCGDungeonSubSystem::OnPCGGenerationComplete()
{
    // PCG 던전이 생성된 후 앵커시스템 실행
    TArray<FPCGAnchorData> GameplayAnchors = GenerateGameplayAnchors();
    AnchorSystem->ProcessAnchors(GameplayAnchors);
}
```

#### **B. 게임플레이 앵커 생성**
```cpp
// PCG 던전 내부에서 게임플레이 요소 위치 결정
TArray<FPCGAnchorData> GenerateGameplayAnchors()
{
    TArray<FPCGAnchorData> Anchors;
    
    // PCG가 생성한 방들 중에서 적절한 위치 선택
    // 예: 큰 방에 목표 오브젝트, 좁은 통로에 함정 등
    
    // 1. 목표 오브젝트 앵커 (택배물)
    FPCGAnchorData ObjectiveAnchor;
    ObjectiveAnchor.AnchorType = EPCGAnchorType::Objective;
    ObjectiveAnchor.Location = FindSuitableRoomCenter(); // PCG 생성된 방 중앙
    Anchors.Add(ObjectiveAnchor);
    
    // 2. 추출 포인트 앵커
    FPCGAnchorData ExtractAnchor;
    ExtractAnchor.AnchorType = EPCGAnchorType::Extract;
    ExtractAnchor.Location = FindSpawnArea(); // PCG 생성된 스폰 영역
    Anchors.Add(ExtractAnchor);
    
    // 3. 적 스폰 앵커
    TArray<FVector> EnemySpawnPoints = FindEnemySpawnPoints(); // PCG 생성된 적절한 위치들
    for (const FVector& SpawnPoint : EnemySpawnPoints)
    {
        FPCGAnchorData EnemyAnchor;
        EnemyAnchor.AnchorType = EPCGAnchorType::EnemySpawn;
        EnemyAnchor.Location = SpawnPoint;
        Anchors.Add(EnemyAnchor);
    }
    
    return Anchors;
}
```

#### **C. 앵커 기반 액터 스폰**
```cpp
// PCGAnchorSystem에서 실제 게임플레이 액터들 스폰
bool UPCGAnchorSystem::ProcessAnchors(const TArray<FPCGAnchorData>& Anchors)
{
    for (const FPCGAnchorData& Anchor : Anchors)
    {
        switch (Anchor.AnchorType)
        {
        case EPCGAnchorType::Objective:
            // 택배물 스폰 (BP_ParcelActor)
            SpawnParcelActor(Anchor);
            break;
            
        case EPCGAnchorType::Extract:
            // 추출 포털 스폰 (BP_ExtractionPortal)
            SpawnExtractionPortal(Anchor);
            break;
            
        case EPCGAnchorType::EnemySpawn:
            // 적 스폰 포인트 생성 (BP_EnemySpawner)
            SpawnEnemySpawner(Anchor);
            break;
            
        case EPCGAnchorType::HazardSpawn:
            // 함정 배치 (BP_ElectricFloorHazard)
            SpawnHazard(Anchor);
            break;
        }
    }
}
```

## 🔧 **실제 구현 방법**

### 1. **PCG 생성 완료 감지**
```cpp
// PCGDungeonSubSystem.h
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPCGGenerationComplete);

UPROPERTY(BlueprintAssignable, Category = "PCG Dungeon")
FOnPCGGenerationComplete OnPCGGenerationComplete;

// PCG 생성 완료 시 호출
void NotifyPCGGenerationComplete();
```

### 2. **PCG 던전 내부 위치 탐색**
```cpp
// PCG가 생성한 던전 내부에서 적절한 위치 찾기
FVector FindSuitableRoomCenter()
{
    // PCG가 생성한 방들의 중심점 중에서 선택
    // 큰 방, 안전한 위치, 접근 가능한 위치 등 고려
    return FVector::ZeroVector; // 실제 구현 필요
}

TArray<FVector> FindEnemySpawnPoints()
{
    // PCG가 생성한 통로, 작은 방, 숨겨진 공간 등에서 선택
    TArray<FVector> SpawnPoints;
    // 실제 구현 필요
    return SpawnPoints;
}
```

### 3. **데이터 테이블 연동**
```cpp
// DT_Objectives 테이블 예시
Row Name: Objective_1
- Class: BP_ParcelActor
- Count: 1
- Required: true
- Description: "Primary objective parcel"

Row Name: Extract_1  
- Class: BP_ExtractionPortal
- Count: 1
- Required: true
- Description: "Main extraction point"

// DT_Spawners 테이블 예시
Row Name: EnemySpawn_1
- Class: BP_StalkerSpawner
- Count: 2
- SpawnChance: 1.0
- Description: "Stalker enemy spawner"

Row Name: HazardSpawn_1
- Class: BP_ElectricFloorHazard
- Count: 1
- SpawnChance: 0.8
- Description: "Electric floor hazard"
```

## 🎮 **사용 시나리오**

### **시나리오 1: 게임 시작**
```
1. 레벨 로드
2. BP_Dungeon의 PCG 컴포넌트 실행
3. PCG_MultiFloorDungeon 그래프로 던전 생성
4. 던전 생성 완료 신호 감지
5. 앵커시스템이 게임플레이 요소들 배치
6. 멀티플레이어 복제 설정
7. 게임 시작
```

### **시나리오 2: 미션 변경**
```
1. 새 미션 시작
2. 새로운 시드로 PCG 재생성
3. 던전 구조 변경
4. 앵커시스템이 새로운 위치에 게임플레이 요소 배치
5. 플레이어들에게 새로운 던전 제공
```

## 📝 **요약**

**앵커시스템은 PCG 던전 생성과 게임플레이 요소 배치 사이의 브릿지 역할을 합니다.**

- **PCG**: 던전 구조 (방, 계단, 벽, 문 등) 생성
- **앵커시스템**: 생성된 던전 내부에 게임플레이 요소 (목표, 적, 함정 등) 배치
- **결과**: 완전한 게임 레벨 제공

이렇게 PCG가 환경을 만들고, 앵커시스템이 그 환경에 게임플레이를 추가하는 구조입니다!
