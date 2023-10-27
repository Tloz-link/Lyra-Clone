// Copyright Epic Games, Inc. All Rights Reserved.

#include "Una.h"
#include "Modules/ModuleManager.h"
//#include "Una/System/UnaAssetManager.h"

/**
 * FUnaGameModule
 */
class FUnaGameModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

void FUnaGameModule::StartupModule()
{

}

void FUnaGameModule::ShutdownModule()
{

}

IMPLEMENT_PRIMARY_GAME_MODULE(FUnaGameModule, Una, "Una");
