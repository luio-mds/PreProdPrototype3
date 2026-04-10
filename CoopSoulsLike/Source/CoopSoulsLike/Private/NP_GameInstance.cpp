// Fill out your copyright notice in the Description page of Project Settings.


#include "NP_GameInstance.h"

#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

const FName UNP_GameInstance::SERVER_NAME_KEY(TEXT("SERVER_NAME_KEY"));
const FName UNP_GameInstance::MATCH_TYPE_KEY(TEXT("MATCH_TYPE_KEY"));

UNP_GameInstance::UNP_GameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OnCreateSessionCompleteDelegate =
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UNP_GameInstance::NP_OnCreateSessionComplete);

	OnStartSessionCompleteDelegate =
		FOnStartSessionCompleteDelegate::CreateUObject(this, &UNP_GameInstance::NP_OnStartOnlineGameComplete);

	OnFindSessionsCompleteDelegate =
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UNP_GameInstance::NP_OnFindSessionsComplete);

	OnJoinSessionCompleteDelegate =
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UNP_GameInstance::NP_OnJoinSessionComplete);

	OnDestroySessionCompleteDelegate =
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UNP_GameInstance::NP_OnDestroySessionComplete);
}

void UNP_GameInstance::Init()
{
	Super::Init();

	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UNP_GameInstance::NetworkFailureHappened);
	}

	if (IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get())
	{
		UE_LOG(LogTemp, Log, TEXT("OnlineSubsystem: %s"), *OnlineSub->GetSubsystemName().ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No OnlineSubsystem found"));
	}
}

void UNP_GameInstance::BeginDestroy()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
	}

	Super::BeginDestroy();
}

IOnlineSessionPtr UNP_GameInstance::GetSessionInterface() const
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (!OnlineSub)
	{
		return nullptr;
	}

	return OnlineSub->GetSessionInterface();
}

TSharedPtr<const FUniqueNetId> UNP_GameInstance::GetPrimaryNetId() const
{
	const ULocalPlayer* LocalPlayer = GetFirstGamePlayer();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	const FUniqueNetIdRepl NetId = LocalPlayer->GetPreferredUniqueNetId();
	if (!NetId.IsValid())
	{
		return nullptr;
	}

	return NetId.GetUniqueNetId();
}

void UNP_GameInstance::NetworkFailureHappened(
	UWorld* World,
	UNetDriver* NetDriver,
	ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	UE_LOG(LogTemp, Warning, TEXT("NetworkFailure Type=%d Error=%s"),
		(int32)FailureType, *ErrorString);

	if (NetDriver)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failure NetDriver Class = %s"),
			*NetDriver->GetClass()->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failure NetDriver is null"));
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			10.f,
			FColor::Red,
			FString::Printf(TEXT("FailureType=%d Error=%s"), (int32)FailureType, *ErrorString));
	}

	DestroySessionAndLeaveGame();
}

bool UNP_GameInstance::HostSession(
	TSharedPtr<const FUniqueNetId> UserId,
	FName SessionName,
	bool bInIsLAN,
	bool bIsPresence,
	int32 MaxNumberPlayers,
	const FString& InSearchKeyword)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !UserId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("HostSession failed: invalid session interface or user id"));
		return false;
	}

	if (Sessions->GetNamedSession(SessionName) != nullptr)
	{
		bPendingCreateAfterDestroy = true;
		PendingSessionName = SessionName;
		PendingMaxPlayers = MaxNumberPlayers;
		PendingSearchKeyword = InSearchKeyword;
		DestroySessionAndLeaveGame();
		return true;
	}

	SessionSettings = MakeShared<FOnlineSessionSettings>();
	SessionSettings->bIsLANMatch = bInIsLAN;
	SessionSettings->bUsesPresence = bIsPresence;
	SessionSettings->NumPublicConnections = MaxNumberPlayers;
	SessionSettings->NumPrivateConnections = 0;
	SessionSettings->bAllowInvites = true;
	SessionSettings->bAllowJoinInProgress = true;
	SessionSettings->bShouldAdvertise = true;
	SessionSettings->bAllowJoinViaPresence = true;
	SessionSettings->bAllowJoinViaPresenceFriendsOnly = false;
	SessionSettings->bUseLobbiesIfAvailable = true;

	SessionSettings->Set(SETTING_MAPNAME, FString(TEXT("Lvl_NetworkTest")), EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings->Set(SEARCH_KEYWORDS, InSearchKeyword, EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings->Set(MATCH_TYPE_KEY, FString(TEXT("CoopSoulsLike")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	const FString ServerName = CustomServerName.IsEmpty() ? TEXT("UnnamedServer") : CustomServerName;
	SessionSettings->Set(SERVER_NAME_KEY, ServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	OnCreateSessionCompleteDelegateHandle =
		Sessions->AddOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegate);

	const bool bStarted = Sessions->CreateSession(*UserId, SessionName, *SessionSettings);

	if (!bStarted)
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
		UE_LOG(LogTemp, Warning, TEXT("CreateSession did not start"));
	}

	return bStarted;
}

bool UNP_GameInstance::FindSessions(
	TSharedPtr<const FUniqueNetId> UserId,
	bool bInIsLAN,
	bool bIsPresence,
	const FString& InSearchKeyword)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !UserId.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("Online subsystem not found or invalid user id"));
		}
		NP_OnFindSessionsComplete(false);
		return false;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->bIsLanQuery = bInIsLAN;
	SessionSearch->MaxSearchResults = 100;
	SessionSearch->PingBucketSize = 50;

	// Steam lobby search
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(SEARCH_KEYWORDS, InSearchKeyword, EOnlineComparisonOp::Equals);

	(void)bIsPresence;

	FoundServers.Empty();

	OnFindSessionsCompleteDelegateHandle =
		Sessions->AddOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegate);

	const bool bStarted = Sessions->FindSessions(*UserId, SessionSearch.ToSharedRef());

	if (!bStarted)
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegateHandle);
		UE_LOG(LogTemp, Warning, TEXT("FindSessions did not start"));
		NP_OnFindSessionsComplete(false);
	}

	return bStarted;
}

bool UNP_GameInstance::JoinSession(
	TSharedPtr<const FUniqueNetId> UserId,
	FName SessionName,
	const FOnlineSessionSearchResult& SearchResult)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !UserId.IsValid())
	{
		return false;
	}

	OnJoinSessionCompleteDelegateHandle =
		Sessions->AddOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegate);

	const bool bStarted = Sessions->JoinSession(*UserId, SessionName, SearchResult);

	if (!bStarted)
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegateHandle);
		UE_LOG(LogTemp, Warning, TEXT("JoinSession did not start"));
	}

	return bStarted;
}

void UNP_GameInstance::StartOnlineGame()
{
	TSharedPtr<const FUniqueNetId> UserId = GetPrimaryNetId();
	if (!UserId.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Invalid UniqueNetId"));
		}
		return;
	}

	HostSession(UserId, NAME_GameSession, bIsLAN, true, 8, SearchKeyword);
}

void UNP_GameInstance::FindOnlineGame()
{
	TSharedPtr<const FUniqueNetId> UserId = GetPrimaryNetId();
	if (!UserId.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Invalid UniqueNetId"));
		}
		return;
	}

	if (IOnlineSubsystem* OSS = IOnlineSubsystem::Get())
	{
		const FString SubName = OSS->GetSubsystemName().ToString();
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Using Subsystem: %s"), *SubName));
		}
	}

	FindSessions(UserId, bIsLAN, true, SearchKeyword);
}

void UNP_GameInstance::JoinOnlineGame(bool& _condition)
{
	_condition = JoinSessionByIndex(0);
}

bool UNP_GameInstance::JoinSessionByIndex(int32 SessionIndex)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			5.f,
			FColor::Green,
			FString::Printf(TEXT("UI Index: %d | Search Index: %d"),
				SessionIndex,
				FoundServers[SessionIndex].SessionIndex));
	}

	if (!SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(FoundServers[SessionIndex].SessionIndex))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Session search invalid or result not found."));
		}
		return false;
	}

	TSharedPtr<const FUniqueNetId> UserId = GetPrimaryNetId();
	IOnlineSessionPtr Sessions = GetSessionInterface();

	if (!Sessions.IsValid() || !UserId.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("session not valid or user id not valid."));
		}
		return false;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("delegate handle added."));
	}

	OnJoinSessionCompleteDelegateHandle =
		Sessions->AddOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegate);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Joining session..."));
	}

	return Sessions->JoinSession(*UserId, NAME_GameSession, SessionSearch->SearchResults[FoundServers[SessionIndex].SessionIndex]);
}

void UNP_GameInstance::DestroySessionAndLeaveGame()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}

	OnDestroySessionCompleteDelegateHandle =
		Sessions->AddOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegate);

	const bool bStarted = Sessions->DestroySession(NAME_GameSession);
	if (!bStarted)
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateHandle);
		UE_LOG(LogTemp, Warning, TEXT("DestroySession did not start"));
	}
}

void UNP_GameInstance::NP_OnCreateSessionComplete(FName SessionName, bool bSuccess)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}

	Sessions->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);

	if (bSuccess)
	{
		OnStartSessionCompleteDelegateHandle =
			Sessions->AddOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegate);

		Sessions->StartSession(SessionName);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
			bSuccess ? TEXT("Session created successfully") : TEXT("Session creation failed"));
	}
}

void UNP_GameInstance::NP_OnStartOnlineGameComplete(FName SessionName, bool bSuccess)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegateHandle);
	}

	if (bSuccess)
	{
		OnSessionStarted();

		/*FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(
			TimerHandle,
			[this]()
			{
				FString LevelOptions = TEXT("?listen");
				if (SessionSettings.IsValid() && SessionSettings->bIsLANMatch)
				{
					LevelOptions.Append(TEXT("?bIsLanMatch=1"));
				}

				UGameplayStatics::OpenLevel(GetWorld(), TEXT("Lvl_NetworkTest"), true, LevelOptions);
			},
			1.0f,
			false);*/
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
			bSuccess ? TEXT("Session started successfully.") : TEXT("Failed to start session."));
	}
}

void UNP_GameInstance::NP_OnFindSessionsComplete(bool bSuccess)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}

	Sessions->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegateHandle);

	FoundServers.Empty();

	if (!bSuccess)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("FindSessions failed"));
		}
		OnSessionsUpdated();
		return;
	}

	if (!SessionSearch.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("Search invalid"));
		}
		OnSessionsUpdated();
		return;
	}

	if (SessionSearch->SearchResults.Num() == 0)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow, TEXT("No online sessions found"));
		}
		OnSessionsUpdated();
		OnSessionsNotFound();
		return;
	}

	for (int32 Index = 0; Index < SessionSearch->SearchResults.Num(); ++Index)
	{
		const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[Index];

		FServerInfo Info;
		Info.SessionIndex = Index;
		Info.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
		Info.CurrentPlayers = Info.MaxPlayers - Result.Session.NumOpenPublicConnections;
		Info.Ping = Result.PingInMs;

		FString ServerName;
		if (!Result.Session.SessionSettings.Get(SERVER_NAME_KEY, ServerName))
		{
			ServerName = Result.Session.OwningUserName;
		}
		Info.ServerName = ServerName;

		FoundServers.Add(Info);

		FString FoundKeyword;
		Result.Session.SessionSettings.Get(SEARCH_KEYWORDS, FoundKeyword);

		FString FoundMap;
		Result.Session.SessionSettings.Get(SETTING_MAPNAME, FoundMap);

		UE_LOG(LogTemp, Log, TEXT("Found Session [%d] Name=%s Keyword=%s Map=%s Ping=%d"),
			Index, *ServerName, *FoundKeyword, *FoundMap, Info.Ping);
	}

	OnSessionsUpdated();
	OnSessionFound();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green,
			FString::Printf(TEXT("Found %d online sessions"), FoundServers.Num()));
	}
}

void UNP_GameInstance::NP_OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Join complete: SessionInterface invalid"));
		return;
	}

	Sessions->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegateHandle);

	UE_LOG(LogTemp, Warning, TEXT("Join complete result = %d"), (int32)Result);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Yellow,
			FString::Printf(TEXT("Join complete result = %d"), (int32)Result));
	}

	APlayerController* PlayerController = GetFirstLocalPlayerController();
	FString TravelURL;
	const bool bHasConnect = Sessions->GetResolvedConnectString(SessionName, TravelURL);

	UE_LOG(LogTemp, Warning, TEXT("Resolved connect string = %s | URL = %s"),
		bHasConnect ? TEXT("true") : TEXT("false"),
		*TravelURL);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.f, bHasConnect ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("Resolved=%s URL=%s"),
				bHasConnect ? TEXT("true") : TEXT("false"),
				*TravelURL));
	}

	if (UWorld* World = GetWorld())
	{
		if (UNetDriver* Driver = World->GetNetDriver())
		{
			UE_LOG(LogTemp, Warning, TEXT("Current NetDriver=%s"),
				*Driver->GetClass()->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Current NetDriver=null"));
		}
	}

	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		if (bHasConnect)
		{
			PC->ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute);
		}
	}
}

void UNP_GameInstance::NP_OnDestroySessionComplete(FName SessionName, bool bSuccess)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}

	Sessions->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateHandle);

	if (bSuccess && bPendingCreateAfterDestroy)
	{
		bPendingCreateAfterDestroy = false;

		TSharedPtr<const FUniqueNetId> UserId = GetPrimaryNetId();
		if (UserId.IsValid())
		{
			HostSession(UserId, PendingSessionName, bIsLAN, true, PendingMaxPlayers, PendingSearchKeyword);
			return;
		}
	}

	if (bSuccess)
	{
		UGameplayStatics::OpenLevel(GetWorld(), TEXT("MainMenu"), true);
	}
}