#include "UnaAssetManager.h"
#include "Una/UnaLogChannels.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(UnaAssetManager)


UUnaAssetManager::UUnaAssetManager()
	: Super()
{

}

UUnaAssetManager& UUnaAssetManager::Get()
{
	check(GEngine);

	if (UUnaAssetManager* Singleton = Cast<UUnaAssetManager>(GEngine->AssetManager))
	{
		return *Singleton;
	}

	UE_LOG(LogUna, Fatal, TEXT("invaild AssetManagerClassname in DefaultEnine.ini(project settings); it must be UnaAssetManager."));

	// Fatal로 인해 크래시가 나서 도달하지 않지만 컴파일을 위해 리턴
	return *NewObject<UUnaAssetManager>();
}

void UUnaAssetManager::AddLoadedAsset(const UObject* Asset)
{
	if (ensureAlways(Asset))
	{
		FScopeLock Lock(&SyncObject);
		LoadedAssets.Add(Asset);
	}
}

bool UUnaAssetManager::ShouldLogAssetLoads()
{
	static bool bLogAssetLoads = FParse::Param(FCommandLine::Get(), TEXT("LogAssetLoads"));
    return bLogAssetLoads;
}

UObject* UUnaAssetManager::SynchronousLoadAsset(const FSoftObjectPath& AssetPath)
{
	if (AssetPath.IsValid())
	{
		TUniquePtr<FScopeLogTime> LogTimePtr;
		if (ShouldLogAssetLoads())
		{
			LogTimePtr = MakeUnique<FScopeLogTime>(*FString::Printf(TEXT("synchronous loaded assets [%s]"), *AssetPath.ToString()), nullptr, FScopeLogTime::ScopeLog_Seconds);
		}

		if (UAssetManager::IsValid())
		{
			return UAssetManager::GetStreamableManager().LoadSynchronous(AssetPath);
		}

		return AssetPath.TryLoad();
	}

	return nullptr;
}

void UUnaAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	UE_LOG(LogUna, Display, TEXT("UUnaAssetManager::StartInitialLoading"));
}
