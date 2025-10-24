# 🐛 PCG 플레이어 스폰 문제 해결 가이드

## ⚠️ **문제 상황**

`ServerIncrementClientReady_Implementation()`의 `HasAuthority()` 체크를 통과하지 못함

## 🔍 **문제 원인 및 해결**

### **원인 1: 서버 단독 실행 시 무한 대기**

```cpp
// 문제 코드
if (Expected > 0 && ClientsPCGReadyCount >= Expected)
{
    // Expected == 0 (클라이언트 없음) 일 때 이 블록 실행 안 됨!
}
```

**해결:**
```cpp
// 수정된 코드
if (Expected == 0 || ClientsPCGReadyCount >= Expected)
{
    if (Expected == 0)
    {
        // 클라이언트가 없으면 즉시 진행
        ServerFinalizePCG();
    }
}
```

### **원인 2: RPC 호출 실패**

클라이언트에서 `GameState->ServerIncrementClientReady()` 호출이 서버로 전달되지 않음

**체크 포인트:**

1. **GameState 복제 확인**
   ```cpp
   // PickpackerGameState.cpp
   DOREPLIFETIME(APickpackerGameState, PCGRunCounter);
   DOREPLIFETIME(APickpackerGameState, ClientsPCGReadyCount);
   ```

2. **GameMode 설정**
   ```
   BP_PickpackerGameMode:
   └─ Game State Class: APickpackerGameState
   ```

3. **RPC 선언 확인**
   ```cpp
   // PickpackerGameState.h
   UFUNCTION(Server, Reliable)
   void ServerIncrementClientReady();
   
   // PickpackerGameState.cpp
   void APickpackerGameState::ServerIncrementClientReady_Implementation()
   {
       // 구현
   }
   ```

## 📊 **디버깅 체크리스트**

### **1. 서버 단독 실행 테스트**

에디터에서 Play → Standalone Game

**예상 로그:**
```
[PCGDungeonSubSystem] Server PCG complete - no clients, proceeding immediately
[PCGDungeonSubSystem] Server finalize - all clients ready, processing anchors
[PCGDungeonSubSystem] Generated N anchors from spawn points
[PCGDungeonSubSystem] Server finalize completed - gameplay spawn ready, players can spawn now
```

### **2. 리슨 서버 + 클라이언트 테스트**

에디터에서 Play → Number of Players: 2

**서버 로그:**
```
[PCGDungeonSubSystem] Server PCG complete - waiting for 1 clients
[PickpackerGameState] Triggering client PCG run (RunCounter=1)
[PickpackerGameState] Client PCG ready 1/1 (Authority: Yes, NetMode: 1)
[PickpackerGameState] All clients ready - finalizing on server
[PCGDungeonSubSystem] Server finalize completed - gameplay spawn ready, players can spawn now
```

**클라이언트 로그:**
```
[PickpackerGameState] Client PCG triggered by server (RunCounter=1)
[PCGDungeonSubSystem] MarkClientPCGReady called - NetMode: 3
[PCGDungeonSubSystem] Client PCG ready - calling ServerIncrementClientReady RPC
```

### **3. RPC가 호출되지 않는 경우**

**확인 사항:**

```cpp
// 1. GameState가 null인가?
APickpackerGameState* GameState = World->GetGameState<APickpackerGameState>();
if (!GameState)
{
    // 문제: GameMode에서 GameState 클래스가 제대로 설정되지 않음
}

// 2. HasAuthority()가 false인가?
UE_LOG(LogTemp, Log, TEXT("GameState Authority: %s"), GameState->HasAuthority() ? TEXT("Yes") : TEXT("No"));
// GameState는 항상 Authority를 가져야 함

// 3. RPC 선언이 올바른가?
UFUNCTION(Server, Reliable) // ✅
UFUNCTION(Server) // ❌ Reliable 누락
```

## ✅ **해결 방법 정리**

### **수정 1: 서버 단독 실행 지원**
```cpp
// NotifyPCGGenerationComplete()
const int32 ExpectedClients = GS->GetExpectedClientCount();
if (ExpectedClients == 0)
{
    // 즉시 ServerFinalizePCG() 호출
    ServerFinalizePCG();
}
```

### **수정 2: 클라이언트 준비 조건 수정**
```cpp
// ServerIncrementClientReady_Implementation()
if (Expected == 0 || ClientsPCGReadyCount >= Expected)
{
    // Expected == 0: 서버 단독
    // ClientsPCGReadyCount >= Expected: 모든 클라이언트 준비 완료
    ServerFinalizePCG();
}
```

### **수정 3: 상세 로깅 추가**
```cpp
// 모든 단계에서 NetMode와 Authority 로깅
UE_LOG(LogTemp, Log, TEXT("NetMode: %d, Authority: %s"), 
    (int32)GetWorld()->GetNetMode(),
    HasAuthority() ? TEXT("Yes") : TEXT("No"));
```

## 🎮 **언리얼 엔진 설정 확인**

### **Project Settings → Maps & Modes**
```
Default GameMode: BP_PickpackerGameMode
  ├─ Game State Class: BP_PickpackerGameState (parent: APickpackerGameState)
  └─ Player Controller Class: BP_PickpackerPlayerController (parent: APickpackerPlayerController)
```

### **World Settings (레벨 열고)**
```
Selected GameMode: BP_PickpackerGameMode
```

## 🔧 **최종 플로우**

```
1. 서버 PCG 생성 완료
   ↓
2. 스폰 포인트 수집
   ↓
3. ExpectedClients 체크
   ├─ 0개 → 즉시 ServerFinalizePCG() ✅
   └─ N개 → TriggerClientPCGRun()
              ↓
           클라이언트 PCG 생성
              ↓
           ServerIncrementClientReady()
              ↓
           모두 준비 → ServerFinalizePCG() ✅
```

이제 서버 단독 실행과 멀티플레이어 모두 올바르게 작동할 것입니다! 🎉
