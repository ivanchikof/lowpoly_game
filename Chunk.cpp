#include "Chunk.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include "FastNoiseLite.h"

Chunk::Chunk(int cx, int cy, int cz) {
    pos = {(float)cx * CHUNK_WIDTH, 0, (float)cz * CHUNK_WIDTH};
    dirty = true;
    modified = false;
    mesh = {0};
    model = {0};

    if (LoadFromFile()) return; 

    for(int x=0; x<CHUNK_WIDTH; x++) for(int y=0; y<CHUNK_HEIGHT; y++) for(int z=0; z<CHUNK_WIDTH; z++) blocks[x][y][z] = 0;

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise.SetSeed(1337); 
    noise.SetFrequency(0.03f);
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);

    int WATER_LEVEL = 20;

    for(int x=0; x<CHUNK_WIDTH; x++) {
        for(int z=0; z<CHUNK_WIDTH; z++) {
            float worldX = x + pos.x; float worldZ = z + pos.z;
            float noiseVal = noise.GetNoise(worldX, worldZ);
            int h = 15 + (int)((noiseVal + 1.0f) * 0.5f * 20.0f);
            if (h >= CHUNK_HEIGHT) h = CHUNK_HEIGHT - 1;

            for(int y=0; y<CHUNK_HEIGHT; y++) {
                if (y == 0) blocks[x][y][z] = 10;
                else if (y < h) {
                    if (y == h - 1) {
                        blocks[x][y][z] = (h <= WATER_LEVEL) ? 2 : 1;
                        if (h <= WATER_LEVEL + 2 && blocks[x][y][z] == 1) blocks[x][y][z] = 4;
                    } else blocks[x][y][z] = (y > h-5) ? 2 : 3;
                } else if (y <= WATER_LEVEL) blocks[x][y][z] = 8;
            }
        }
    }

    for(int x=2; x<CHUNK_WIDTH-2; x++) {
        for(int z=2; z<CHUNK_WIDTH-2; z++) {
            for(int y=WATER_LEVEL+1; y<CHUNK_HEIGHT-6; y++) {
                if (blocks[x][y][z] == 1 && blocks[x][y+1][z] == 0) { 
                    if (GetRandomValue(0, 100) < 2) { 
                        blocks[x][y+1][z] = 5; blocks[x][y+2][z] = 5; 
                        blocks[x][y+3][z] = 5; blocks[x][y+4][z] = 5;
                        for(int lx=-2; lx<=2; lx++) {
                            for(int lz=-2; lz<=2; lz++) {
                                for(int ly=3; ly<=5; ly++) {
                                    if (abs(lx) == 2 && abs(lz) == 2 && ly == 5) continue; 
                                    if (blocks[x+lx][y+ly][z+lz] == 0) blocks[x+lx][y+ly][z+lz] = 7;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void Chunk::SaveToFile() {
    if (!modified) return; 
    int cx = (int)(pos.x / CHUNK_WIDTH);
    int cz = (int)(pos.z / CHUNK_WIDTH);
    std::ofstream file(TextFormat("chunk_%d_%d.bin", cx, cz), std::ios::binary);
    if (file.is_open()) {
        file.write((char*)blocks, sizeof(blocks));
        modified = false; 
    }
}

bool Chunk::LoadFromFile() {
    int cx = (int)(pos.x / CHUNK_WIDTH);
    int cz = (int)(pos.z / CHUNK_WIDTH);
    std::ifstream file(TextFormat("chunk_%d_%d.bin", cx, cz), std::ios::binary);
    if (file.is_open()) {
        file.read((char*)blocks, sizeof(blocks));
        return true;
    }
    return false;
}

Chunk::~Chunk() {
    SaveToFile(); 
    if (model.meshCount > 0) UnloadModel(model);
}

void Chunk::BuildMesh() {
    if (model.meshCount > 0) UnloadModel(model); 

    // Допоміжні лямбди для точного визначення сили течії води
    auto getWaterLevel = [](int type) {
        if (type == 8) return 7;
        if (type >= 11 && type <= 16) return 17 - type;
        return 0;
    };

    // НОВА РОЗУМНА СИСТЕМА ПЕРЕВІРКИ ГРАНЕЙ
    auto shouldDrawFace = [&](int bType, int nType, bool isSide) {
        if (nType == 0) return true; // Повітря — завжди малюємо

        bool isCurrentWater = (bType == 8 || (bType >= 11 && bType <= 16));
        bool isNeighborWater = (nType == 8 || (nType >= 11 && nType <= 16));

        // Якщо обидва блоки є водою
        if (isCurrentWater && isNeighborWater) {
            if (isSide) {
                // ВИПРАВЛЕННЯ: Малюємо бічну стінку сходинки, тільки якщо наша вода вища за сусідню!
                return getWaterLevel(bType) > getWaterLevel(nType);
            }
            return false; // По вертикалі стінки всередині води не потрібні
        }

        // Якщо наша вода торкається іншого блоку
        if (isCurrentWater && !isNeighborWater) {
            if (nType == 7) return true; // Листя прозоре — малюємо стінку води
            return false; // Тверді блоки повністю закривають воду — ховаємо стінку
        }

        // Логіка для звичайних твердих блоків (малюємо, якщо сусід прозорий)
        if (nType == 7 || nType == 8 || (nType >= 11 && nType <= 16)) return true;
        return false;
    };

    std::vector<Vertex> v; std::vector<unsigned short> i; unsigned short count = 0;
    for(int x=0; x<CHUNK_WIDTH; x++) for(int y=0; y<CHUNK_HEIGHT; y++) for(int z=0; z<CHUNK_WIDTH; z++) {
        if(blocks[x][y][z] == 0) continue;
        
        auto addFace = [&](Vector3 p1, Vector3 p2, Vector3 p3, Vector3 p4, Vector3 n, int texIndex, Color faceTint) {
            float uS = texIndex / 10.0f, uE = (texIndex + 1) / 10.0f;
            v.push_back({p1, {uS, 1.0f}, n, faceTint}); v.push_back({p2, {uE, 1.0f}, n, faceTint});
            v.push_back({p3, {uE, 0.0f}, n, faceTint}); v.push_back({p4, {uS, 0.0f}, n, faceTint});
            i.push_back(count+0); i.push_back(count+1); i.push_back(count+2);
            i.push_back(count+0); i.push_back(count+2); i.push_back(count+3);
            count+=4;
        };

        float fx = x+pos.x, fy = y+pos.y, fz = z+pos.z;
        int bType = blocks[x][y][z];
        int texTop = 0, texBottom = 0, texSide = 0;
        Color baseTint = WHITE; 
        
        if (bType == 1) { texSide = 0; texTop = 1; texBottom = 2; } 
        else if (bType == 2) { texSide = 2; texTop = 2; texBottom = 2; } 
        else if (bType == 3) { texSide = 3; texTop = 3; texBottom = 3; } 
        else if (bType == 4) { texSide = 4; texTop = 4; texBottom = 4; } 
        else if (bType == 5) { texSide = 5; texTop = 6; texBottom = 6; } 
        else if (bType == 7) { texSide = 7; texTop = 7; texBottom = 7; } 
        else if (bType == 8 || (bType >= 11 && bType <= 16)) { texSide = 8; texTop = 8; texBottom = 8; } 
        else if (bType == 10) { texSide = 3; texTop = 3; texBottom = 3; baseTint = {30, 20, 40, 255}; }

        // Розрахунок динамічної висоти
        float topY = fy + 0.5f;
        if (bType == 8 || (bType >= 11 && bType <= 16)) {
            int level = getWaterLevel(bType);
            topY = fy - 0.5f + (level / 7.0f); 
        }

        // Логіка тіней
        bool inShadow = false;
        for (int iy = y + 1; iy < CHUNK_HEIGHT; iy++) {
            int above = blocks[x][iy][z];
            if (above != 0 && above != 8 && !(above >= 11 && above <= 16)) { 
                inShadow = true;
                break;
            }
        }

        float shadowMult = inShadow ? 0.45f : 1.0f;
        Color tintTop = { (unsigned char)(baseTint.r * 1.0f * shadowMult), (unsigned char)(baseTint.g * 1.0f * shadowMult), (unsigned char)(baseTint.b * 1.0f * shadowMult), baseTint.a };
        Color tintZ =   { (unsigned char)(baseTint.r * 0.8f * shadowMult), (unsigned char)(baseTint.g * 0.8f * shadowMult), (unsigned char)(baseTint.b * 0.8f * shadowMult), baseTint.a };
        Color tintX =   { (unsigned char)(baseTint.r * 0.6f * shadowMult), (unsigned char)(baseTint.g * 0.6f * shadowMult), (unsigned char)(baseTint.b * 0.6f * shadowMult), baseTint.a };
        Color tintBot = { (unsigned char)(baseTint.r * 0.5f * shadowMult), (unsigned char)(baseTint.g * 0.5f * shadowMult), (unsigned char)(baseTint.b * 0.5f * shadowMult), baseTint.a };

        // ЗАСТОСОВУЄМО НАШУ НОВУ ФУНКЦІЮ ДЛЯ КОЖНОЇ З 6 ГРАНЕЙ КУБА
        if(z==CHUNK_WIDTH-1 || shouldDrawFace(bType, blocks[x][y][z+1], true)) addFace({fx-0.5f, fy-0.5f, fz+0.5f}, {fx+0.5f, fy-0.5f, fz+0.5f}, {fx+0.5f, topY, fz+0.5f}, {fx-0.5f, topY, fz+0.5f}, {0,0,1}, texSide, tintZ);
        if(z==0 || shouldDrawFace(bType, blocks[x][y][z-1], true)) addFace({fx+0.5f, fy-0.5f, fz-0.5f}, {fx-0.5f, fy-0.5f, fz-0.5f}, {fx-0.5f, topY, fz-0.5f}, {fx+0.5f, topY, fz-0.5f}, {0,0,-1}, texSide, tintZ);
        if(y==CHUNK_HEIGHT-1 || shouldDrawFace(bType, blocks[x][y+1][z], false)) addFace({fx-0.5f, topY, fz+0.5f}, {fx+0.5f, topY, fz+0.5f}, {fx+0.5f, topY, fz-0.5f}, {fx-0.5f, topY, fz-0.5f}, {0,1,0}, texTop, tintTop);
        if(y==0 || shouldDrawFace(bType, blocks[x][y-1][z], false)) addFace({fx-0.5f, fy-0.5f, fz-0.5f}, {fx+0.5f, fy-0.5f, fz-0.5f}, {fx+0.5f, fy-0.5f, fz+0.5f}, {fx-0.5f, fy-0.5f, fz+0.5f}, {0,-1,0}, texBottom, tintBot);
        if(x==CHUNK_WIDTH-1 || shouldDrawFace(bType, blocks[x+1][y][z], true)) addFace({fx+0.5f, fy-0.5f, fz+0.5f}, {fx+0.5f, fy-0.5f, fz-0.5f}, {fx+0.5f, topY, fz-0.5f}, {fx+0.5f, topY, fz+0.5f}, {1,0,0}, texSide, tintX);
        if(x==0 || shouldDrawFace(bType, blocks[x-1][y][z], true)) addFace({fx-0.5f, fy-0.5f, fz-0.5f}, {fx-0.5f, fy-0.5f, fz+0.5f}, {fx-0.5f, topY, fz+0.5f}, {fx-0.5f, topY, fz-0.5f}, {-1,0,0}, texSide, tintX);
    }
    if (v.empty()) { dirty = false; return; }

    mesh = {0}; mesh.vertexCount = v.size(); mesh.triangleCount = i.size()/3;
    mesh.vertices = (float*)MemAlloc(mesh.vertexCount*3*sizeof(float));
    mesh.texcoords = (float*)MemAlloc(mesh.vertexCount*2*sizeof(float));
    mesh.normals = (float*)MemAlloc(mesh.vertexCount*3*sizeof(float));
    mesh.colors = (unsigned char*)MemAlloc(mesh.vertexCount*4*sizeof(unsigned char));
    mesh.indices = (unsigned short*)MemAlloc(i.size()*sizeof(unsigned short));
    
    for(size_t k=0; k<v.size(); k++) {
        mesh.vertices[k*3+0]=v[k].position.x; mesh.vertices[k*3+1]=v[k].position.y; mesh.vertices[k*3+2]=v[k].position.z;
        mesh.texcoords[k*2+0]=v[k].texcoord.x; mesh.texcoords[k*2+1]=v[k].texcoord.y;
        mesh.normals[k*3+0]=v[k].normal.x; mesh.normals[k*3+1]=v[k].normal.y; mesh.normals[k*3+2]=v[k].normal.z;
        mesh.colors[k*4+0]=v[k].color.r; mesh.colors[k*4+1]=v[k].color.g; mesh.colors[k*4+2]=v[k].color.b; mesh.colors[k*4+3]=v[k].color.a;
    }
    for(size_t k=0; k<i.size(); k++) mesh.indices[k] = i[k];
    UploadMesh(&mesh, false);
    model = LoadModelFromMesh(mesh);
    dirty = false;
}