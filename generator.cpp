#include "raylib.h"
#include <cmath> // Додано цю бібліотеку для математичних функцій!

// Функція для малювання шуму
void DrawNoise(Image* img, int startX, Color c1, Color c2, int intensity) {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            ImageDrawPixel(img, startX + x, y, GetRandomValue(0, 100) < intensity ? c1 : c2);
        }
    }
}

int main() {
    InitWindow(100, 100, "Texture Atlas Generator");
    
    // Створюємо атлас на 10 текстур (160 пікселів в ширину)
    Image atlas = GenImageColor(160, 16, BLANK);

    // Палітри кольорів
    Color dirt = {122, 89, 67, 255};      Color dirtDark = {105, 75, 56, 255};
    Color grass = {100, 160, 75, 255};    Color grassLight = {115, 175, 85, 255};
    Color stone = {140, 140, 140, 255};   Color stoneDark = {125, 125, 125, 255};
    Color sand = {218, 210, 160, 255};    Color sandDark = {205, 195, 145, 255};
    Color woodSide = {87, 65, 47, 255};   Color woodLine = {66, 48, 33, 255};
    Color woodTop = {173, 140, 104, 255}; Color woodRing = {145, 114, 82, 255};
    Color leaves = {60, 130, 60, 255};    Color leavesDark = {45, 105, 45, 255};
    Color water = {50, 100, 200, 200};    Color waterLight = {70, 130, 220, 200};
    Color lava = {220, 80, 20, 255};      Color lavaYellow = {240, 160, 30, 255};

    // 0: Трава (Збоку)
    DrawNoise(&atlas, 0, dirt, dirtDark, 50);
    for(int x = 0; x < 16; x++) {
        int drop = 3 + (GetRandomValue(0, 2));
        for(int y = 0; y < drop; y++) ImageDrawPixel(&atlas, x, y, GetRandomValue(0,10)<7 ? grass : grassLight);
    }
    // 1: Трава (Зверху)
    DrawNoise(&atlas, 16, grass, grassLight, 50);
    // 2: Земля
    DrawNoise(&atlas, 32, dirt, dirtDark, 50);
    // 3: Камінь
    DrawNoise(&atlas, 48, stone, stoneDark, 40);
    // 4: Пісок
    DrawNoise(&atlas, 64, sand, sandDark, 30);
    // 5: Дерево (Стовбур збоку)
    for(int y=0; y<16; y++) for(int x=0; x<16; x++) ImageDrawPixel(&atlas, 80 + x, y, (x%4==0 || x%5==0) ? woodLine : woodSide);
    // 6: Дерево (Зріз зверху)
    for(int y=0; y<16; y++) for(int x=0; x<16; x++) {
        float dist = std::sqrt(std::pow(x-7.5f, 2) + std::pow(y-7.5f, 2));
        ImageDrawPixel(&atlas, 96 + x, y, ((int)dist % 3 == 0) ? woodRing : woodTop);
    }
    // 7: Листя
    for(int y=0; y<16; y++) for(int x=0; x<16; x++) {
        if(GetRandomValue(0, 100) < 15) ImageDrawPixel(&atlas, 112 + x, y, BLANK);
        else ImageDrawPixel(&atlas, 112 + x, y, GetRandomValue(0, 10) < 5 ? leaves : leavesDark);
    }
    // 8: Вода
    for(int y=0; y<16; y++) for(int x=0; x<16; x++) ImageDrawPixel(&atlas, 128 + x, y, (y%4==0 || (y+x)%6==0) ? waterLight : water);
    // 9: Лава
    DrawNoise(&atlas, 144, lava, lavaYellow, 30);

    ExportImage(atlas, "atlas.png");
    UnloadImage(atlas);
    CloseWindow();
    return 0;
}