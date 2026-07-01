#pragma once
#include "Globals.h"
#include "Chunk.h"
#include <vector>

class World {
public:
    std::vector<Chunk*> chunks;
    float fluidTimer; 

    World();
    ~World();
    int GetBlock(int x, int y, int z);
    void SetBlock(int x, int y, int z, int type);
    void Update(Vector3 playerPos);
    void SaveAll();
    void UpdateFluids(float dt); 
};