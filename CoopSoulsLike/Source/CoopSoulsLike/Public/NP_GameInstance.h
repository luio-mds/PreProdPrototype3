// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "NP_GameInstance.generated.h"

USTRUCT(BlueprintType)
struct FServerInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString ServerName = TEXT("");

	UPROPERTY(BlueprintReadOnly)
	int32 MaxPlayers = 100;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Ping = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 SessionIndex = -1;
};

UCLASS()
class COOPSOULSLIKE_API UNP_GameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UNP_GameInstance(const FObjectInitializer& ObjectInitializer);

	virtual void Init() override;
	virtual void BeginDestroy() override;

	bool HostSession(
		TSharedPtr<const FUniqueNetId> UserId,
		FName SessionName,
		bool bInIsLAN,
		bool bIsPresence,
		int32 MaxNumberPlayers,
		const FString& InSearchKeyword);

	bool FindSessions(
		TSharedPtr<const FUniqueNetId> UserId,
		bool bInIsLAN,
		bool bIsPresence,
		const FString& InSearchKeyword);

	bool JoinSession(
		TSharedPtr<const FUniqueNetId> UserId,
		FName SessionName,
		const FOnlineSessionSearchResult& SearchResult);

	UFUNCTION(BlueprintCallable, Category = "Network|Connection")
	void StartOnlineGame();

	UFUNCTION(BlueprintCallable, Category = "Network|Connection")
	void FindOnlineGame();

	UFUNCTION(BlueprintCallable, Category = "Network|Connection")
	void JoinOnlineGame();

	UFUNCTION(BlueprintCallable, Category = "Network|Connection")
	bool JoinSessionByIndex(int32 SessionIndex);

	UFUNCTION(BlueprintCallable, Category = "Network|Connection")
	void DestroySessionAndLeaveGame();

	UPROPERTY(BlueprintReadWrite, Category = "Network|Settings")
	bool bIsLAN = false;

	UPROPERTY(BlueprintReadWrite, Category = "Network|Settings")
	FString CustomServerName = TEXT("MyServer");

	UPROPERTY(BlueprintReadWrite, Category = "Network|Settings")
	FString SearchKeyword = TEXT("Custom12");

	UPROPERTY(BlueprintReadOnly, Category = "Network|Sessions")
	TArray<FServerInfo> FoundServers;

	UFUNCTION(BlueprintImplementableEvent, Category = "Network|Sessions")
	void OnSessionsUpdated();

	UFUNCTION(BlueprintImplementableEvent, Category = "Network|Sessions")
	void OnSessionStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Network|Sessions")
	void OnSessionFound();

	UFUNCTION(BlueprintCallable, Category = "Network|Sessions")
	const TArray<FServerInfo>& GetFoundServers() const { return FoundServers; }

protected:
	void NetworkFailureHappened(
		UWorld* World,
		UNetDriver* NetDriver,
		ENetworkFailure::Type FailureType,
		const FString& ErrorString);

	void NP_OnCreateSessionComplete(FName SessionName, bool bSuccess);
	void NP_OnStartOnlineGameComplete(FName SessionName, bool bSuccess);
	void NP_OnFindSessionsComplete(bool bSuccess);
	void NP_OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void NP_OnDestroySessionComplete(FName SessionName, bool bSuccess);

	TSharedPtr<const FUniqueNetId> GetPrimaryNetId() const;
	IOnlineSessionPtr GetSessionInterface() const;

protected:
	FOnCreateSessionCompleteDelegate OnCreateSessionCompleteDelegate;
	FOnStartSessionCompleteDelegate OnStartSessionCompleteDelegate;
	FOnFindSessionsCompleteDelegate OnFindSessionsCompleteDelegate;
	FOnJoinSessionCompleteDelegate OnJoinSessionCompleteDelegate;
	FOnDestroySessionCompleteDelegate OnDestroySessionCompleteDelegate;

	FDelegateHandle OnCreateSessionCompleteDelegateHandle;
	FDelegateHandle OnStartSessionCompleteDelegateHandle;
	FDelegateHandle OnFindSessionsCompleteDelegateHandle;
	FDelegateHandle OnJoinSessionCompleteDelegateHandle;
	FDelegateHandle OnDestroySessionCompleteDelegateHandle;

	TSharedPtr<FOnlineSessionSettings> SessionSettings;
	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	FName PendingSessionName = NAME_GameSession;
	bool bPendingCreateAfterDestroy = false;
	int32 PendingMaxPlayers = 2;
	FString PendingSearchKeyword = TEXT("Custom12");

	static const FName SERVER_NAME_KEY;
	static const FName MATCH_TYPE_KEY;
};
