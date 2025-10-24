# 🎯 PCG 앵커시스템 - Blueprint 구현 가이드 (UE 5.6)

## ⚠️ **중요: C++ API 제한 사항**

UE 5.6의 `UPCGManagedActors`는 C++에서 직접 액터를 가져올 수 있는 public 메서드가 없습니다:
- ❌ `GeneratedResources` - private 멤버
- ❌ `GetGeneratedActors()` - 존재하지 않음
- ❌ `GetActors()` - 존재하지 않음
- ❌ `GetActor()` - 존재하지 않음

**해결책: Blueprint에서 PCG 생성 액터를 수집하여 C++ 서브시스템에 전달**

## 🔧 **Blueprint 구현 방법**

### 1. **BP_PCGDungeonSubsystem 생성**

`Content/Pickpacker/Blueprints/Subsystems/` 폴더에 생성

```
BP_PCGDungeonSubsystem
├── Parent Class: PCGDungeonSubSystem
└── Event Graph 구현
```

### 2. **CollectPCGGeneratedActors 이벤트 구현**

```
Event CollectPCGGeneratedActors
├── Input: InPCG (PCG Component)
├── Input: Out Actors (Array of Actors) [Reference]
└── Implementation:
    ├── [방법 1] PCG Component의 Generated Actors 직접 접근
    │   ├── Get Managed Resources (InPCG)
    │   ├── For Each Loop
    │   │   ├── Cast to PCGManagedActors
    │   │   ├── Get Generated Actors (Blueprint 노드)
    │   │   └── Append to Out Actors
    │   └── Return
    │
    └── [방법 2] World Tag 검색 (Fallback)
        ├── Get All Actors with Tag ("PCG.Generated")
        ├── Filter by Owner (InPCG->GetOwner())
        └── Append to Out Actors
```

### 3. **Blueprint 노드 구성**

#### **방법 1: PCG 컴포넌트에서 직접 수집 (권장)**

```
Event CollectPCGGeneratedActors (In PCG, Out Actors)
│
├─ ForEachLoop (In PCG → Get Managed Resources)
│  │
│  ├─ Cast to PCGManagedActors
│  │  │
│  │  └─ Branch (Success?)
│  │     │
│  │     ├─ Yes → Get Generated Actors (Blueprint exposed)
│  │     │         │
│  │     │         └─ Append to Out Actors
│  │     │
│  │     └─ No → Continue
│  │
│  └─ Return
```

#### **방법 2: World Tag 검색 (Fallback)**

```
Event CollectPCGGeneratedActors (In PCG, Out Actors)
│
├─ Get All Actors with Tag
│  ├─ Tag: "PCG.Generated"
│  └─ Actor Class: Actor
│
├─ ForEachLoop (All Tagged Actors)
│  │
│  ├─ Get Root Component → Get Attach Parent
│  │  │
│  │  └─ Compare (Parent == In PCG?)
│  │     │
│  │     ├─ Yes → Add to Out Actors
│  │     └─ No → Continue
│  │
│  └─ Return
```

## 📋 **상세 구현 단계**

### **Step 1: BP_PCGDungeonSubsystem 생성**

1. Content Browser에서 우클릭
2. Blueprint Class → PCGDungeonSubSystem 선택
3. 이름: `BP_PCGDungeonSubsystem`
4. 저장 위치: `Content/Pickpacker/Blueprints/Subsystems/`

### **Step 2: CollectPCGGeneratedActors 이벤트 구현**

1. BP_PCGDungeonSubsystem 열기
2. Event Graph에서 우클릭
3. "Add Event" → "Event Collect PCG Generated Actors" 선택
4. 다음 노드들 연결:

```
[Event Collect PCG Generated Actors]
  ↓
[ForEachLoop] ← [Get Managed Resources] ← [In PCG]
  ↓ (Loop Body)
[Branch] ← [Cast to PCGManagedActors] ← [Array Element]
  ↓ (True)
[Append] ← [Get Generated Actors] 
  ↓        [Cast Result]
[Out Actors] (설정)
```

### **Step 3: Project Settings 설정**

```
Project Settings → Engine → Subsystems
├── Default World Subsystems 추가
└── BP_PCGDungeonSubsystem 등록
```

## 🎮 **BP_Dungeon 연동**

### **PCG 생성 완료 이벤트 설정**

```
BP_Dungeon Event Graph:
│
├─ Event BeginPlay
│  └─ [PCG Component] → Generate
│
└─ Event PCG Generation Complete
   ├─ Get Game Instance Subsystem
   │  └─ Class: BP_PCGDungeonSubsystem
   │
   └─ Notify PCG Generation Complete
      ├─ In PCG: [PCG Component]
      └─ [게임 시작 로직]
```

## 🏷️ **PCG 그래프 태그 설정**

### **PCG_MultiFloorDungeon 그래프**

```
1. Actor Spawner 노드 선택
2. Details Panel → Actor Tags
3. 다음 태그 추가:
   ├─ PlayerSpawnPoint
   ├─ EnemySpawnPoint
   ├─ ObjectiveSpawnPoint (선택)
   └─ HazardSpawnPoint (선택)
```

## 🔄 **전체 플로우**

```
1. 레벨 로드
   ↓
2. BP_Dungeon → PCG 생성
   ↓
3. PCG 완료 → Event PCG Generation Complete
   ↓
4. BP_PCGDungeonSubsystem → Notify PCG Generation Complete
   ↓
5. CollectPCGGeneratedActors 호출 (Blueprint)
   ├─ ForEachManagedResource
   ├─ Cast to PCGManagedActors
   ├─ Get Generated Actors (Blueprint 노드)
   └─ Out Actors에 추가
   ↓
6. C++ 서브시스템 → 태그 필터링
   ├─ PlayerSpawnPoint → Extract 앵커
   └─ EnemySpawnPoint → EnemySpawn 앵커
   ↓
7. ProcessAnchors → 게임플레이 액터 스폰
   ├─ BP_ExtractionPortal
   └─ BP_EnemySpawner
```

## ⚠️ **중요 사항**

1. **Blueprint 구현 필수**
   - C++만으로는 PCG 생성 액터 접근 불가
   - `CollectPCGGeneratedActors` 이벤트를 반드시 Blueprint에서 구현해야 함

2. **PCG 태그 설정**
   - PCG 그래프의 Actor Spawner 노드에서 태그 설정
   - `PlayerSpawnPoint`, `EnemySpawnPoint` 등

3. **아웃라이너에 표시되지 않음**
   - PCG 런타임 생성 액터는 아웃라이너에 없음
   - `Get All Actors` 류의 함수로 검색 불가
   - 반드시 PCG 컴포넌트를 통해 접근

## 🎯 **결론**

UE 5.6에서는 PCG 생성 액터를 C++에서 직접 가져올 수 없으므로:

1. ✅ **Blueprint에서 `CollectPCGGeneratedActors` 구현**
2. ✅ **PCG 컴포넌트의 Blueprint 노드 활용**
3. ✅ **C++ 서브시스템은 Blueprint가 수집한 액터 처리**

이 방식이 UE 5.6에서 PCG 앵커시스템을 구현하는 **유일하고 권장되는 방법**입니다!


