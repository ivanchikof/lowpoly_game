#pragma once
#include "Globals.h"
#include <vector>

class Chunk {
public:
    char blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_WIDTH]; 
    Mesh mesh;
    Model model;
    Vector3 pos;
    bool dirty;
    bool modified; // Вказує, чи змінював гравець цей чанк

    Chunk(int cx, int cy, int cz);
    ~Chunk();
    void BuildMesh();
    
    bool LoadFromFile(); // Завантаження з диску
    void SaveToFile();   // Збереження на диск
};