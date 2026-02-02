

#include "Character/CharacterBase.h"


ACharacterBase::ACharacterBase()
{
 	
	PrimaryActorTick.bCanEverTick = false;

	//创建武器骨骼网络
	Weapon = CreateDefaultSubobject<USkeletalMeshComponent>("Weapon");
	//将武器附加到角色的手部插槽上
	Weapon->SetupAttachment(GetMesh(), "WeaponHandSocket");
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);

}

UAbilitySystemComponent* ACharacterBase::GetAbilitySystemComponent() const
{

		return AbilitySystemComponent;

}

void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
}



