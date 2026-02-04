

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerEffectActor.generated.h"

class USphereComponent;

UCLASS()
class DEMO_API APlayerEffectActor : public AActor
{
	GENERATED_BODY()
	
public:	
	
	APlayerEffectActor();

	UFUNCTION()
	virtual void OnOverlapBegin( UPrimitiveComponent* OverlappedComp, AActor* OtherActor,  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
	UFUNCTION()
	virtual	void EndOverLap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

protected:

	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent>Sphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent>Mesh;
};
