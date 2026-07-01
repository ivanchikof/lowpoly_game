#include "Player.h"
#include "raymath.h"
#include <cmath>

Player::Player() {
    position = {8.0f, 60.0f, 8.0f}; 
    velocity = {0, 0, 0};
    camera = {0};
    yaw = 0.0f; pitch = 0.0f;
    isGrounded = false; isCrouching = false;
    currentBlockToPlace = 1; 

    camera.up = {0, 1, 0};
    camera.fovy = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;
}

void Player::SpawnSafely(World& world) {
    bool found = false;
    int testX = 8, testZ = 8; 
    
    while (!found) {
        for (int y = CHUNK_HEIGHT - 1; y > 0; y--) {
            int b = world.GetBlock(testX, y, testZ);
            if (b != 0) { 
                if (b == 8 || b == 9 || b == 7 || (b >= 11 && b <= 16)) { 
                    testX += 16; 
                    testZ += 16;
                    break; 
                } else {
                    position = {(float)testX, y + 2.5f, (float)testZ};
                    found = true;
                    break;
                }
            }
        }
    }
}

void Player::Update(World& world, float dt) {
    if (IsKeyPressed(KEY_ONE))   currentBlockToPlace = 1;  
    if (IsKeyPressed(KEY_TWO))   currentBlockToPlace = 2;  
    if (IsKeyPressed(KEY_THREE)) currentBlockToPlace = 3;  
    if (IsKeyPressed(KEY_FOUR))  currentBlockToPlace = 4;  
    if (IsKeyPressed(KEY_FIVE))  currentBlockToPlace = 5;  
    if (IsKeyPressed(KEY_SIX))   currentBlockToPlace = 7;  
    if (IsKeyPressed(KEY_SEVEN)) currentBlockToPlace = 10; 

    Vector2 mouseDelta = GetMouseDelta();
    yaw -= mouseDelta.x * 0.003f;
    pitch -= mouseDelta.y * 0.003f;
    if(pitch > 1.5f) pitch = 1.5f;   
    if(pitch < -1.5f) pitch = -1.5f;

    Vector3 forward = { cosf(pitch)*sinf(yaw), sinf(pitch), cosf(pitch)*cosf(yaw) };
    Vector3 right = { sinf(yaw - PI/2.0f), 0, cosf(yaw - PI/2.0f) };

    int headBlock = world.GetBlock(round(position.x), round(position.y), round(position.z));
    int feetBlock = world.GetBlock(round(position.x), round(position.y - 1.0f), round(position.z));
    
    // Перевіряємо всі види води (8 та 11-16)
    bool inWater = (headBlock == 8 || headBlock == 9 || feetBlock == 8 || feetBlock == 9 ||
                    (headBlock >= 11 && headBlock <= 16) || (feetBlock >= 11 && feetBlock <= 16));

    isCrouching = IsKeyDown(keys.crouch); 
    bool isSprinting = IsKeyDown(keys.sprint);
    
    float targetSpeed = 5.0f; 
    if (isCrouching) targetSpeed = 2.0f;
    else if (isSprinting) targetSpeed = 8.5f;
    if (inWater) targetSpeed *= 0.5f; 

    Vector3 moveDir = {0};
    if (IsKeyDown(keys.forward)) moveDir = Vector3Add(moveDir, {forward.x, 0, forward.z});
    if (IsKeyDown(keys.backward)) moveDir = Vector3Subtract(moveDir, {forward.x, 0, forward.z});
    if (IsKeyDown(keys.right)) moveDir = Vector3Add(moveDir, right);
    if (IsKeyDown(keys.left)) moveDir = Vector3Subtract(moveDir, right);

    if (Vector3Length(moveDir) > 0) moveDir = Vector3Normalize(moveDir);
    Vector3 targetVelocity = Vector3Scale(moveDir, targetSpeed);

    float accel = inWater ? 3.0f : (isGrounded ? 12.0f : 1.5f);
    velocity.x = Lerp(velocity.x, targetVelocity.x, accel * dt);
    velocity.z = Lerp(velocity.z, targetVelocity.z, accel * dt);

    if (inWater) {
        velocity.y -= 3.0f * dt; 
        if (velocity.y < -4.0f) velocity.y = -4.0f; 
        if (IsKeyDown(keys.jump)) velocity.y = 4.0f; 
    } else {
        velocity.y -= 20.0f * dt; 
        if (isGrounded && IsKeyPressed(keys.jump)) {
            velocity.y = 7.5f; 
            isGrounded = false;
        }
    }

    auto isSolid = [&](float x, float y, float z) {
        int b = world.GetBlock(round(x), round(y), round(z));
        return b != 0 && b != 8 && b != 9 && b != 7 && !(b >= 11 && b <= 16); 
    };

    position.x += velocity.x * dt;
    if(isSolid(position.x, position.y - 1.0f, position.z) || isSolid(position.x, position.y, position.z)) {
        position.x -= velocity.x * dt; velocity.x = 0;
    }
    position.z += velocity.z * dt;
    if(isSolid(position.x, position.y - 1.0f, position.z) || isSolid(position.x, position.y, position.z)) {
        position.z -= velocity.z * dt; velocity.z = 0;
    }

    position.y += velocity.y * dt;
    isGrounded = false;
    
    float playerBottom = position.y - 1.5f; 
    int bx = round(position.x); int bz = round(position.z); int by = round(playerBottom); 

    if (velocity.y <= 0 && isSolid(bx, by, bz)) {
        float blockTop = by + 0.5f; 
        if (playerBottom <= blockTop) { 
            position.y = blockTop + 1.5f; 
            velocity.y = 0;
            isGrounded = true;
        }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        Vector3 rayPos = camera.position; Vector3 prevPos = rayPos; 
        for(int i = 0; i < 100; i++) { 
            rayPos = Vector3Add(rayPos, Vector3Scale(forward, 0.05f)); 
            int bx = round(rayPos.x); int by = round(rayPos.y); int bz = round(rayPos.z);
            int hitBlock = world.GetBlock(bx, by, bz);
            
            if (hitBlock != 0 && hitBlock != 8 && hitBlock != 9 && !(hitBlock >= 11 && hitBlock <= 16)) { 
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (hitBlock != 10) world.SetBlock(bx, by, bz, 0); 
                } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    int px = round(prevPos.x); int py = round(prevPos.y); int pz = round(prevPos.z);
                    bool inPlayerBody = (px == round(position.x) && (py == round(position.y) || py == round(position.y - 1.0f)) && pz == round(position.z));
                    if (!inPlayerBody) world.SetBlock(px, py, pz, currentBlockToPlace); 
                }
                break; 
            }
            prevPos = rayPos; 
        }
    }

    float eyeHeight = isCrouching ? 0.2f : 0.6f;
    camera.position = { position.x, position.y + eyeHeight, position.z };
    camera.target = Vector3Add(camera.position, forward);
}