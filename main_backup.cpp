#include "raylib.h"
#include <cmath>
#include <vector>

// --- КОНСТАНТИ СВІТУ ---
const int MAP_WIDTH = 32;
const int MAP_HEIGHT = 16;
const int MAP_LENGTH = 32;

char world[MAP_WIDTH][MAP_HEIGHT][MAP_LENGTH] = { 0 };

// --- СТРУКТУРИ ---
struct Vertex {
    Vector3 position;
    Vector2 texcoord;
    Vector3 normal;
    Color color;
};

// --- ФІЗИКА (AABB COLLISION) ---
bool IsColliding(Vector3 pos) {
    float minX = pos.x - 0.3f; float maxX = pos.x + 0.3f;
    float minY = pos.y - 1.5f; float maxY = pos.y + 0.1f;
    float minZ = pos.z - 0.3f; float maxZ = pos.z + 0.3f;

    int startX = (int)std::floor(minX + 0.5f); int endX = (int)std::floor(maxX + 0.5f);
    int startY = (int)std::floor(minY + 0.5f); int endY = (int)std::floor(maxY + 0.5f);
    int startZ = (int)std::floor(minZ + 0.5f); int endZ = (int)std::floor(maxZ + 0.5f);

    for (int x = startX; x <= endX; x++) {
        for (int y = startY; y <= endY; y++) {
            for (int z = startZ; z <= endZ; z++) {
                if (y < 0) return true;
                if (x < 0 || x >= MAP_WIDTH || z < 0 || z >= MAP_LENGTH) return true;
                if (world[x][y][z] != 0) return true;
            }
        }
    }
    return false;
}

// --- ГЕНЕРАЦІЯ АТЛАСУ ТЕКСТУР ---
Texture2D GenerateTextureAtlas() {
    Image img = GenImageColor(48, 16, BLACK);
    for(int x = 0; x < 48; x++) {
        for(int y = 0; y < 16; y++) {
            int blockType = x / 16;
            float noise = (float)(GetRandomValue(0, 20) - 10) / 255.0f;
            Color c;
            if (blockType == 0) c = (Color){ (unsigned char)(230 + noise*255), (unsigned char)(200 + noise*255), 120, 255 };
            else if (blockType == 1) c = (Color){ (unsigned char)(110 + noise*255), (unsigned char)(70 + noise*255), 40, 255 };
            else c = (Color){ 50, (unsigned char)(180 + noise*255), 50, 255 };
            ImageDrawPixel(&img, x, y, c);
        }
    }
    Texture2D tex = LoadTextureFromImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    UnloadImage(img);
    return tex;
}

// --- ГЕНЕРАЦІЯ МЕШУ ---
Mesh BuildWorldMesh() {
    std::vector<Vertex> vertices;
    std::vector<unsigned short> indices;
    unsigned short vertexCount = 0;

    for (int x = 0; x < MAP_WIDTH; x++) {
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int z = 0; z < MAP_LENGTH; z++) {
                int type = world[x][y][z];
                if (type == 0) continue;

                float uS = (type - 1) / 3.0f;
                float uE = type / 3.0f;

                auto addFace = [&](Vector3 p1, Vector3 p2, Vector3 p3, Vector3 p4, Vector3 n) {
                    vertices.push_back({p1, {uS, 1.0f}, n, WHITE});
                    vertices.push_back({p2, {uE, 1.0f}, n, WHITE});
                    vertices.push_back({p3, {uE, 0.0f}, n, WHITE});
                    vertices.push_back({p4, {uS, 0.0f}, n, WHITE});
                    indices.push_back(vertexCount + 0); indices.push_back(vertexCount + 1); indices.push_back(vertexCount + 2);
                    indices.push_back(vertexCount + 0); indices.push_back(vertexCount + 2); indices.push_back(vertexCount + 3);
                    vertexCount += 4;
                };

                float fx = (float)x, fy = (float)y, fz = (float)z;
                if (z == MAP_LENGTH - 1 || world[x][y][z+1] == 0) addFace({fx-0.5f, fy-0.5f, fz+0.5f}, {fx+0.5f, fy-0.5f, fz+0.5f}, {fx+0.5f, fy+0.5f, fz+0.5f}, {fx-0.5f, fy+0.5f, fz+0.5f}, {0,0,1});
                if (z == 0 || world[x][y][z-1] == 0) addFace({fx+0.5f, fy-0.5f, fz-0.5f}, {fx-0.5f, fy-0.5f, fz-0.5f}, {fx-0.5f, fy+0.5f, fz-0.5f}, {fx+0.5f, fy+0.5f, fz-0.5f}, {0,0,-1});
                if (y == MAP_HEIGHT - 1 || world[x][y+1][z] == 0) addFace({fx-0.5f, fy+0.5f, fz+0.5f}, {fx+0.5f, fy+0.5f, fz+0.5f}, {fx+0.5f, fy+0.5f, fz-0.5f}, {fx-0.5f, fy+0.5f, fz-0.5f}, {0,1,0});
                if (y == 0 || world[x][y-1][z] == 0) addFace({fx-0.5f, fy-0.5f, fz-0.5f}, {fx+0.5f, fy-0.5f, fz-0.5f}, {fx+0.5f, fy-0.5f, fz+0.5f}, {fx-0.5f, fy-0.5f, fz+0.5f}, {0,-1,0});
                if (x == MAP_WIDTH - 1 || world[x+1][y][z] == 0) addFace({fx+0.5f, fy-0.5f, fz+0.5f}, {fx+0.5f, fy-0.5f, fz-0.5f}, {fx+0.5f, fy+0.5f, fz-0.5f}, {fx+0.5f, fy+0.5f, fz+0.5f}, {1,0,0});
                if (x == 0 || world[x-1][y][z] == 0) addFace({fx-0.5f, fy-0.5f, fz-0.5f}, {fx-0.5f, fy-0.5f, fz+0.5f}, {fx-0.5f, fy+0.5f, fz+0.5f}, {fx-0.5f, fy+0.5f, fz-0.5f}, {-1,0,0});
            }
        }
    }

    Mesh mesh = { 0 };
    mesh.vertexCount = vertices.size();
    mesh.triangleCount = indices.size() / 3;
    mesh.vertices = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.texcoords = (float*)MemAlloc(mesh.vertexCount * 2 * sizeof(float));
    mesh.normals = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.indices = (unsigned short*)MemAlloc(indices.size() * sizeof(unsigned short));

    for (size_t i = 0; i < vertices.size(); i++) {
        mesh.vertices[i*3+0] = vertices[i].position.x; mesh.vertices[i*3+1] = vertices[i].position.y; mesh.vertices[i*3+2] = vertices[i].position.z;
        mesh.texcoords[i*2+0] = vertices[i].texcoord.x; mesh.texcoords[i*2+1] = vertices[i].texcoord.y;
        mesh.normals[i*3+0] = vertices[i].normal.x; mesh.normals[i*3+1] = vertices[i].normal.y; mesh.normals[i*3+2] = vertices[i].normal.z;
    }
    for (size_t i = 0; i < indices.size(); i++) mesh.indices[i] = indices[i];

    UploadMesh(&mesh, false);
    return mesh;
}

// --- MAIN ---
int main() {
    InitWindow(800, 450, "Voxel Engine - Full Version");
    Camera3D camera = { {16,12,16}, {17,12,16}, {0,1,0}, 60, CAMERA_PERSPECTIVE };
    DisableCursor(); SetTargetFPS(60);

    for(int x = 0; x < MAP_WIDTH; x++) {
        for(int z = 0; z < MAP_LENGTH; z++) {
            float wave = std::sin(x * 0.15f) + std::cos(z * 0.15f);
            int height = (int)((wave + 2.0f) * 1.5f) + 1;
            for(int y = 0; y < height; y++) world[x][y][z] = (y == 0) ? 1 : (y == height - 1) ? 3 : 2;
        }
    }

    Texture2D atlas = GenerateTextureAtlas();
    Mesh mesh = BuildWorldMesh();
    Model model = LoadModelFromMesh(mesh);
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = atlas;

    float velocityY = 0.0f; bool onGround = false, meshUpdate = true;
    while (!WindowShouldClose()) {
        if(meshUpdate) {
            UnloadModel(model);
            mesh = BuildWorldMesh();
            model = LoadModelFromMesh(mesh);
            model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = atlas;
            meshUpdate = false;
        }
        
        Vector3 oldPos = camera.position; UpdateCamera(&camera, CAMERA_FIRST_PERSON);
        Vector3 move = { camera.position.x - oldPos.x, 0, camera.position.z - oldPos.z };
        camera.position = oldPos;
        camera.position.x += move.x; if(IsColliding(camera.position)) camera.position.x = oldPos.x;
        camera.position.z += move.z; if(IsColliding(camera.position)) camera.position.z = oldPos.z;

        velocityY -= 0.012f;
        if(onGround && IsKeyPressed(KEY_SPACE)) { velocityY = 0.22f; onGround = false; }
        camera.position.y += velocityY;
        if(IsColliding(camera.position)) {
            if(velocityY < 0) onGround = true;
            camera.position.y = oldPos.y; velocityY = 0;
        } else {
            Vector3 below = camera.position; below.y -= 0.05f;
            onGround = IsColliding(below);
        }

        Vector3 fwd = { camera.target.x - camera.position.x, camera.target.y - camera.position.y, camera.target.z - camera.position.z };
        float len = sqrt(fwd.x*fwd.x + fwd.y*fwd.y + fwd.z*fwd.z); fwd.x/=len; fwd.y/=len; fwd.z/=len;
        
        int hX=-1, hY=-1, hZ=-1, pX=-1, pY=-1, pZ=-1; bool hit = false;
        for(float d=0; d<6; d+=0.1f) {
            int mx = (int)floor(camera.position.x + fwd.x*d + 0.5f);
            int my = (int)floor(camera.position.y + fwd.y*d + 0.5f);
            int mz = (int)floor(camera.position.z + fwd.z*d + 0.5f);
            if(mx>=0 && mx<MAP_WIDTH && my>=0 && my<MAP_HEIGHT && mz>=0 && mz<MAP_LENGTH) {
                if(world[mx][my][mz]!=0) { hit=true; hX=mx; hY=my; hZ=mz; break; }
                pX=mx; pY=my; pZ=mz;
            }
        }
        if(hit) {
            if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { world[hX][hY][hZ]=0; meshUpdate=true; }
            if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) { if(pX!=-1) { world[pX][pY][pZ]=3; meshUpdate=true; } }
        }

        BeginDrawing();
            ClearBackground(SKYBLUE);
            BeginMode3D(camera);
                DrawModel(model, {0,0,0}, 1, WHITE);
            EndMode3D();

            // ДОДАЙ ЦІ РЯДКИ:
            DrawCircle(GetScreenWidth() / 2, GetScreenHeight() / 2, 3, RED); // Приціл
            DrawText("Full Physics & Mesh Enabled", 10, 10, 20, DARKGRAY);   // Текст
            DrawFPS(10, 35);                                                // FPS для контролю
        EndDrawing();
    }
    UnloadTexture(atlas); UnloadModel(model); CloseWindow(); return 0;
}