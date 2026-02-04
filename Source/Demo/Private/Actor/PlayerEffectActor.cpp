

#include "Actor/PlayerEffectActor.h"
#include "Components/SphereComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/PlayerAttributeSet.h"



APlayerEffectActor::APlayerEffectActor()
{
 	
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->SetupAttachment(GetRootComponent());
}

void APlayerEffectActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(OtherActor))
	{
		const UPlayerAttributeSet* PlayerAttributeSet = Cast<UPlayerAttributeSet>(ASCInterface->GetAbilitySystemComponent()->GetAttributeSet(UPlayerAttributeSet::StaticClass()));
		
		UPlayerAttributeSet* MutablePlayerAttributeSet = const_cast<UPlayerAttributeSet*>(PlayerAttributeSet);
		MutablePlayerAttributeSet->SetHealth(PlayerAttributeSet->GetHealth() + 25.f);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("³ÔµôÑªÆ¿£¡ÑªÁ¿ +25"));
		}
		Destroy();
	}
}

void APlayerEffectActor::EndOverLap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
}

void APlayerEffectActor::BeginPlay()
{
	Super::BeginPlay();
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &APlayerEffectActor::OnOverlapBegin);
	Sphere->OnComponentEndOverlap.AddDynamic(this, &APlayerEffectActor::EndOverLap);
}




