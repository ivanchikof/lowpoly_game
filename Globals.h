#pragma once
#include "raylib.h"

const int CHUNK_WIDTH = 16;
const int CHUNK_HEIGHT = 64; 
const int RENDER_DISTANCE = 3; 

enum GameState { 
    STATE_PLAYING, 
    STATE_MENU, 
    STATE_SETTINGS_MAIN, 
    STATE_SETTINGS_GRAPHICS, 
    STATE_SETTINGS_CONTROLS 
};

struct Vertex { 
    Vector3 position; 
    Vector2 texcoord; 
    Vector3 normal; 
    Color color; 
};