// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_EDITOR

#include "AnchorEditorWidget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayTagContainer.h"
#include "GameplayTagAssetInterface.h"
#include "Editor/EditorEngine.h"
#include "Editor.h"
#include "Engine/Selection.h"

UAnchorEditorWidget::UAnchorEditorWidget()
{
	AllActors.Empty();
	ActorsByTag.Empty();
	ValidationResults.Empty();
}

void UAnchorEditorWidget::ScanLevel()
{
	UE_LOG(LogTemp, Log, TEXT("[AnchorEditorWidget] Scanning level..."));

	// Clear previous results
	AllActors.Empty();
	ActorsByTag.Empty();

	// Scan level
	ScanLevelInternal();

	UE_LOG(LogTemp, Log, TEXT("[AnchorEditorWidget] Level scan complete. Found %d actors"), AllActors.Num());

	// Notify Blueprint
	OnLevelScanned();
}

void UAnchorEditorWidget::AddTagToSelected(const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnchorEditorWidget] Invalid tag provided"));
		return;
	}

	// Get selected actors from editor
	TArray<AActor*> SelectedActors;
	if (GEditor)
	{
		USelection* Selection = GEditor->GetSelectedActors();
		if (Selection)
		{
			Selection->GetSelectedObjects<AActor>(SelectedActors);
		}
	}

	if (SelectedActors.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnchorEditorWidget] No actors selected"));
		return;
	}

	// Add tag to selected actors (참고: 실제 태그 저장은 별도 구현 필요)
	for (AActor* Actor : SelectedActors)
	{
		if (Actor && IsValid(Actor))
		{
			if (IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Actor))
			{
				FGameplayTagContainer ActorTags;
				TagInterface->GetOwnedGameplayTags(ActorTags);
				ActorTags.AddTag(Tag);

				UE_LOG(LogTemp, Log, TEXT("[AnchorEditorWidget] Added tag %s to actor %s"),
					*Tag.ToString(), *Actor->GetName());
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[AnchorEditorWidget] Added tag %s to %d actors"),
		*Tag.ToString(), SelectedActors.Num());
}

void UAnchorEditorWidget::RemoveTagFromSelected(const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnchorEditorWidget] Invalid tag provided"));
		return;
	}

	TArray<AActor*> SelectedActors;
	if (GEditor)
	{
		USelection* Selection = GEditor->GetSelectedActors();
		if (Selection)
		{
			Selection->GetSelectedObjects<AActor>(SelectedActors);
		}
	}

	if (SelectedActors.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnchorEditorWidget] No actors selected"));
		return;
	}

	for (AActor* Actor : SelectedActors)
	{
		if (Actor && IsValid(Actor))
		{
			if (IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Actor))
			{
				FGameplayTagContainer ActorTags;
				TagInterface->GetOwnedGameplayTags(ActorTags);
				ActorTags.RemoveTag(Tag);

				UE_LOG(LogTemp, Log, TEXT("[AnchorEditorWidget] Removed tag %s from actor %s"),
					*Tag.ToString(), *Actor->GetName());
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[AnchorEditorWidget] Removed tag %s from %d actors"),
		*Tag.ToString(), SelectedActors.Num());
}

void UAnchorEditorWidget::ClearTagsFromSelected()
{
	TArray<AActor*> SelectedActors;
	if (GEditor)
	{
		USelection* Selection = GEditor->GetSelectedActors();
		if (Selection)
		{
			Selection->GetSelectedObjects<AActor>(SelectedActors);
		}
	}

	if (SelectedActors.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnchorEditorWidget] No actors selected"));
		return;
	}

	for (AActor* Actor : SelectedActors)
	{
		if (Actor && IsValid(Actor))
		{
			if (IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Actor))
			{
				FGameplayTagContainer ActorTags;
				TagInterface->GetOwnedGameplayTags(ActorTags);
				ActorTags.Reset();

				UE_LOG(LogTemp, Log, TEXT("[AnchorEditorWidget] Cleared all tags from actor %s"),
					*Actor->GetName());
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[AnchorEditorWidget] Cleared all tags from %d actors"),
		SelectedActors.Num());
}

void UAnchorEditorWidget::ValidateAnchors()
{
	UE_LOG(LogTemp, Log, TEXT("[AnchorEditorWidget] Validating anchors..."));

	ValidationResults.Empty();
	ValidateAnchorsInternal();

	UE_LOG(LogTemp, Log, TEXT("[AnchorEditorWidget] Validation complete. %d issues found"),
		ValidationResults.Num());

	OnValidationComplete();
}

TArray<AActor*> UAnchorEditorWidget::GetAllActors() const
{
	TArray<AActor*> Result;
	Result.Reserve(AllActors.Num());
	for (AActor* Actor : AllActors)
	{
		Result.Add(Actor);
	}
	return Result;
}

TArray<AActor*> UAnchorEditorWidget::GetActorsWithTag(const FGameplayTag& Tag) const
{
	if (const FEditorTaggedActors* Found = ActorsByTag.Find(Tag))
	{
		TArray<AActor*> Result;
		Result.Reserve(Found->Actors.Num());
		for (AActor* Actor : Found->Actors)
		{
			Result.Add(Actor);
		}
		return Result;
	}
	return {};
}

void UAnchorEditorWidget::ScanLevelInternal()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnchorEditorWidget] No world found"));
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !IsValid(Actor))
		{
			continue;
		}

		AllActors.Add(Actor);

		if (IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Actor))
		{
			FGameplayTagContainer ActorTags;
			TagInterface->GetOwnedGameplayTags(ActorTags);

			if (ActorTags.Num() > 0)
			{
				for (const FGameplayTag& Tag : ActorTags)
				{
					FEditorTaggedActors& List = ActorsByTag.FindOrAdd(Tag);
					List.Actors.Add(Actor);
				}
			}
		}
	}
}

void UAnchorEditorWidget::ValidateAnchorsInternal()
{
	for (const auto& TagPair : ActorsByTag)
	{
		const FGameplayTag& Tag = TagPair.Key;
		const TArray<TObjectPtr<AActor>>& Actors = TagPair.Value.Actors;

		TSet<AActor*> UniqueActors;
		for (AActor* Actor : Actors)
		{
			if (UniqueActors.Contains(Actor))
			{
				ValidationResults.Add(FString::Printf(TEXT("Duplicate actor %s found with tag %s"),
					*Actor->GetName(), *Tag.ToString()));
			}
			else
			{
				UniqueActors.Add(Actor);
			}
		}

		for (AActor* Actor : Actors)
		{
			if (!Actor || !IsValid(Actor))
			{
				ValidationResults.Add(FString::Printf(TEXT("Invalid actor found with tag %s"),
					*Tag.ToString()));
			}
		}
	}

	int32 ActorsWithoutTags = 0;
	for (AActor* Actor : AllActors)
	{
		if (Actor && IsValid(Actor))
		{
			if (IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Actor))
			{
				FGameplayTagContainer ActorTags;
				TagInterface->GetOwnedGameplayTags(ActorTags);
				if (ActorTags.Num() == 0)
				{
					ActorsWithoutTags++;
				}
			}
		}
	}

	if (ActorsWithoutTags > 0)
	{
		ValidationResults.Add(FString::Printf(TEXT("%d actors found without any tags"), ActorsWithoutTags));
	}
}

#endif // WITH_EDITOR
