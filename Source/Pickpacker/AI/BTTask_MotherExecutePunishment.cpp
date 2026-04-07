
#include "BTTask_MotherExecutePunishment.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "AI/MotherAIActor.h"
#include "AI/MotherAIController.h"
#include "GameFramework/Character.h"

UBTTask_MotherExecutePunishment::UBTTask_MotherExecutePunishment()
{
	NodeName = TEXT("Mother Execute Punishment");
}

EBTNodeResult::Type UBTTask_MotherExecutePunishment::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	CachedOwnerComp = &OwnerComp;
	
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	AMotherAIActor* MotherAI = Cast<AMotherAIActor>(AIController->GetPawn());
	if (!MotherAI)
	{
		return EBTNodeResult::Failed;
	}

	// Get target player from blackboard
	ACharacter* TargetPlayer = Cast<ACharacter>(BlackboardComp->GetValueAsObject(TargetPlayerKey.SelectedKeyName));
	if (!TargetPlayer)
	{
		return EBTNodeResult::Failed;
	}
	if (CachedMotherAI)
	{
		CachedMotherAI->OnPunishmentFinished.RemoveDynamic(this, &UBTTask_MotherExecutePunishment::OnPunishmentFinished);
	}

	// Mother AI ���� ����
	CachedMotherAI = MotherAI;

	MotherAI->OnPunishmentFinished.AddDynamic(this, &UBTTask_MotherExecutePunishment::OnPunishmentFinished);
	
	MotherAI->ExecutePunishment(TargetPlayer);

	return EBTNodeResult::InProgress;
}

void UBTTask_MotherExecutePunishment::OnPunishmentFinished(bool interrupted)
{
	UE_LOG(LogTemp, Log, TEXT("[BTTask_MotherExecutePunishment] OnPunishmentFinished called, interrupted: %d"), interrupted);

	if (CachedOwnerComp)
	{
		FinishLatentTask(*CachedOwnerComp, interrupted ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
	}

	// �̺�Ʈ ����
	if (CachedMotherAI)
	{
		CachedMotherAI->OnPunishmentFinished.RemoveDynamic(this, &UBTTask_MotherExecutePunishment::OnPunishmentFinished);
		CachedMotherAI = nullptr;
	}

	CachedOwnerComp = nullptr;
	
}



