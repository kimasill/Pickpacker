
#include "BTTask_MotherExecutePunishment.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/AI/MotherAIController.h"
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

	MotherAI->OnPunishmentFinished.AddDynamic(this, &UBTTask_MotherExecutePunishment::OnPunishmentFinished);
	
	MotherAI->ExecutePunishment(TargetPlayer);

	return EBTNodeResult::InProgress;
}

void UBTTask_MotherExecutePunishment::OnPunishmentFinished(bool interrupted)
{
	if (CachedOwnerComp)
	{
		FinishLatentTask(*CachedOwnerComp, interrupted ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
	}

	if(AMotherAIActor* MotherAI = Cast<AMotherAIActor>(CachedOwnerComp->GetAIOwner()->GetPawn()))
	{
		MotherAI->OnPunishmentFinished.RemoveDynamic(this, &UBTTask_MotherExecutePunishment::OnPunishmentFinished);
	}
	
}



