// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "GameplayTagContainer.h"
#include "AnchorEditorWidget.generated.h"

class AActor;
class UWorld;

/** Wrapper for actor arrays to be used as TMap values in UPROPERTY */
USTRUCT(BlueprintType)
struct FEditorTaggedActors
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<AActor>> Actors;
};

/**
 * Anchor Editor Widget - Tool for managing anchors in the level
 * Allows bulk tag assignment and validation
 */
UCLASS(BlueprintType)
class BLASTER_API UAnchorEditorWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:
	UAnchorEditorWidget();

	/**
	 * Scan the current level for all actors
	 */
	UFUNCTION(BlueprintCallable, Category = "Anchor Editor")
	void ScanLevel();

	/**
	 * Add a tag to selected actors
	 */
	UFUNCTION(BlueprintCallable, Category = "Anchor Editor")
	void AddTagToSelected(const FGameplayTag& Tag);

	/**
	 * Remove a tag from selected actors
	 */
	UFUNCTION(BlueprintCallable, Category = "Anchor Editor")
	void RemoveTagFromSelected(const FGameplayTag& Tag);

	/**
	 * Clear all tags from selected actors
	 */
	UFUNCTION(BlueprintCallable, Category = "Anchor Editor")
	void ClearTagsFromSelected();

	/**
	 * Validate all anchors in the level
	 */
	UFUNCTION(BlueprintCallable, Category = "Anchor Editor")
	void ValidateAnchors();

	/**
	 * Get all actors in the level
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Anchor Editor")
	TArray<AActor*> GetAllActors() const;

	/**
	 * Get actors with specific tag
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Anchor Editor")
	TArray<AActor*> GetActorsWithTag(const FGameplayTag& Tag) const;

	/**
	 * Get validation results
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Anchor Editor")
	TArray<FString> GetValidationResults() const { return ValidationResults; }

protected:
	/**
	 * Called when level scan is complete
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Anchor Editor")
	void OnLevelScanned();

	/**
	 * Called when validation is complete
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Anchor Editor")
	void OnValidationComplete();

private:
	/** All actors found in the level */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> AllActors;

	/** Actors grouped by tag */
	UPROPERTY()
	TMap<FGameplayTag, FEditorTaggedActors> ActorsByTag;

	/** Validation results */
	UPROPERTY()
	TArray<FString> ValidationResults;

	/**
	 * Internal function to scan level for actors
	 */
	void ScanLevelInternal();

	/**
	 * Internal function to validate anchors
	 */
	void ValidateAnchorsInternal();
};
