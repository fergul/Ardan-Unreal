// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Sensor.h"
/**
 * 
 */
class ARDAN_API SensorManager
{
public:
	SensorManager(UWorld* world);
	~SensorManager();

	void AddSensor(ASensor * sensor);

	void FindSensors();

	void ReceivePacket(uint8 * pkt);

private:
	UWorld* world;
	TArray<ASensor*> sensors;
	TMap<int, ASensor*> sensorTable;

	FColor colours[12] = { FColor(255, 0, 0),
		FColor(0, 255, 0),
		FColor(0, 0, 255),
		FColor(255,255,0),
		FColor(0,255,255),
		FColor(255,0,255),
		FColor(192,192,192),
		FColor(128,0,0),
		FColor(0,128,0),
		FColor(128,0,128),
		FColor(0,128,128),
		FColor(0,0,128) };
};
