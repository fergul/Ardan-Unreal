// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Ardan.h"
#include "ArdanPlayerController.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Runtime/Engine/Classes/Components/DecalComponent.h"
#include "Kismet/HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetStringLibrary.h"
#include "ArdanSave.h"
#include "Sensor.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "UObjectGlobals.h"
#include "Runtime/Engine/Classes/Camera/CameraActor.h"
#include "ArdanCharacter.h"
#include "ArdanUtilities.h"
#include <inttypes.h>

//using namespace cppkafka;
using namespace std;
#define LOG(txt) { GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::Red, txt, false); };

bool pathFlag = false;

static void dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque) {
	if (rkmessage->err)
		fprintf(stderr, "%% Message delivery failed: %s\n",
			rd_kafka_err2str(rkmessage->err));
	else {
		int64_t timestamp;
		rd_kafka_timestamp_type_t tstype;
		timestamp = rd_kafka_message_timestamp(rkmessage, &tstype);
		UE_LOG(LogNet, Log,
			TEXT(">> Message delivered (%zd bytes, partition %I32d, time: %I64d )\n"),
			rkmessage->len, rkmessage->partition, timestamp);
	}
	/* The rkmessage is destroyed automatically by librdkafka */
}
AArdanPlayerController::AArdanPlayerController() {
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
	/* Allows camera movement when world is paused */
	SetTickableWhenPaused(true);
	bShouldPerformFullTickWhenPaused = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
	
  /* Networking setup */
	//sockSubSystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	//socket = sockSubSystem->CreateSocket(NAME_DGram, TEXT("UDPCONN2"), true);
	//if (socket) UE_LOG(LogNet, Log, TEXT("Created Socket"));
	//socket->SetReceiveBufferSize(RecvSize, RecvSize);
	//socket->SetSendBufferSize(SendSize, SendSize);

	/* Set up destination address:port to send Cooja messages to */
	//FIPv4Address::Parse(address, ip);
	//addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	//addr->SetIp(ip.Value);
	//addr->SetPort(port);
}

void AArdanPlayerController::BeginPlay() {
  Super::BeginPlay();
	bRecording = false;
  conns = FRunnableConnection::create(5000, &packetQ);
  if (conns) {UE_LOG(LogNet, Log, TEXT("Kafka connection created"));}
  else { UE_LOG(LogNet, Log, TEXT("Kafka connection setup failed"));}


	TSubclassOf<ACameraActor> ClassToFind;
	cameras.Empty();
	currentCam = 0;
	for(TActorIterator<AActor> It(GetWorld(), ACameraActor::StaticClass()); It; ++It)
	{
		ACameraActor* Actor = (ACameraActor*)*It;
		if(!Actor->IsPendingKill() && !Actor->GetName().Equals("CameraActor_0")) {
			cameras.Add(Actor);
      Actor->SetTickableWhenPaused(true);
		}
	}
	UE_LOG(LogNet, Log, TEXT("Found %d cameras"), cameras.Num());
	
	sensorManager = new SensorManager(GetWorld(), this);
	sensorManager->FindSensors();

  UE_LOG(LogNet, Log, TEXT("--START--"));
	
	rd_kafka_conf_t *conf;  /* Temporary configuration object */
	char errstr[512];       /* librdkafka API error reporting buffer */
	char *buf = "hello";          /* Message value temporary buffer */
	const char *brokers = "192.168.0.121:9092";    /* Argument: broker list */
	const char *topic = "time"; /* Argument: topic to produce to */
	UE_LOG(LogNet, Log, TEXT("--Kafka:-- %s"), brokers);
	fprintf(stderr, "%s\n", brokers);
	conf = rd_kafka_conf_new();
	rd_kafka_conf_set(conf, "queue.buffering.max.ms", "0", NULL, 0);
	rd_kafka_conf_set(conf, "socket.blocking.max.ms", "1", NULL, 0);

	/* Set bootstrap broker(s) as a comma-separated list of
	* host or host:port (default port 9092).
	* librdkafka will use the bootstrap brokers to acquire the full
	* set of brokers from the cluster. */
	if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers,
		errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
		fprintf(stderr, "%s\n", errstr);
	}
	/*
	* Create producer instance.
	*
	* NOTE: rd_kafka_new() takes ownership of the conf object
	*       and the application must not reference it again after
	*       this call.
	*/
	rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
	if (!rk) {
		fprintf(stderr,
			"%% Failed to create new producer: %s\n", errstr);
		//return 1;
	}
	rkt = rd_kafka_topic_new(rk, topic, NULL);
	if (!rkt) {
		fprintf(stderr, "%% Failed to create topic object: %s\n",
			rd_kafka_err2str(rd_kafka_last_error()));
		rd_kafka_destroy(rk);
		//return 1;
	}
	UE_LOG(LogNet, Log, TEXT("--Kafka: Created time producer topic --"));
	const char *sensorTopic = "sensor"; /* Argument: topic to produce to */

	conf = rd_kafka_conf_new();
	rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);
	rd_kafka_conf_set(conf, "queue.buffering.max.ms", "0", NULL, 0);
	rd_kafka_conf_set(conf, "group.id", "rdkafka_consumer_example", NULL, 0);
	rd_kafka_conf_set(conf, "socket.blocking.max.ms", "1", NULL, 0);
	rd_kafka_conf_set(conf, "fetch.error.backoff.ms", "1", NULL, 0);
	


	/* Set bootstrap broker(s) as a comma-separated list of
	* host or host:port (default port 9092).
	* librdkafka will use the bootstrap brokers to acquire the full
	* set of brokers from the cluster. */
	if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers,
		errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
		fprintf(stderr, "%s\n", errstr);
	}
	/*
	* Create producer instance.
	*
	* NOTE: rd_kafka_new() takes ownership of the conf object
	*       and the application must not reference it again after
	*       this call.
	*/
	sensorrk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
	if (!sensorrk) {
		fprintf(stderr,
			"%% Failed to create new producer: %s\n", errstr);
		//return 1;
	}
	sensorrkt = rd_kafka_topic_new(sensorrk, sensorTopic, NULL);
	if (!sensorrkt) {
		fprintf(stderr, "%% Failed to create topic object: %s\n",
			rd_kafka_err2str(rd_kafka_last_error()));
		rd_kafka_destroy(sensorrk);
		//return 1;
	}
	UE_LOG(LogNet, Log, TEXT("--Kafka: Created sensor producer topic --"));
	sensorManager->SetProducer(sensorrk, sensorrkt);





	rd_kafka_poll(sensorrk, 0/*non-blocking*/);
}

void AArdanPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	packetQ.Empty();
	if (rk) {
		rd_kafka_destroy(rk);
	}
	if (sensorrk) {
		rd_kafka_destroy(sensorrk);
	}
  if (conns) {
    conns->Stop();
    conns->Shutdown();
  }
	delete sensorManager;
}

void AArdanPlayerController::NewTimeline() {
	reset = true;
	bRecording = true;
	bReplay = false;
	bReplayHistory = true;
	curTime = 0;
	replayTime = 0;
	actorManager->NewTimeline();
	sensorManager->NewTimeline();
	UE_LOG(LogNet, Log, TEXT("LogBlueprintUserMessages: [EvacMap_C_0] %s: Evacuation New timeline"), *(UKismetStringLibrary::TimeSecondsToString(GetWorld()->GetTimeSeconds())));
}

void AArdanPlayerController::replayPressed() {
	bReplay = true;
	bRecording = false;
	bReplayHistory = false;
	curTime = 0;
	replayTime = 0;
	actorManager->ResetTimelines();
	sensorManager->Replay();
	UE_LOG(LogNet, Log, TEXT("LogBlueprintUserMessages: [EvacMap_C_0] %s: Evacuation Reset timeline"), *(UKismetStringLibrary::TimeSecondsToString(GetWorld()->GetTimeSeconds())));
}

void AArdanPlayerController::JumpForwardPressed() {
	replayTime += 5.0;
	curTime += 5.0;
}

void AArdanPlayerController::JumpBackwardPressed() {
	replayTime -= 5.0;
	curTime -= 5.0;
	if (replayTime < 0) {
		replayTime = 0;
		curTime = 0;
	}
	actorManager->rewindCurrentMeshActors(false, curTime);
	actorManager->rewindAllMeshActors(true, curTime);
	actorManager->rewindCurrentPawnActors(curTime);
	actorManager->rewindAllPawnActors(curTime);

	sensorManager->RewindState(curTime);
}


void AArdanPlayerController::PlayerTick(float DeltaTime) {
	Super::PlayerTick(DeltaTime);

	if (UGameplayStatics::IsGamePaused(GetWorld())) return;
	elapsed += DeltaTime;
	tenth += DeltaTime;
	rtick += DeltaTime;

	rd_kafka_poll(sensorrk, 0/*non-blocking*/);

	/* Pop and set actors old location else push current location onto stack */
	LOG(FString::Printf(TEXT("CurTime: %f"), curTime));
	if (bReverse) {
		replayTime -= DeltaTime;
		curTime -= DeltaTime;

		actorManager->rewindCurrentMeshActors(false, curTime);
		actorManager->rewindAllMeshActors(true, curTime);
		actorManager->rewindCurrentPawnActors(curTime);
		actorManager->rewindAllPawnActors(curTime);

		sensorManager->RewindState(curTime);
		return;
	}
	else {
		curTime += DeltaTime;
		if (bReplay) {
			replayTime += DeltaTime;
			UE_LOG(LogNet, Log, TEXT("Replaying: %f"), replayTime);
			actorManager->replayCurrentActors(replayTime);
			actorManager->replayAllActors(replayTime);
			actorManager->replayCurrentPawnActors(replayTime);
			actorManager->replayAllPawnActors(replayTime);

			sensorManager->FastForwardState(replayTime);
		}
		else if (bReplayHistory) {
			replayTime += DeltaTime;
			actorManager->replayAllActors(replayTime);
			actorManager->replayAllPawnActors(replayTime);
		}
		if (bRecording && rtick >= 0.03) {
			rtick = 0;
			actorManager->recordActors(DeltaTime, curTime);
			actorManager->recordPawnActors(DeltaTime, curTime, pathFlag, reset);
			if (pathFlag) pathFlag = false;
			if (reset) reset = false;
		}
	}
  
	//actorManager->diff(NULL);
	//sensorManager->DiffState(0, curTime);

	/* Working out FPS */
	
	tickCount += 1;
	if (elapsed >= 1) {
		UE_LOG(LogNet, Log, TEXT("TSTAMP(%f): %f %f %f %d "), curTime, DeltaTime, 1/DeltaTime,
					elapsed/tickCount,
					tickCount);
		elapsed = 0;
		tickCount = 0;
		pathFlag = true;
		//for (APawn *actor : pawnActors) {
		//	UE_LOG(LogNet, Log, TEXT("%f,Location,%s,%s"), GetWorld()->TimeSeconds, *(actor->GetName()), *(actor->GetActorLocation().ToString()));
		//}
	}
	if (!(bReplay || bReverse) && tenth >= 1.0) {
		
		tenth = 0;
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.Owner = NULL;
		SpawnInfo.Instigator = NULL;
		SpawnInfo.bDeferConstruction = false;
  	SpawnInfo.bNoFail 		= false;
		//SpawnInfo.bNoCollisionFail = bNoCollisionFail;
		 //APawn* const Pawn = GetPawn();
		 /*if (Pawn) {
			 FVector sourceLoc = Pawn->GetActorLocation();
			 FRotator sourceRot = Pawn->GetActorRotation();
			 ATimeSphere *ts = GetWorld()->SpawnActor<ATimeSphere>(sourceLoc, sourceRot, SpawnInfo);
			 if (timeSpheres.Num() > 0 && !reset) {
				 DrawDebugLine(GetWorld(),
					 ts->GetActorLocation(), timeSpheres.Top()->GetActorLocation(),
					 FColor(255, 0, 0), false, 60, 0, 4);
			 }
			 reset = false;
			 if (ts != NULL) {
				 timeSpheres.Push(ts);
			 }
			 else UE_LOG(LogNet, Log, TEXT("Noo"));*/
	//}
		
		
		
	}

	// keep updating the destination every tick while desired
	if (bMoveToMouseCursor)
	{
		MoveToMouseCursor();
	}
	update_sensors();
}

void AArdanPlayerController::update_sensors() {
	rd_kafka_message_t *rkmessage;
  int count = 0;
	struct pkt pkt;
  while (!packetQ.IsEmpty()){
		//UE_LOG(LogNet, Log, TEXT("Got a packet to read..."));

    packetQ.Dequeue(rkmessage);
		pkt.data = (uint8_t *) rkmessage->payload;
		pkt.size = rkmessage->len;
		sensorManager->ReceivePacket(&pkt);
		rd_kafka_message_destroy(rkmessage);
  }
}

void AArdanPlayerController::SaveArdanState() {
	UArdanSave* save = Cast<UArdanSave>(UGameplayStatics::CreateSaveGameObject(UArdanSave::StaticClass()));
	save->PlayerName = FString("ARDAN");
	save->currentPawnHistory = *(actorManager->currentPawnHistory);
	save->currentHistory = *(actorManager->currentHistory);
	save->currentHistory.histMap = actorManager->currentHistory->histMap;
	save->currentHistory.name = FString("HELLO");
	sensorManager->CopyOutState(&save->sensorHistory);
	UE_LOG(LogNet, Log, TEXT("ARDAN Saved: %s (%d:%d)"), *save->SaveSlotName, 
		actorManager->currentHistory->histMap.Num(), 
		save->currentHistory.histMap.Num());
	ArdanUtilities::SaveGameToSlotCompressed(save, save->SaveSlotName, save->UserIndex);
	save->SaveSlotName = TEXT("UncompressedSave");
	UGameplayStatics::SaveGameToSlot(save, save->SaveSlotName, save->UserIndex);

}

void AArdanPlayerController::LoadArdanState() {
	UArdanSave* save = Cast<UArdanSave>(UGameplayStatics::CreateSaveGameObject(UArdanSave::StaticClass()));
	//save = Cast<UArdanSave>(UGameplayStatics::LoadGameFromSlot(save->SaveSlotName, save->UserIndex));
	save = Cast<UArdanSave>(ArdanUtilities::LoadGameFromSlotCompressed(save->SaveSlotName, save->UserIndex));
	FString PlayerNameToDisplay = save->PlayerName;
	*actorManager->currentHistory = save->currentHistory;
	*actorManager->currentPawnHistory = save->currentPawnHistory;
	UE_LOG(LogNet, Log, TEXT("ARDAN Loaded: %s (%d)"), *save->currentHistory.name, save->currentHistory.histMap.Num());
	actorManager->FixReferences(actorManager->currentHistory);
	actorManager->FixPawnReferences(actorManager->currentPawnHistory);
	sensorManager->CopyInState(&save->sensorHistory);
	replayPressed();

}

void AArdanPlayerController::SetupInputComponent()
{
	// set up gameplay key bindings
	Super::SetupInputComponent();

	InputComponent->BindAction("SetDestination", IE_Pressed, this, &AArdanPlayerController::OnSetDestinationPressed);
	InputComponent->BindAction("SetDestination", IE_Released, this, &AArdanPlayerController::OnSetDestinationReleased);

	// support touch devices
	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AArdanPlayerController::MoveToTouchLocation);
	InputComponent->BindTouch(EInputEvent::IE_Repeat, this, &AArdanPlayerController::MoveToTouchLocation);

	InputComponent->BindAction("ResetVR", IE_Pressed, this, &AArdanPlayerController::OnResetVR);

	InputComponent->BindAction("ScrollUp", IE_Pressed, this, &AArdanPlayerController::ScrollUp);
	InputComponent->BindAction("ScrollDown", IE_Pressed, this, &AArdanPlayerController::ScrollDown);

	InputComponent->BindAction("Pause", IE_Pressed, this, &AArdanPlayerController::Pause).bExecuteWhenPaused = true;
	InputComponent->BindAction("NextCamera", IE_Pressed, this, &AArdanPlayerController::NextCamera);

  InputComponent->BindAction("Slow", IE_Pressed, this, &AArdanPlayerController::speedSlow);
  InputComponent->BindAction("Normal", IE_Pressed, this, &AArdanPlayerController::speedNormal);
  InputComponent->BindAction("Fast", IE_Pressed, this, &AArdanPlayerController::speedFast);
  InputComponent->BindAction("Reverse", IE_Pressed, this, &AArdanPlayerController::reversePressed);
  InputComponent->BindAction("Reverse", IE_Released, this, &AArdanPlayerController::reverseReleased);

  InputComponent->BindAction("ResetReplay", IE_Pressed, this, &AArdanPlayerController::replayPressed);
	InputComponent->BindAction("NewTimeline", IE_Pressed, this, &AArdanPlayerController::NewTimeline);

	InputComponent->BindAction("Save", IE_Pressed, this, &AArdanPlayerController::SaveArdanState);
	InputComponent->BindAction("Load", IE_Pressed, this, &AArdanPlayerController::LoadArdanState);
	InputComponent->BindAction("TimeJumpForward", IE_Pressed, this, &AArdanPlayerController::JumpForwardPressed);
	InputComponent->BindAction("TimeJumpBackward", IE_Pressed, this, &AArdanPlayerController::JumpBackwardPressed);

	InputComponent->BindAction("StartFire", IE_Pressed, this, &AArdanPlayerController::StartAFire);
	InputComponent->BindAction("Record", IE_Pressed, this, &AArdanPlayerController::Record);
}

void AArdanPlayerController::Record()
{
	bRecording = true;
	actorManager = NewObject<UActorManager>();
	actorManager->init(GetWorld(), this);
	actorManager->initHist();
}

void AArdanPlayerController::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AArdanPlayerController::MoveToMouseCursor()
{
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		if (AArdanCharacter* MyPawn = Cast<AArdanCharacter>(GetPawn()))
		{
			if (MyPawn->GetCursorToWorld())
			{
				UNavigationSystem::SimpleMoveToLocation(this, MyPawn->GetCursorToWorld()->GetComponentLocation());
			}
		}
	}
	else
	{
		// Trace to see what is under the mouse cursor
		FHitResult Hit;
		GetHitResultUnderCursor(ECC_Visibility, false, Hit);

		if (Hit.bBlockingHit)
		{
			// We hit something, move there
			SetNewMoveDestination(Hit.ImpactPoint);
		}
	}
}

void AArdanPlayerController::MoveToTouchLocation(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	FVector2D ScreenSpaceLocation(Location);

	// Trace to see what is under the touch location
	FHitResult HitResult;
	GetHitResultAtScreenPosition(ScreenSpaceLocation, CurrentClickTraceChannel, true, HitResult);
	if (HitResult.bBlockingHit)
	{
		// We hit something, move there
		SetNewMoveDestination(HitResult.ImpactPoint);
	}
}

void AArdanPlayerController::SetNewMoveDestination(const FVector DestLocation)
{
	APawn* const MyPawn = GetPawn();
	if (MyPawn)
	{
		UNavigationSystem* const NavSys = GetWorld()->GetNavigationSystem();
		float const Distance = FVector::Dist(DestLocation, MyPawn->GetActorLocation());

		// We need to issue move command only if far enough in order for walk animation to play correctly
		if (NavSys && (Distance > 120.0f))
		{
			NavSys->SimpleMoveToLocation(this, DestLocation);
		}
	}
}

void AArdanPlayerController::OnSetDestinationPressed()
{
	// set flag to keep updating destination until released
	bMoveToMouseCursor = true;
}

void AArdanPlayerController::OnSetDestinationReleased()
{
	// clear flag to indicate we should stop updating the destination
	bMoveToMouseCursor = false;
}


void AArdanPlayerController::reversePressed() {
  bReverse = true;
}

void AArdanPlayerController::reverseReleased() {
  bReverse = false;
  //Pause();
}

void AArdanPlayerController::NextCamera() {
	if (cameras.Num() == 0) return;
	this->SetViewTargetWithBlend((ACameraActor *)cameras[currentCam], SmoothBlendTime);
	UE_LOG(LogNet, Log, TEXT("camera %s"), *cameras[currentCam]->GetName());

	currentCam = (currentCam + 1 ) % cameras.Num();
}

void AArdanPlayerController::Pause() {
  UE_LOG(LogNet, Log, TEXT("Paused"));
	UGameplayStatics::SetGamePaused(GetWorld(), !UGameplayStatics::IsGamePaused(GetWorld()));

  fbb.Clear();
	UnrealCoojaMsg::MessageBuilder msg(fbb);

	//msg.add_id(ID);
	msg.add_type(UnrealCoojaMsg::MsgType_SIMSTATE);
	msg.add_simState((UGameplayStatics::IsGamePaused(GetWorld()) ?
		UnrealCoojaMsg::SimState_PAUSE :
		UnrealCoojaMsg::SimState_RESUME));
	//msg.add_pir()
	auto mloc = msg.Finish();
	fbb.Finish(mloc);


	if (rd_kafka_produce(sensorrkt, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY, 
												fbb.GetBufferPointer(), fbb.GetSize(), NULL, 0, NULL) == -1) {
		fprintf(stderr, "%% Failed to produce to topic %s: %s\n", rd_kafka_topic_name(sensorrkt),
			rd_kafka_err2str(rd_kafka_last_error()));
		UE_LOG(LogNet, Log, TEXT("--KAFKAFAIL--"));
	}
	rd_kafka_poll(sensorrk, 0);

}

void AArdanPlayerController::speedSlow() {
  UE_LOG(LogNet, Log, TEXT("Speed at 0.1"));
  UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.1);

  fbb.Clear();
	UnrealCoojaMsg::MessageBuilder msg(fbb);
	msg.add_type(UnrealCoojaMsg::MsgType_SIMSTATE);
	msg.add_simState(slow ?
		UnrealCoojaMsg::SimState_NORMAL :
		UnrealCoojaMsg::SimState_SLOW);

	auto mloc = msg.Finish();
	fbb.Finish(mloc);

	
	if (rd_kafka_produce(sensorrkt, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY,
		fbb.GetBufferPointer(), fbb.GetSize(), NULL, 0, NULL) == -1) {
		fprintf(stderr, "%% Failed to produce to topic %s: %s\n", rd_kafka_topic_name(sensorrkt),
			rd_kafka_err2str(rd_kafka_last_error()));
		UE_LOG(LogNet, Log, TEXT("--KAFKAFAIL--"));
	}
	rd_kafka_poll(sensorrk, 0);
  slow = !slow;
}

void AArdanPlayerController::speedNormal() {
  UE_LOG(LogNet, Log, TEXT("Speed at 1.0"));
  UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0);

  fbb.Clear();
	UnrealCoojaMsg::MessageBuilder msg(fbb);
	msg.add_type(UnrealCoojaMsg::MsgType_SIMSTATE);
	msg.add_simState(UnrealCoojaMsg::SimState_NORMAL);
	auto mloc = msg.Finish();
	fbb.Finish(mloc);

	
	if (rd_kafka_produce(sensorrkt, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY,
		fbb.GetBufferPointer(), fbb.GetSize(), NULL, 0, NULL) == -1) {
		fprintf(stderr, "%% Failed to produce to topic %s: %s\n", rd_kafka_topic_name(sensorrkt),
			rd_kafka_err2str(rd_kafka_last_error()));
		UE_LOG(LogNet, Log, TEXT("--KAFKAFAIL--"));
	}
	rd_kafka_poll(sensorrk, 0);
}

void AArdanPlayerController::speedFast() {
  UE_LOG(LogNet, Log, TEXT("Speed at 2.0"));
  UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 2.0);

  fbb.Clear();
	UnrealCoojaMsg::MessageBuilder msg(fbb);
	msg.add_type(UnrealCoojaMsg::MsgType_SIMSTATE);
	msg.add_simState(UnrealCoojaMsg::SimState_DOUBLE);
	auto mloc = msg.Finish();
	fbb.Finish(mloc);


	if (rd_kafka_produce(sensorrkt, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY,
		fbb.GetBufferPointer(), fbb.GetSize(), NULL, 0, NULL) == -1) {
		fprintf(stderr, "%% Failed to produce to topic %s: %s\n", rd_kafka_topic_name(sensorrkt),
			rd_kafka_err2str(rd_kafka_last_error()));
		UE_LOG(LogNet, Log, TEXT("--KAFKAFAIL--"));
	}
	rd_kafka_poll(sensorrk, 0);
}

void AArdanPlayerController::ScrollUp() {
	AArdanCharacter* const pawn = (AArdanCharacter *) GetPawn();

		if (pawn){
			FRotator rot = pawn->GetCameraBoom()->RelativeRotation;
			rot.Add(5, 0, 0);
			pawn->GetCameraBoom()->SetWorldRotation(rot);

		}
}

void AArdanPlayerController::ScrollDown() {
	AArdanCharacter* const pawn = (AArdanCharacter *) GetPawn();

		if (pawn){
			FRotator rot = pawn->GetCameraBoom()->RelativeRotation;
			rot.Add(-5, 0, 0);
			pawn->GetCameraBoom()->SetWorldRotation(rot);
		}
}


void AArdanPlayerController::StartAFire() {
	// Trace to see what is under the mouse cursor
	FHitResult Hit;
	GetHitResultUnderCursor(ECC_Visibility, false, Hit);

	if (Hit.bBlockingHit) {
		// We hit something, start a fire there
		//static ConstructorHelpers::FObjectFinder<UParticleSystem> particle (TEXT("ParticleSystem'/Game/StarterContent/Particles/P_Fire.P_Fire'"));
		UParticleSystem *ps = ArdanUtilities::LoadObjFromPath<UParticleSystem>(TEXT("ParticleSystem'/Game/StarterContent/Particles/P_Fire.P_Fire'"));
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ps, Hit.Location);
		firePoints.Add(Hit.Location);// , Hit.ImpactNormal.Rotation, true);
	}
}