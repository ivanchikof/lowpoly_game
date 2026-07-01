#include "Globals.h"
#include "World.h"
#include "Player.h"
#include "rlgl.h" 
#include <fstream>
#include <iostream>
#include <cmath>

// --- ОНОВЛЕНИЙ ШЕЙДЕР З ПІДТРИМКОЮ РУХОМОГО СОНЦЯ ---
const char* vsSource = R"(
    #version 330
    in vec3 vertexPosition;
    in vec2 vertexTexCoord;
    in vec3 vertexNormal; // Передаємо нормаль (напрямок грані)
    in vec4 vertexColor;
    out vec2 fragTexCoord;
    out vec4 fragColor;
    out vec3 fragNormal;  // Передаємо нормаль у фрагментний шейдер
    uniform mat4 mvp;
    void main() {
        fragTexCoord = vertexTexCoord;
        fragColor = vertexColor;
        fragNormal = vertexNormal;
        gl_Position = mvp * vec4(vertexPosition, 1.0);
    }
)";

const char* fsSource = R"(
    #version 330
    in vec2 fragTexCoord;
    in vec4 fragColor;
    in vec3 fragNormal;
    out vec4 finalColor;
    uniform sampler2D texture0;
    uniform vec3 sunDir;    // Напрямок сонця (передається з C++)
    uniform float ambient;  // Загальна яскравість світу (день/ніч)
    void main() {
        vec4 texelColor = texture(texture0, fragTexCoord);
        if (texelColor.a < 0.1) discard; 
        
        // Рахуємо, наскільки сонце освітлює цю конкретність грань
        float dotProduct = dot(normalize(fragNormal), normalize(sunDir));
        float lightIntensity = max(dotProduct, 0.0);
        
        // Змішуємо розсіяне світло (ambient) та пряме світло сонця
        float totalLight = ambient + lightIntensity * (1.0 - ambient);
        
        // Множимо текстуру на динамічне світло сонця ТА на статичну тінь дерев (fragColor)
        finalColor = texelColor * fragColor * vec4(vec3(totalLight), 1.0);
    }
)";

// Функція для плавного змішування кольорів неба
Color LerpColor(Color a, Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        255
    };
}

void SaveGameInfo(Player& p) {
    std::ofstream file("player.dat");
    file << p.position.x << " " << p.position.y << " " << p.position.z << "\n";
    file << p.currentBlockToPlace << "\n";
    file << p.keys.forward << " " << p.keys.backward << " " << p.keys.left << " " << p.keys.right << " ";
    file << p.keys.jump << " " << p.keys.sprint << " " << p.keys.crouch << "\n";
    file.close();
}

void LoadGameInfo(Player& p) {
    std::ifstream file("player.dat");
    if(file.is_open()) {
        file >> p.position.x >> p.position.y >> p.position.z;
        file >> p.currentBlockToPlace;
        if(file.good()) { 
            file >> p.keys.forward >> p.keys.backward >> p.keys.left >> p.keys.right;
            file >> p.keys.jump >> p.keys.sprint >> p.keys.crouch;
        }
        file.close();
    }
}

const char* GetCustomKeyName(int keycode) {
    switch(keycode) {
        case KEY_SPACE: return "SPACE"; case KEY_LEFT_SHIFT: return "L-SHIFT";
        case KEY_LEFT_CONTROL: return "L-CTRL"; case KEY_UP: return "UP ARROW";
        case KEY_DOWN: return "DOWN ARROW"; case KEY_LEFT: return "LEFT ARROW";
        case KEY_RIGHT: return "RIGHT ARROW";
    }
    if (keycode > 32 && keycode <= 126) return TextFormat("%c", (char)keycode); 
    return "???";
}

bool DrawButton(int x, int y, int width, int height, const char* text) {
    Rectangle rect = {(float)x, (float)y, (float)width, (float)height};
    bool isHovered = CheckCollisionPointRec(GetMousePosition(), rect);
    DrawRectangleRec(rect, isHovered ? DARKGRAY : GRAY);
    DrawRectangleLinesEx(rect, 2, BLACK);
    DrawText(text, x + width/2 - MeasureText(text, 20)/2, y + height/2 - 10, 20, WHITE);
    return isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

int main() {
    InitWindow(1024, 768, "Voxel Engine - Day & Night Cycle");
    SetExitKey(KEY_NULL); 
    SetTargetFPS(60);
    
    Texture2D atlas = LoadTexture("atlas.png");
    SetTextureFilter(atlas, TEXTURE_FILTER_POINT);
    
    Shader voxelShader = LoadShaderFromMemory(vsSource, fsSource);
    
    voxelShader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocation(voxelShader, "vertexPosition");
    voxelShader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = GetShaderLocation(voxelShader, "vertexTexCoord");
    voxelShader.locs[SHADER_LOC_VERTEX_COLOR] = GetShaderLocation(voxelShader, "vertexColor");
    voxelShader.locs[SHADER_LOC_VERTEX_NORMAL] = GetShaderLocation(voxelShader, "vertexNormal"); // Передаємо нормалі!
    
    // Отримуємо посилання на змінні всередині шейдера
    int sunDirLoc = GetShaderLocation(voxelShader, "sunDir");
    int ambientLoc = GetShaderLocation(voxelShader, "ambient");
    
    World world;
    Player player;
    
    LoadGameInfo(player); 
    world.Update(player.position);
    if (player.position.y == 60.0f) player.SpawnSafely(world); 
    
    GameState gameState = STATE_PLAYING;
    DisableCursor();

    int hotbarBlocks[] = {1, 2, 3, 4, 5, 7, 10}; 
    const char* blockNames[] = {"Grass", "Dirt", "Stone", "Sand", "Wood", "Leaves", "Bedrock"};
    
    float autoSaveTimer = 0.0f;
    float saveMessageTimer = 0.0f;
    int* keyToBind = nullptr; 

    // --- НАЛАШТУВАННЯ ЧАСУ ---
    float timeOfDay = 450.0f; // Починаємо вранці
    const float DAY_LENGTH = 1800.0f; // 1800 секунд = 30 хвилин. (Постав 60.0f для швидкого тесту!)

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        if (IsKeyPressed(KEY_ESCAPE) && keyToBind == nullptr) {
            if (gameState == STATE_PLAYING) { gameState = STATE_MENU; EnableCursor(); } 
            else if (gameState == STATE_MENU) { gameState = STATE_PLAYING; DisableCursor(); } 
            else { gameState = STATE_MENU; } 
        }

        if (gameState == STATE_PLAYING) {
            player.Update(world, dt);
            world.Update(player.position);
            world.UpdateFluids(dt);
            
            // Рух часу
            timeOfDay += dt;
            if (timeOfDay >= DAY_LENGTH) timeOfDay = 0.0f;

            autoSaveTimer += dt;
            if (autoSaveTimer >= 30.0f) {
                world.SaveAll(); SaveGameInfo(player);
                saveMessageTimer = 3.0f; autoSaveTimer = 0.0f;
            }
        }

        // --- МАТЕМАТИКА СОНЦЯ ТА НЕБА ---
        float angle = (timeOfDay / DAY_LENGTH) * 2.0f * PI;
        // Сонце обертається по колу (X та Y)
        Vector3 sunDir = { cosf(angle), sinf(angle), 0.3f }; 
        
        // Розраховуємо яскравість світу (ambient). Сонце вгорі = яскраво (0.8), сонце внизу = ніч (0.15)
        float ambient = 0.15f + fmaxf(0.0f, sunDir.y) * 0.65f;
        
        // Кольори для плавного неба
        Color dayColor = SKYBLUE;
        Color nightColor = (Color){10, 14, 28, 255};
        Color sunsetColor = (Color){230, 90, 40, 255};
        
        Color currentSkyColor;
        // Плавний перехід кольору неба залежно від висоти сонця (sunDir.y)
        float skyT = (sunDir.y + 1.0f) * 0.5f; // Переводимо з [-1, 1] в [0, 1]
        currentSkyColor = LerpColor(nightColor, dayColor, skyT);
        
        // Додаємо красивий помаранчевий відтінок під час заходу/сходу
        if (sunDir.y > -0.2f && sunDir.y < 0.2f) {
            float sunsetFactor = 1.0f - (fabs(sunDir.y) / 0.2f);
            currentSkyColor = LerpColor(currentSkyColor, sunsetColor, sunsetFactor * 0.8f);
        }

        // Відправляємо дані про сонце у шейдер перед малюванням світу
        SetShaderValue(voxelShader, sunDirLoc, &sunDir, SHADER_UNIFORM_VEC3);
        SetShaderValue(voxelShader, ambientLoc, &ambient, SHADER_UNIFORM_FLOAT);

        // --- РЕНДЕР СВІТУ ---
        BeginDrawing(); 
        ClearBackground(currentSkyColor); // Динамічне небо!
        
        BeginMode3D(player.camera);
            rlDisableBackfaceCulling();
            for(auto c : world.chunks) {
                if(c->dirty) {
                    c->BuildMesh();
                    c->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = atlas;
                    c->model.materials[0].shader = voxelShader;
                }
                if(c->model.meshCount > 0) DrawModel(c->model, {0,0,0}, 1, WHITE);
            }
            rlEnableBackfaceCulling();
        EndMode3D();

        // --- ІНТЕРФЕЙС ---
        int cx = GetScreenWidth() / 2; int cy = GetScreenHeight() / 2;

        if (gameState == STATE_PLAYING) {
            DrawRectangle(cx - 10, cy - 2, 20, 4, LIGHTGRAY);
            DrawRectangle(cx - 2, cy - 10, 4, 20, LIGHTGRAY);

            int slotSize = 50; int padding = 5;
            int totalWidth = (slotSize * 7) + (padding * 6);
            int startX = (GetScreenWidth() - totalWidth) / 2;
            int startY = GetScreenHeight() - slotSize - 20;

            for (int i = 0; i < 7; i++) {
                int slotX = startX + i * (slotSize + padding);
                DrawRectangle(slotX, startY, slotSize, slotSize, { 0, 0, 0, 150 });
                if (player.currentBlockToPlace == hotbarBlocks[i]) {
                    DrawRectangleLinesEx({(float)slotX, (float)startY, (float)slotSize, (float)slotSize}, 3, YELLOW);
                    DrawText(blockNames[i], cx - MeasureText(blockNames[i], 20)/2, startY - 30, 20, WHITE);
                } else DrawRectangleLinesEx({(float)slotX, (float)startY, (float)slotSize, (float)slotSize}, 1, GRAY);
                DrawText(TextFormat("%d", i + 1), slotX + 5, startY + 5, 10, WHITE);
            }

            /*Виводимо поточний час доби на екран для зручності
            int hours = (int)((timeOfDay / DAY_LENGTH) * 24.0f);
            int minutes = (int)(((timeOfDay / DAY_LENGTH) * 24.0f - hours) * 60.0f);
            DrawText(TextFormat("Time: %02d:%02d", hours, minutes), 10, 80, 20, WHITE);
            */ 
            
            if (saveMessageTimer > 0) {
                DrawText("GAME SAVED!", 20, GetScreenHeight() - 40, 20, GREEN);
                saveMessageTimer -= dt;
            }
        } 
        else if (gameState == STATE_MENU) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 150}); 
            DrawText("PAUSED", cx - MeasureText("PAUSED", 40)/2, cy - 150, 40, WHITE);
            
            if (DrawButton(cx - 100, cy - 50, 200, 40, "Resume")) { gameState = STATE_PLAYING; DisableCursor(); }
            if (DrawButton(cx - 100, cy, 200, 40, "Manual Save")) {
                world.SaveAll(); SaveGameInfo(player);
                saveMessageTimer = 3.0f; gameState = STATE_PLAYING; DisableCursor();
            }
            if (DrawButton(cx - 100, cy + 50, 200, 40, "Graphics")) gameState = STATE_SETTINGS_GRAPHICS;
            if (DrawButton(cx - 100, cy + 100, 200, 40, "Controls")) gameState = STATE_SETTINGS_CONTROLS;
            if (DrawButton(cx - 100, cy + 150, 200, 40, "Quit")) break; 
        }
        else if (gameState == STATE_SETTINGS_GRAPHICS) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180});
            DrawText("GRAPHICS SETTINGS", cx - MeasureText("GRAPHICS SETTINGS", 40)/2, cy - 150, 40, WHITE);
            DrawText("More options coming soon...", cx - 150, cy, 20, LIGHTGRAY);
            if (DrawButton(cx - 100, cy + 100, 200, 40, "Back")) gameState = STATE_MENU;
        }
        else if (gameState == STATE_SETTINGS_CONTROLS) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180});
            
            if (keyToBind != nullptr) {
                DrawText("PRESS ANY KEY...", cx - MeasureText("PRESS ANY KEY...", 40)/2, cy - 20, 40, YELLOW);
                int key = GetKeyPressed(); if (key != 0) { *keyToBind = key; keyToBind = nullptr; }
            } else {
                DrawText("CONTROLS (Click to bind)", cx - MeasureText("CONTROLS (Click to bind)", 40)/2, cy - 200, 40, WHITE);
                int sy = cy - 120;
                if (DrawButton(cx - 150, sy, 300, 30, TextFormat("Forward: %s", GetCustomKeyName(player.keys.forward)))) keyToBind = &player.keys.forward;
                if (DrawButton(cx - 150, sy + 40, 300, 30, TextFormat("Backward: %s", GetCustomKeyName(player.keys.backward)))) keyToBind = &player.keys.backward;
                if (DrawButton(cx - 150, sy + 80, 300, 30, TextFormat("Left: %s", GetCustomKeyName(player.keys.left)))) keyToBind = &player.keys.left;
                if (DrawButton(cx - 150, sy + 120, 300, 30, TextFormat("Right: %s", GetCustomKeyName(player.keys.right)))) keyToBind = &player.keys.right;
                if (DrawButton(cx - 150, sy + 160, 300, 30, TextFormat("Jump: %s", GetCustomKeyName(player.keys.jump)))) keyToBind = &player.keys.jump;
                if (DrawButton(cx - 150, sy + 200, 300, 30, TextFormat("Sprint: %s", GetCustomKeyName(player.keys.sprint)))) keyToBind = &player.keys.sprint;
                if (DrawButton(cx - 150, sy + 240, 300, 30, TextFormat("Crouch: %s", GetCustomKeyName(player.keys.crouch)))) keyToBind = &player.keys.crouch;

                if (DrawButton(cx - 100, sy + 300, 200, 40, "Back")) { SaveGameInfo(player); gameState = STATE_MENU; }
            }
        }

        DrawFPS(10, 10);
        EndDrawing();
    }
    
    world.SaveAll(); 
    UnloadShader(voxelShader); 
    UnloadTexture(atlas);
    CloseWindow(); 
    return 0;
}