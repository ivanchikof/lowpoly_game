#include "World.h"
#include <cmath>
#include <cstdlib>

World::World() {
    fluidTimer = 0.0f; 
}

World::~World() {
    SaveAll(); 
    for(auto c : chunks) delete c;
    chunks.clear();
}

void World::SaveAll() {
    for(auto c : chunks) c->SaveToFile();
}

int World::GetBlock(int x, int y, int z) {
    if(y < 0 || y >= CHUNK_HEIGHT) return 0;
    int cx = floor((float)x / CHUNK_WIDTH); int cz = floor((float)z / CHUNK_WIDTH);
    for(auto c : chunks) {
        if(floor(c->pos.x / CHUNK_WIDTH) == cx && floor(c->pos.z / CHUNK_WIDTH) == cz) {
            int bx = x - cx * CHUNK_WIDTH; int bz = z - cz * CHUNK_WIDTH;
            return c->blocks[bx][y][bz];
        }
    }
    return 0;
}

void World::SetBlock(int x, int y, int z, int type) {
    if(y < 0 || y >= CHUNK_HEIGHT) return;
    int cx = floor((float)x / CHUNK_WIDTH); int cz = floor((float)z / CHUNK_WIDTH);
    for(auto c : chunks) {
        if(floor(c->pos.x / CHUNK_WIDTH) == cx && floor(c->pos.z / CHUNK_WIDTH) == cz) {
            int bx = x - cx * CHUNK_WIDTH; int bz = z - cz * CHUNK_WIDTH;
            c->blocks[bx][y][bz] = type;
            c->dirty = true;
            c->modified = true; 
            return;
        }
    }
}

void World::UpdateFluids(float dt) {
    fluidTimer += dt;
    if (fluidTimer < 0.2f) return; // Тік оновлення води кожні 0.2 секунди
    fluidTimer = 0.0f;

    auto getWaterLevel = [](int type) {
        if (type == 8) return 7;             // Максимальний рівень джерела
        if (type >= 11 && type <= 16) return 17 - type; // Поточні рівні течії від 6 до 1
        return 0;
    };

    auto getWaterType = [](int level) {
        if (level == 7) return 8;
        if (level >= 1 && level <= 6) return 17 - level;
        return 0;
    };

    struct FluidNode { int x, y, z; int type; };
    std::vector<FluidNode> fluids;

    for (auto c : chunks) {
        int cx = round(c->pos.x); int cz = round(c->pos.z);
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int y = 1; y < CHUNK_HEIGHT; y++) { 
                for (int z = 0; z < CHUNK_WIDTH; z++) { 
                    int type = c->blocks[x][y][z];
                    if (type == 8 || (type >= 11 && type <= 16) || type == 9) { 
                        fluids.push_back({x + cx, y, z + cz, type});
                    }
                }
            }
        }
    }

    int dx[] = {1, -1, 0, 0};
    int dz[] = {0, 0, 1, -1};

    for (const auto& f : fluids) {
        // Логіка для звичайної лави (густа, тече як раніше)
        if (f.type == 9) {
            int blockBelow = GetBlock(f.x, f.y - 1, f.z);
            if (blockBelow == 0) { SetBlock(f.x, f.y - 1, f.z, 9); continue; }
            if (GetRandomValue(0, 10) > 3) continue;
            for (int i = 0; i < 4; i++) {
                if (GetBlock(f.x + dx[i], f.y, f.z + dz[i]) == 0 && GetRandomValue(0, 15) == 0) {
                    SetBlock(f.x + dx[i], f.y, f.z + dz[i], 9);
                }
            }
            continue;
        }

        // --- РОЗУМНА ФІЗИКА ВОДИ З ДЕКАЄМ (ОБМЕЖЕННЯМ) ТА ВИСИХАННЯМ ---
        int currentLvl = getWaterLevel(f.type);

        if (f.type >= 11 && f.type <= 16) { 
            // Перевіряємо, чи є у поточної течії джерело живлення
            int aboveType = GetBlock(f.x, f.y + 1, f.z);
            bool fedFromAbove = (aboveType == 8 || (aboveType >= 11 && aboveType <= 16));
            
            int maxNeighborLvl = 0;
            for (int i = 0; i < 4; i++) {
                maxNeighborLvl = fmax(maxNeighborLvl, getWaterLevel(GetBlock(f.x + dx[i], f.y, f.z + dz[i])));
            }

            // Якщо джерело зверху закрили і поруч немає сильнішого блоку води - висихаємо
            if (!fedFromAbove && maxNeighborLvl <= currentLvl) {
                int nextLvl = currentLvl - 1;
                SetBlock(f.x, f.y, f.z, getWaterType(nextLvl));
                continue; 
            }
        }

        // Розтікання води
        int blockBelow = GetBlock(f.x, f.y - 1, f.z);
        if (blockBelow == 0 || (blockBelow >= 11 && blockBelow <= 16)) {
            // Водоспад: вниз вода падає завжди на повну силу течії
            if (blockBelow != 8) SetBlock(f.x, f.y - 1, f.z, 11); 
        } else {
            // Розповзання вбік з загасанням сили
            if (currentLvl > 1) {
                for (int i = 0; i < 4; i++) {
                    int nx = f.x + dx[i]; int nz = f.z + dz[i];
                    int neighborType = GetBlock(nx, f.y, nz);
                    
                    if (neighborType == 0) {
                        SetBlock(nx, f.y, nz, getWaterType(currentLvl - 1));
                    } else if (neighborType >= 11 && neighborType <= 16) {
                        // Якщо ми сильніші за сусідню течію, то підсилюємо її
                        if (getWaterLevel(neighborType) < currentLvl - 1) {
                            SetBlock(nx, f.y, nz, getWaterType(currentLvl - 1));
                        }
                    }
                }
            }
        }
    }
}

void World::Update(Vector3 playerPos) {
    int pCX = floor(playerPos.x / CHUNK_WIDTH);
    int pCZ = floor(playerPos.z / CHUNK_WIDTH);

    for (auto it = chunks.begin(); it != chunks.end();) {
        int cX = floor((*it)->pos.x / CHUNK_WIDTH);
        int cZ = floor((*it)->pos.z / CHUNK_WIDTH);

        if (abs(cX - pCX) > RENDER_DISTANCE || abs(cZ - pCZ) > RENDER_DISTANCE) {
            delete *it; 
            it = chunks.erase(it); 
        } else {
            ++it;
        }
    }

    for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE; x++) {
        for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE; z++) {
            int targetCX = pCX + x;
            int targetCZ = pCZ + z;
            bool exists = false;
            for (auto c : chunks) {
                if (floor(c->pos.x / CHUNK_WIDTH) == targetCX && floor(c->pos.z / CHUNK_WIDTH) == targetCZ) {
                    exists = true; break;
                }
            }
            if (!exists) chunks.push_back(new Chunk(targetCX, 0, targetCZ));
        }
    }
}