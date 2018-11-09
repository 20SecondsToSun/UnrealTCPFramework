// Fill out your copyright notice in the Description page of Project Settings.

#include "TCPServerComponent.h"
#include "Runtime/Sockets/Public/Sockets.h"
#include "Runtime/Networking/Public/Common/TcpSocketBuilder.h"
#include "Runtime/Networking/Public/Interfaces/IPv4/IPv4Address.h"
#include "Runtime/Networking/Public/Interfaces/IPv4/IPv4Endpoint.h"
#include "Async.h"

UTCPServerComponent::UTCPServerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTCPServerComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UTCPServerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopListenServer();
	Super::EndPlay(EndPlayReason);
}

void UTCPServerComponent::StartListenServer(const FString& Ip, int32 Port)
{
	FIPv4Address Address;
	FIPv4Address::Parse(Ip, Address);

	FIPv4Endpoint Endpoint(Address, Port);		

	ListenSocket = FTcpSocketBuilder(*SocketName)
		//.AsNonBlocking()
		.AsReusable()
		.BoundToEndpoint(Endpoint);

	if (ListenSocket)
	{
		ListenSocket->SetReceiveBufferSize(BufferMaxSize, BufferMaxSize);
		ListenSocket->SetSendBufferSize(BufferMaxSize, BufferMaxSize);

		bServerRunning = true;

		if (ListenSocket->Listen(MaxConnections))
		{
			OnListenSocketStart.Broadcast();

			ServerFinishedFuture = Async<void>(EAsyncExecution::Thread, [&]()
			{
				UE_LOG(LogTemp, Warning, TEXT("Server starting...!"));

				while (bServerRunning)
				{					
					bool bHasPendingConnection;
					ListenSocket->HasPendingConnection(bHasPendingConnection);

					if (bHasPendingConnection)
					{
						FSocket* Client = ListenSocket->Accept(TEXT("TCP-Client"));
						TSharedPtr<FInternetAddr> Addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();						
						Client->GetAddress(*Addr);	
						Clients.Add(Client);

						FString MyString = Addr->ToString(true);
						UE_LOG(LogTemp, Warning, TEXT("Incoming connection: %s"), *MyString);

						AsyncTask(ENamedThreads::GameThread, [&, Addr]()
						{
							OnClientConnected.Broadcast(Addr->ToString(true));
						});
					}		

					for (FSocket* Client : Clients)
					{
						uint32 BufferSize = 0;
						TArray<uint8> ReceiveBuffer;	

						//TOOD: disconnection doesn't work
						if (Client->GetConnectionState() != ESocketConnectionState::SCS_Connected)
						{
							AsyncTask(ENamedThreads::GameThread, [&]()
							{
								OnClientDisconnected.Broadcast("");
							});
						}

						if (Client->HasPendingData(BufferSize))
						{
							ReceiveBuffer.SetNumUninitialized(BufferSize);

							int32 BytesRead = 0;
							Client->Recv(ReceiveBuffer.GetData(), ReceiveBuffer.Num(), BytesRead);							
							FString data = StringFromBinaryArray(ReceiveBuffer);							 

							AsyncTask(ENamedThreads::GameThread, [&, data]()
							{
								OnReceivedString.Broadcast(data);
							});
						}
						
						Mutex.Lock();
						for (FTCPMessage& TCPMessage : MessageQueque)
						{
							if (!TCPMessage.bExpired)
							{
								int32 BytesSent = 0;								
								const FString Message = TCPMessage.GetMessage();
								bool bSent = Client->Send((uint8*)TCHAR_TO_UTF8(*Message), Message.Len(), BytesSent);

								if (bSent)
								{
									UE_LOG(LogTemp, Warning, TEXT("------ Message Sent ------ %s"), *Message);
									TCPMessage.bExpired = true;
								}	
								else
								{
									UE_LOG(LogTemp, Error, TEXT("------ Probably Disconnected ------"));
									//todo: remove from array
								}
							}
						}
						Mutex.Unlock();
					}					

					FPlatformProcess::Sleep(.00001);
				}						
			});
		}
		else
		{
			OnListenSocketError.Broadcast(ESocketError::SocketStartError);
		}
	}
	else
	{
		OnListenSocketError.Broadcast(ESocketError::SocketCreationError);
	}	
}

void UTCPServerComponent::StopListenServer()
{
	if (ListenSocket)
	{
		bServerRunning = false;
		ServerFinishedFuture.Wait();

		for (FSocket* Client : Clients)
		{
			Client->Close();
		}

		Clients.Empty();

		ListenSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
		ListenSocket = nullptr;

		OnListenSocketStop.Broadcast();
	}
}

FString UTCPServerComponent::StringFromBinaryArray(TArray<uint8> BinaryArray)
{
	BinaryArray.Add(0); 
	//return FString(ANSI_TO_TCHAR(reinterpret_cast<const char*>(BinaryArray.GetData())));
	return FString(UTF8_TO_TCHAR(BinaryArray.GetData()));
}

void UTCPServerComponent::SendMessage(const FString& Message)
{	
	Mutex.Lock();

	ClearExpired();
	MessageQueque.Add(FTCPMessage(Message, 0));	

	Mutex.Unlock();
}

void UTCPServerComponent::ClearExpired()
{
	for (int32 i = 0; i < MessageQueque.Num(); ++i)
	{
		if (MessageQueque[i].bExpired)
		{
			MessageQueque.RemoveAt(i);
		}
	}
}
