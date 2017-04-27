#pragma once
#include "ActorManager.generated.h"

USTRUCT()
struct FObjectMeta {
	GENERATED_USTRUCT_BODY();
	UPROPERTY(SaveGame)
	FVector velocity;
	UPROPERTY(SaveGame)
	FVector angularVelocity;
	UPROPERTY(SaveGame)
	FTransform transform;
	UPROPERTY(SaveGame)
	float deltaTime;
	UPROPERTY(SaveGame)
	float timeStamp;
};

USTRUCT()
struct FObjectInfo {
	GENERATED_USTRUCT_BODY();
	AActor* actor;
	UPROPERTY(SaveGame)
	FString actorName;
	FObjectInfo* ancestor;
	UPROPERTY(SaveGame)
	FString ancestorName;
	FObjectInfo* descendant;
	UPROPERTY(SaveGame)
	FString descendantName;
	UPROPERTY(SaveGame)
	TArray<FObjectMeta> hist;
	UPROPERTY(SaveGame)
	int index;
	UPROPERTY(SaveGame)
	bool bisGhost = false;
};

USTRUCT(BlueprintType)
struct FHistory {
	GENERATED_USTRUCT_BODY();
	UPROPERTY(SaveGame)
	TMap<FString, FObjectInfo> histMap;
	UPROPERTY(SaveGame)
	FString name;
	UPROPERTY(SaveGame)
	bool bfinished = false;
	UPROPERTY(SaveGame)
	int level = 1;

};

FORCEINLINE FArchive &operator <<(FArchive &Ar, FObjectMeta& TheStruct)
{
	Ar << TheStruct.velocity;
	Ar << TheStruct.angularVelocity;
	Ar << TheStruct.transform;
	Ar << TheStruct.deltaTime;
	Ar << TheStruct.timeStamp;
	return Ar;
}

FORCEINLINE FArchive &operator <<(FArchive &Ar, FHistory& TheStruct)
{
	Ar << TheStruct.histMap;
	Ar << TheStruct.name;
	Ar << TheStruct.bfinished;
	Ar << TheStruct.level;

	return Ar;
}

FORCEINLINE FArchive &operator <<(FArchive &Ar, FObjectInfo& TheStruct)
{
	Ar << TheStruct.actorName;
	Ar << TheStruct.ancestorName;
	Ar << TheStruct.descendantName;
	Ar << TheStruct.hist;
	Ar << TheStruct.index;
	Ar << TheStruct.bisGhost;

	return Ar;
}


/**
 * 
 */
UCLASS()
class ARDAN_API UActorManager : public UObject {
	GENERATED_BODY()

public:
	TArray<FHistory*> pawnHistories;
	TArray<FHistory*> histories;
	FHistory* currentHistory;
	FHistory* currentPawnHistory;
private:
	UWorld* world;
	APlayerController* controller;
	

	bool bReverse = false;
	bool bReplay = false;
	bool bRecording = false;
public:
	UActorManager();
	~UActorManager();
	void init(UWorld* w, APlayerController* c);
	void initHist();

	void recordActors(float deltaTime, float timeStamp);
	void recordPawnActors(float deltaTime, float timeStamp);

	void copyActors(FHistory* dstHistory, FHistory *srcHistory);
	void copyPawnActors(FHistory * dstHistory, FHistory * srcHistory);

	void NewTimeline();
	void FixReferences(FHistory *);
	void FixPawnReferences(FHistory *);

	void ResetTimelines();

	void rewindMeshActors(FHistory *history, bool freeze, float timeStamp);
	void rewindPawnActors(FHistory *history);

	void rewindCurrentMeshActors(bool freeze, float timeStamp);
	void rewindCurrentPawnActors();
	void rewindAllMeshActors(bool freeze, float timeStamp);
	void rewindAllPawnActors();

	void replayActors(FHistory *history, float timeStamp);
	void replayPawnActors(FHistory * history, float timeStamp);
	void replayCurrentActors(float timeStamp);
	void replayAllActors(float timeStamp);
	void replayCurrentPawnActors(float timeStamp);
	void replayAllPawnActors(float timestamp);

	void resetActors(FHistory *history);
	void resetPawnActors(FHistory * history);

	void diff(FObjectInfo * info);
	void ghostActor(AActor * mesh, float amount);
	void colourActor(AActor * mesh);
};