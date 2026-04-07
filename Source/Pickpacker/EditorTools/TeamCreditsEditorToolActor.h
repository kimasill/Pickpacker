// Editor tool actor for setting team credits during PIE
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TeamCreditsEditorToolActor.generated.h"

UCLASS(BlueprintType, Blueprintable)
class PICKPACKER_API ATeamCreditsEditorToolActor : public AActor
{
	GENERATED_BODY()

public:
	ATeamCreditsEditorToolActor();

	/** 에디터에서 버튼 클릭 시 팀 크레딧 적용 (PIE 중) */
	UFUNCTION(CallInEditor, Category = "Pickpacker|Credits")
	void ApplyTeamCredits();

	/** 설정할 팀 크레딧 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Credits", meta = (ClampMin = "0"))
	int32 TargetTeamCredits = 0;
};
