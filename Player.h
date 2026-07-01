#pragma once
#include "Globals.h"
#include "World.h"

struct KeyBinds {
    int forward = KEY_W;
    int backward = KEY_S;
    int left = KEY_A;
    int right = KEY_D;
    int jump = KEY_SPACE;
    int sprint = KEY_LEFT_SHIFT;
    int crouch = KEY_LEFT_CONTROL;
};

class Player {
public:
    Vector3 position;
    Vector3 velocity;
    Camera3D camera;
    
    float yaw;
    float pitch;
    bool isGrounded;
    bool isCrouching;
    int currentBlockToPlace;
    
    KeyBinds keys; 

    Player();
    void SpawnSafely(World& world); 
    void Update(World& world, float dt);
};