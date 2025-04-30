#pragma once

#include "raylib.h"
#include "globals.h"
#include <vector>
#include <random>

class Game
{
public:
    Game(int screenWidth, int screenHeight);
    ~Game();
    void Update(float dt);
    void HandleInput();
    void UpdateUI();

    void Draw();
    void DrawUI();
    std::string FormatWithLeadingZeroes(int number, int width);
    void Randomize();

    static bool isMobile;

private:
    struct Cell {
        bool hasMine;
        CellState state;
        int adjacentMines;
    };

    // Menu related
    void DrawMenuBar();
    bool HandleMenuInput();
    bool isMenuBarHovered;
    bool isFileMenuOpen;
    Rectangle fileMenuRect;
    Rectangle newGameOptionRect;
    Rectangle quitOptionRect;

    void InitializeGrid();
    void PlaceMines();
    void CalculateAdjacentMines();
    void RevealCell(int row, int col);
    void RevealAllMines();
    void RevealAdjacentCells(int row, int col);
    bool IsValidCell(int row, int col) const;
    void CheckWinCondition();
    void DrawGrid() const;
    void DrawCell(int row, int col) const;
    void UpdateScaling();
    void LoadTextures();
    void UnloadTextures();

    bool gameOver;
    bool gameWon;

    float screenScale;
    RenderTexture2D targetRenderTex;
    Font font;

    // Game stats
    float gameTime;
    int remainingMines;

    int screenWidth;
    int screenHeight;
    std::vector<std::vector<Cell>> grid;
    int remainingCells;

    // Scaling related
    float cellSize;
    float scale;
    Vector2 gridOffset;

    // Textures
    Texture2D bombTexture;
    Texture2D flagTexture;
    Texture2D numberTextures[8]; // 1-8
};