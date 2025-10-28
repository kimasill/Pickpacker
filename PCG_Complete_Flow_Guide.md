# 🎯 PCG 생성 및 플레이어 스폰 완전한 플로우

## 🔄 **전체 흐름도**

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. 게임 시작 (매치 시작)                                          │
└─────────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│ 2. 서버: PCG 던전 생성                                            │
│    - APickpackerGameMode::HandleMatchStart()                     │
│    - PCGDungeonSubSystem->GenerateDungeon(SeedSet)               │
│    - BP_Dungeon의 PCG 컴포넌트 실행                               │
└─────────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│ 3. 서버: PCG 생성 완료                                            │
│    - BP_Dungeon: Event PCG Generation Complete                   │
│    - NotifyPCGGenerationComplete(PCGComponent) 호출               │
│    - 스폰 포인트 탐색 (GetAllActorsWithTag)                       │
│      ├─ PlayerSpawnPoint                                         │
│      ├─ EnemySpawnPoint                                          │
│      ├─ ObjectiveSpawnPoint                                      │
│      └─ HazardSpawnPoint                                         │
└─────────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│ 4. 서버 → 클라이언트: PCG 생성 시작 신호                          │
│    - PickpackerGameState->TriggerClientPCGRun()                  │
│    - PCGRunCounter++ (복제)                                      │
└─────────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│ 5. 클라이언트: PCG 생성 시작                                      │
│    - OnRep_PCGRunCounter() 호출                                  │
│    - PCGDungeonSubSystem->GenerateDungeon(SeedSet)               │
│    - 동일한 시드로 동일한 던전 생성                                │
└─────────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│ 6. 클라이언트: PCG 생성 완료                                      │
│    - BP_Dungeon: Event PCG Generation Complete                   │
│    - NotifyPCGGenerationComplete(PCGComponent) 호출               │
│    - 스폰 포인트 탐색 (서버와 동일)                                │
│    - MarkClientPCGReady() 호출                                   │
└─────────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│ 7. 클라이언트 → 서버: 준비 완료 보고                              │
│    - PickpackerGameState->ServerIncrementClientReady()           │
│    - ClientsPCGReadyCount++                                      │
└─────────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│ 8. 서버: 모든 클라이언트 준비 완료 체크                           │
│    - if (ClientsPCGReadyCount >= GetExpectedClientCount())       │
│    - OnAllClientsPCGReady.Broadcast()                            │
└─────────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│ 9. 서버: 게임플레이 요소 스폰                                     │
│    - PCGDungeonSubSystem->ServerFinalizePCG()                    │
│    - GeneratePCGAnchors(SeedSet)                                 │
│    - ProcessAnchors(GameplayAnchors)                             │
│      ├─ BP_ExtractionPortal 스폰 (PlayerSpawnPoint)              │
│      ├─ BP_EnemySpawner 스폰 (EnemySpawnPoint)                   │
│      ├─ BP_ParcelActor 스폰 (ObjectiveSpawnPoint)                │
│      └─ BP_ElectricFloorHazard 스폰 (HazardSpawnPoint)           │
└─────────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│ 10. 플레이어 스폰 가능                                            │
│     - GameMode: 플레이어들을 PlayerSpawnPoint에 스폰              │
│     - 게임 시작                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 🔍 **현재 문제 진단 및 해결**

### **문제점**
```cpp
// MarkClientPCGReady()에서 캐스팅 실패
if (APickpackerPlayerController* PPC = Cast<APickpackerPlayerController>(PC))
{
    PPC->ServerReportPCGReady(); // 여기까지 도달하지 못함
}
```

### **해결 방법 1: PlayerController 캐스팅 우회**
```cpp
// GameState를 직접 사용
void UPCGDungeonSubSystem::MarkClientPCGReady()
{
    if (APickpackerGameState* GameState = World->GetGameState<APickpackerGameState>())
    {
        GameState->ServerIncrementClientReady(); // ✅ 직접 호출
    }
}
```

### **해결 방법 2: 게임 모드에서 PlayerController 클래스 설정**
```
BP_PickpackerGameMode:
├─ Player Controller Class: BP_PickpackerPlayerController
└─ 이렇게 하면 APickpackerPlayerController로 캐스팅 성공
```

## 📝 **코드 요약**

### **1. 서버 PCG 생성**
```cpp
// APickpackerGameMode::HandleMatchStart()
PCGDungeonSubSystem->GenerateDungeon(SeedSet);
  ↓
// BP_Dungeon: PCG 생성 완료
NotifyPCGGenerationComplete(PCGComponent)
  ↓
// 서버: 스폰 포인트 수집
GetAllActorsWithTag("PlayerSpawnPoint", PlayerSpawnPoints);
GetAllActorsWithTag("EnemySpawnPoint", EnemySpawnPoints);
  ↓
// 클라이언트에게 PCG 생성 시작 신호
PickpackerGameState->TriggerClientPCGRun();
```

### **2. 클라이언트 PCG 생성**
```cpp
// PickpackerGameState::OnRep_PCGRunCounter()
PCGDungeonSubSystem->GenerateDungeon(SeedSet); // 동일 시드
  ↓
// BP_Dungeon: PCG 생성 완료
NotifyPCGGenerationComplete(PCGComponent)
  ↓
// 클라이언트: 스폰 포인트 수집 (서버와 동일)
GetAllActorsWithTag("PlayerSpawnPoint", PlayerSpawnPoints);
  ↓
// 서버에 준비 완료 보고
MarkClientPCGReady()
  ↓
GameState->ServerIncrementClientReady();
```

### **3. 서버 최종 처리**
```cpp
// PickpackerGameState::ServerIncrementClientReady()
ClientsPCGReadyCount++;
if (ClientsPCGReadyCount >= GetExpectedClientCount())
{
    // 모든 클라이언트 준비 완료
    PCGDungeonSubSystem->ServerFinalizePCG();
      ↓
    // 앵커 생성 및 게임플레이 액터 스폰
    GeneratePCGAnchors(SeedSet);
    ProcessAnchors(GameplayAnchors);
      ↓
    // 플레이어 스폰 가능
}
```

## 🎮 **언리얼 엔진 설정**

### **1. GameMode 설정**
```
BP_PickpackerGameMode:
├─ Player Controller Class: APickpackerPlayerController
│  (또는 BP_PickpackerPlayerController)
└─ Game State Class: BP_PickpackerGameState
```

### **2. BP_Dungeon 이벤트 그래프**
```
Event PCG Generation Complete
├─ Get Game Instance Subsystem (PCGDungeonSubSystem)
└─ Notify PCG Generation Complete
   └─ In PCG: [Self → PCG Component]
```

### **3. PCG 그래프 태그 설정**
```
PCG_MultiFloorDungeon:
├─ Actor Spawner (PlayerSpawn)
│  └─ Actor Tags: "PlayerSpawnPoint"
└─ Actor Spawner (EnemySpawn)
   └─ Actor Tags: "EnemySpawnPoint"
```

## ⚠️ **중요 체크리스트**

- [x] `APickpackerPlayerController` 생성
- [x] `MarkClientPCGReady()` 수정 (GameState 직접 사용)
- [x] `OnClientPCGReady()` 메서드 추가
- [x] `ServerFinalizePCG()` 스폰 로직 구현
- [ ] GameMode에서 PlayerController 클래스 설정
- [ ] PCG 그래프에 태그 설정
- [ ] 데이터 테이블 생성

## 🐛 **디버깅 로그 확인**

플로우가 올바르게 작동하는지 확인하려면 다음 로그를 확인하세요:

```
[PCGDungeonSubSystem] Server PCG complete - waiting for clients
[PickpackerGameState] Triggering client PCG run (RunCounter=1)
[PickpackerGameState] Client PCG triggered by server (RunCounter=1)
[PCGDungeonSubSystem] Client PCG complete - reporting to server
[PickpackerGameState] Client PCG ready 1/2
[PickpackerGameState] Client PCG ready 2/2
[PickpackerGameState] All clients ready - finalizing on server
[PCGDungeonSubSystem] Server finalize - all clients ready, processing anchors
[PCGDungeonSubSystem] Server finalize completed - gameplay spawn ready, players can spawn now
```

이제 전체 플로우가 수정되었으며, PlayerController 캐스팅 문제도 해결되었습니다! 🎉

