// Fill out your copyright notice in the Description page of Project Settings.


#include "ElimAnnouncement.h"
#include "Components/TextBlock.h"

void UElimAnnouncement::SetElimAnnouncement(const FString Attacker, const FString Victim)
{
	FString ElimAnnouncementText = FString::Printf(TEXT("%s eliminated %s"), *Attacker, *Victim);

	if (AnnouncementText)
	{
			AnnouncementText->SetText(FText::FromString(ElimAnnouncementText));
	}
}
