/*
* Class implementation for pawn class in MulticopterSim
*
* Copyright (C) 2018 Simon D. Levy
*
* MIT License
*/

#include "3DFly.h"

// Structures to hold static mesh initializations
DECLARE_STATIC_MESH(FFrameStatics, "3DFly/Frame.Frame", FrameStatics)
DECLARE_STATIC_MESH(FMotorStatics, "3DFly/Motor.Motor", MotorStatics)
DECLARE_STATIC_MESH(FProp1Statics, "3DFly/Prop1.Prop1", Prop1Statics)
DECLARE_STATIC_MESH(FProp2Statics, "3DFly/Prop2.Prop2", Prop2Statics)
DECLARE_STATIC_MESH(FProp3Statics, "3DFly/Prop3.Prop3", Prop3Statics)
DECLARE_STATIC_MESH(FProp4Statics, "3DFly/Prop4.Prop4", Prop4Statics)

A3DFlyPawn::A3DFlyPawn()
{
    Vehicle::objects_t objects = {0};

    objects.pawn = this;
    objects.frameMesh = FrameStatics.mesh.Get();
    objects.motorMesh = MotorStatics.mesh.Get();

    QuadXAP::build(objects, _layout,
            Prop1Statics.mesh.Get(), Prop2Statics.mesh.Get(), Prop3Statics.mesh.Get(), Prop4Statics.mesh.Get()); 

    _vehicle = new QuadXAP(objects, _params);
}

A3DFlyPawn::~A3DFlyPawn()
{
}


void A3DFlyPawn::PostInitializeComponents()
{
    _vehicle->PostInitializeComponents();

    Super::PostInitializeComponents();
}

// Called when the game starts or when spawned
void A3DFlyPawn::BeginPlay()
{
    Super::BeginPlay();
}

void A3DFlyPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
}

// Called automatically on main thread
void A3DFlyPawn::Tick(float DeltaSeconds)
{
    _vehicle->Tick();

    Super::Tick(DeltaSeconds);
}