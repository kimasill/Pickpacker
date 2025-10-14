# 🎯 Pickpacker PCG Level 생성 구조 정리

## 📁 프로젝트 구조

### 1. **PCG Plugin 구조**
```
Plugins/PCGDungeonGenerator/PCGDungeonGenerator/
├── Content/
│   ├── Maps/
│   │   ├── PCG_Mension.umap                    # PCG 맨션 맵
│   │   └── PCG_MultiFloorDungeon.umap          # PCG 다층 던전 맵
│   ├── PCG/
│   │   ├── PCG_MultiFloorDungeon.uasset        # 메인 PCG 그래프
│   │   ├── PCG_SingleFloor.uasset              # 단층 PCG 그래프
│   │   ├── PCG_FloorLoop.uasset                # 바닥 루프 PCG
│   │   ├── PCG_Stairs.uasset                   # 계단 PCG
│   │   ├── PCG_Walls.uasset                    # 벽 PCG
│   │   ├── PCG_Corners.uasset                  # 모서리 PCG
│   │   ├── PCG_Rooms.uasset                    # 방 PCG
│   │   └── Subgraphs/                          # PCG 서브그래프들
│   │       ├── PCG_AnchorMode.uasset           # 앵커 모드
│   │       ├── PCG_FacingMode.uasset           # 방향 모드
│   │       ├── PCG_StairAnchors.uasset         # 계단 앵커
│   │       └── ...
│   ├── Assets/
│   │   ├── Mension/                           # 맨션 에셋들
│   │   │   ├── Materials/                     # 재질
│   │   │   ├── StaticMeshes/                  # 정적 메시
│   │   │   └── Textures/                      # 텍스처
│   │   └── BluePrints/                        # 블루프린트
│   └── DataAssets/
│       ├── DA_Object.uasset                   # 오브젝트 데이터 에셋
│       ├── DA_RegularBuiling.uasset           # 일반 건물 데이터 에셋
│       └── Data/                              # 데이터 테이블들
└── PCGDungeonGenerator.uplugin               # PCG 플러그인 설정
```

### 2. **Pickpacker 데이터 구조**
```
Content/Pickpacker/
├── Data/                                      # 데이터 테이블 폴더
│   ├── DT_Objectives.uasset                   # 목표 데이터 테이블
│   ├── DT_Spawners.uasset                     # 스포너 데이터 테이블
│   └── DT_Progression.uasset                   # 진행도 데이터 테이블
├── Maps/
│   ├── PCG_TestMap.umap                       # PCG 테스트 맵
│   └── PickpackerMainMap.umap                 # 메인 게임 맵
├── Blueprints/
│   ├── Parcels/
│   │   ├── BP_ParcelActor.uasset              # 택배물 액터
│   │   ├── BP_FragileParcel.uasset            # 깨지기 쉬운 택배물
│   │   ├── BP_HeavyParcel.uasset              # 무거운 택배물
│   │   └── BP_UnstableParcel.uasset           # 불안정한 택배물
│   ├── Objectives/
│   │   ├── BP_ObjectiveActor.uasset           # 목표 액터
│   │   └── BP_ExtractionPortal.uasset         # 추출 포털
│   ├── Enemies/
│   │   ├── BP_Stalker.uasset                  # 스토커 적
│   │   └── BP_Watcher.uasset                  # 와처 적
│   ├── Hazards/
│   │   └── BP_ElectricFloorHazard.uasset      # 전류 바닥 함정
│   └── GameModes/
│       ├── BP_PickpackerGameMode.uasset        # 픽패커 게임 모드
│       └── BP_PickpackerGameState.uasset       # 픽패커 게임 스테이트
└── UI/
    ├── WBP_ParcelHUD.uasset                   # 택배물 HUD 위젯
    ├── WBP_SquadHUD.uasset                    # 스쿼드 HUD 위젯
    └── WBP_MissionBriefing.uasset             # 미션 브리핑 위젯
```

## 🔄 PCG Level 생성 플로우

### 1. **게임 시작 시 PCG 생성**
```
APickpackerGameMode::HandleMatchStart()
├── SetMissionConfig(MissionId, Seed)
├── PickpackerGameState::SetSeedSet(SeedSet)
└── PCGDungeonSubSystem::GenerateDungeon(SeedSet)
    ├── IsPCGGenerationAllowed() [서버 전용 체크]
    ├── InternalGenerateDungeon(SeedSet)
    │   ├── FMath::RandInit(Seed) [시드 설정]
    │   ├── GeneratePCGAnchors(SeedSet)
    │   │   ├── Objective Anchors 생성
    │   │   ├── Extract Anchors 생성
    │   │   ├── Enemy Spawn Anchors 생성
    │   │   └── Hazard Spawn Anchors 생성
    │   └── PCGAnchorSystem::ProcessAnchors(Anchors)
    │       ├── DT_Objectives 테이블 소비
    │       ├── DT_Spawners 테이블 소비
    │       ├── 앵커별 액터 스폰
    │       └── 검증 로그 출력
    └── SpawnPCGActors(GeneratedActors)
        ├── SetReplicates(true)
        ├── SetReplicateMovement(true)
        └── SetNetCullDistanceSquared()
```

### 2. **PCG 앵커 태그 시스템**
```
PCG 그래프에서 생성된 앵커들:
├── PCG.Anchor.Objective → BP_ObjectiveActor 스폰
├── PCG.Anchor.Extract → BP_ExtractionPortal 스폰
├── PCG.Anchor.EnemySpawn → BP_Stalker/Watcher 스폰
└── PCG.Anchor.HazardSpawn → BP_ElectricFloorHazard 스폰
```

### 3. **데이터 테이블 연동**
```
DT_Objectives 테이블:
├── Objective_1: BP_ObjectiveActor, Count=1, Required=true
├── Objective_2: BP_ObjectiveActor, Count=1, Required=true
└── Extract_1: BP_ExtractionPortal, Count=1, Required=true

DT_Spawners 테이블:
├── EnemySpawn_1: BP_StalkerSpawner, Count=2, SpawnChance=1.0
├── EnemySpawn_2: BP_WatcherSpawner, Count=1, SpawnChance=0.8
└── HazardSpawn_1: BP_ElectricFloorHazard, Count=1, SpawnChance=0.6
```

## 🛠️ 언리얼 엔진에서 해야 할 작업들

### 1. **데이터 테이블 생성**
```
Content/Pickpacker/Data/ 폴더 생성
├── DT_Objectives 생성 (FObjectiveRow 구조체 기반)
├── DT_Spawners 생성 (FSpawnerRow 구조체 기반)
└── DT_Progression 생성 (진행도 시스템용)
```

### 2. **블루프린트 클래스 생성**
```
Content/Pickpacker/Blueprints/ 폴더 구조 생성
├── Parcels/ 폴더
│   ├── BP_ParcelActor (AParcelActor 기반)
│   ├── BP_FragileParcel (Fragile 타입)
│   ├── BP_HeavyParcel (Heavy 타입)
│   └── BP_UnstableParcel (Unstable 타입)
├── Objectives/ 폴더
│   ├── BP_ObjectiveActor (목표 액터)
│   └── BP_ExtractionPortal (추출 포털)
├── Enemies/ 폴더
│   ├── BP_Stalker (근접 적)
│   └── BP_Watcher (원거리 적)
├── Hazards/ 폴더
│   └── BP_ElectricFloorHazard (전류 바닥)
└── GameModes/ 폴더
    ├── BP_PickpackerGameMode
    └── BP_PickpackerGameState
```

### 3. **UI 위젯 생성**
```
Content/Pickpacker/UI/ 폴더 생성
├── WBP_ParcelHUD (UParcelHUDWidget 기반)
├── WBP_SquadHUD (스쿼드 HUD)
└── WBP_MissionBriefing (미션 브리핑)
```

### 4. **PCG 맵 설정**
```
Content/Pickpacker/Maps/ 폴더 생성
├── PCG_TestMap.umap 생성
│   ├── PCG Volume 배치
│   ├── PCG_MultiFloorDungeon 그래프 연결
│   └── 테스트용 플레이어 스폰
└── PickpackerMainMap.umap 생성
    ├── PCG Volume 배치
    ├── PCG 그래프 연결
    └── 게임플레이 요소 배치
```

### 5. **프로젝트 설정**
```
Project Settings > Maps & Modes
├── Default GameMode: BP_PickpackerGameMode
├── Default GameState: BP_PickpackerGameState
└── Default Map: PCG_TestMap

Project Settings > Input
├── Interact (E키)
├── Drop (G키)
└── Ping (마우스 우클릭)
```

## 🔧 PCG 그래프 커스터마이징

### 1. **기존 PCG 그래프 활용**
```
PCG_MultiFloorDungeon.uasset 사용
├── 다층 던전 생성
├── 계단 연결
├── 방 배치
└── 앵커 포인트 생성
```

### 2. **앵커 포인트 확장**
```
기존 PCG에 추가할 앵커들:
├── Objective Anchors (목표 지점)
├── Extract Anchors (추출 지점)
├── Enemy Spawn Anchors (적 스폰 지점)
└── Hazard Spawn Anchors (함정 지점)
```

### 3. **데이터 에셋 연동**
```
DA_Object.uasset 활용
├── 오브젝트 스폰 정보
├── 스케일링 정보
└── 배치 규칙
```

## 📋 체크리스트

### ✅ 완료된 작업
- [x] C++ 코드 구현 (T1-01, T1-02, T1-10, T1-11, T1-12, T1-20)
- [x] PCG Plugin 구조 파악
- [x] 데이터 구조 설계

### 🔄 진행 중인 작업
- [ ] 데이터 테이블 생성
- [ ] 블루프린트 클래스 생성
- [ ] UI 위젯 생성
- [ ] PCG 맵 설정

### 📝 다음 단계
- [ ] T1-21 (회전/문 통과 제약) 구현
- [ ] T1-30 (목표 시스템) 구현
- [ ] T1-31 (출구 & 카운트다운) 구현
- [ ] T1-32 (정산 UI) 구현

이 구조를 기반으로 언리얼 엔진에서 순차적으로 작업을 진행하시면 됩니다!
