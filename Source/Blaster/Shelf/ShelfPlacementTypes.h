#pragma once

#include "CoreMinimal.h"
#include "ShelfPlacementTypes.generated.h"

USTRUCT(BlueprintType)
struct BLASTER_API FShelfPlacementPreview
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Shelf")
    bool bHasSurface = false;

    UPROPERTY(BlueprintReadOnly, Category = "Shelf")
    bool bWithinSurfaceBounds = false;

    UPROPERTY(BlueprintReadOnly, Category = "Shelf")
    bool bSurfaceAngleValid = false;

    UPROPERTY(BlueprintReadOnly, Category = "Shelf")
    bool bHitShelf = false;

    UPROPERTY(BlueprintReadOnly, Category = "Shelf")
    bool bHasCollision = false;

    UPROPERTY(BlueprintReadOnly, Category = "Shelf")
    bool bIsPlaceable = false;

    UPROPERTY(BlueprintReadOnly, Category = "Shelf")
    FVector ImpactPoint = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Shelf")
    FVector SurfaceNormal = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly, Category = "Shelf")
    FTransform WorldTransform = FTransform::Identity;

    UPROPERTY(BlueprintReadOnly, Category = "Shelf")
    FVector HalfExtent = FVector::ZeroVector;

    void Reset()
    {
        bHasSurface = false;
        bWithinSurfaceBounds = false;
        bSurfaceAngleValid = false;
        bHitShelf = false;
        bHasCollision = false;
        bIsPlaceable = false;
        ImpactPoint = FVector::ZeroVector;
        SurfaceNormal = FVector::UpVector;
        WorldTransform = FTransform::Identity;
        HalfExtent = FVector::ZeroVector;
    }
};




