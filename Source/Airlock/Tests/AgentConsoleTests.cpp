// Copyright Airlock project. All Rights Reserved.

#include "Core/AgentConsoleSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirlockAgentConsolePathTest, "Airlock.Core.AgentConsoleFileLivesUnderSaved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAirlockAgentConsolePathTest::RunTest(const FString& Parameters)
{
	const FString Path = UAgentConsoleSubsystem::CommandFilePath();
	TestTrue(TEXT("under the project's Saved dir"), Path.StartsWith(FPaths::ProjectSavedDir()));
	TestTrue(TEXT("named AgentConsole/commands.txt"), Path.EndsWith(TEXT("AgentConsole/commands.txt")) || Path.EndsWith(TEXT("AgentConsole\\commands.txt")));
	return true;
}

#endif
