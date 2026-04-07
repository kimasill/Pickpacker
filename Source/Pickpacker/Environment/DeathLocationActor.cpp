// Fill out your copyright notice in the Description page of Project Settings.

#include "Environment/DeathLocationActor.h"

#include "Components/SceneComponent.h"

ADeathLocationActor::ADeathLocationActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}
