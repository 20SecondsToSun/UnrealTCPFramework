// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Runtime/Sockets/Public/IPAddress.h"
#include "TCPServerComponent.generated.h"

UENUM(BlueprintType)
enum class ESocketError : uint8
{
	SocketCreationError UMETA(DisplayName = "SocketCreationError"),
	SocketStartError UMETA(DisplayName = "SocketStartError")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTCPDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTCPErrorDelegate, ESocketError, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTCPClientDelegate, const FString&, Client);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTCPMessageDelegate, const TArray<uint8>&, Bytes);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTCPStringMessageDelegate, const FString&, Message);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TCPFRAMEWORK_API UTCPServerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UTCPServerComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network | TCP")
	int32 BufferMaxSize = 2 * 1024 * 1024;	//default roughly 2mb;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network | TCP")
	FString SocketName = FString(TEXT("TCP-server"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network | TCP")
	int32 MaxConnections = 8;
	
	UPROPERTY(BlueprintAssignable, Category = "Network | TCP")
	FTCPDelegate OnListenSocketStart;

	UPROPERTY(BlueprintAssignable, Category = "Network | TCP")
	FTCPDelegate OnListenSocketStop;

	UPROPERTY(BlueprintAssignable, Category = "Network | TCP")
	FTCPErrorDelegate OnListenSocketError;

	UPROPERTY(BlueprintAssignable, Category = "Network | TCP")
	FTCPClientDelegate OnClientConnected;

	UPROPERTY(BlueprintAssignable, Category = "Network | TCP")
	FTCPClientDelegate OnClientDisconnected;

	UPROPERTY(BlueprintAssignable, Category = "Network | TCP")
	FTCPMessageDelegate OnReceivedBytes;

	UPROPERTY(BlueprintAssignable, Category = "Network | TCP")
	FTCPStringMessageDelegate OnReceivedString;	

	UFUNCTION(BlueprintCallable, Category = "Network | TCP")
	void StartListenServer(const FString& Ip, int32 Port = 8080);

	UFUNCTION(BlueprintCallable, Category = "Network | TCP")
	void StopListenServer();

	UFUNCTION(BlueprintCallable, Category = "Network | TCP")
	void SendMessage(const FString& Message);

	static FString StringFromBinaryArray(TArray<uint8> BinaryArray);

protected:
	virtual void BeginPlay() override;	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	FSocket* ListenSocket;
	TArray<FSocket*> Clients;

	bool bServerRunning = true;
	TFuture<void> ServerFinishedFuture;	
};
