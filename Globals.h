#pragma once
#include "raylib.h"

const int CHUNK_WIDTH = 16;
const int CHUNK_HEIGHT = 64; 
const int RENDER_DISTANCE = 3;
const std::string GAME_VERSION = "v1.0.0";

enum GameState { 
    STATE_PLAYING, 
    STATE_MENU, 
    STATE_SETTINGS_MAIN, 
    STATE_SETTINGS_GRAPHICS, 
    STATE_SETTINGS_CONTROLS,
    STATE_CHANGELOG,
    STATE_UPDATE_PROMPT
};

struct Vertex { 
    Vector3 position; 
    Vector2 texcoord; 
    Vector3 normal; 
    Color color; 
};