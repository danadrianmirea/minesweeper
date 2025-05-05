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

    void Draw(float dt);
    void DrawUI();
    std::string FormatWithLeadingZeroes(int number, int width);
    void Randomize();
    void ResetToInitialSize();  // Reset grid size to initial size

    static bool isMobile;

#ifdef DEBUG
    void InitializeDebugGrid();  // Initialize grid with predefined mine pattern for debugging
#endif

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
    bool isHelpMenuOpen;
    Rectangle fileMenuRect;
    Rectangle newGameOptionRect;
    Rectangle customGameOptionRect;  // New option for custom game
    Rectangle quitOptionRect;
    Rectangle helpMenuRect;
    Rectangle aboutOptionRect;
    Rectangle popupRect;
    Rectangle okButtonRect;
    bool showHelpPopup;
    bool showCustomGamePopup;  // New flag for custom game popup
    bool showSavePopup;        // New flag for save popup
    bool showLoadPopup;        // New flag for load popup
    bool showWelcomePopup;     // Flag for welcome popup
    char customGridSizeInput[32];  // Buffer for custom grid size input
    char filenameInput[256];       // Buffer for filename input
    int customGridSizeInputLength;  // Track input length
    int filenameInputLength;        // Track filename input length

    // Mobile tap tracking
    bool isTapping;
    float tapStartTime;
    Vector2 tapStartPos;
    int tapRow;
    int tapCol;
    bool longTapPerformed;  // Track if long tap action has been performed

    void InitializeGrid();
    void PlaceMines();
    void CalculateAdjacentMines();
    void RevealCell(int row, int col);
    void RevealAllMines();
    void RevealNeighboringMines(int row, int col);  // Reveal mines adjacent to a cell
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
    float gameOverTextTimer;  // Timer for game over text display
    bool waitingForNextLevel;  // Track if we're waiting for player input after winning
    bool waitingForGameOver;  // Track if we're waiting for player input after losing

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
    Texture2D backgroundTexture;  // Background texture

    int currentGridSize;  // Track current grid size
    static const int DESKTOP_INITIAL_GRID_SIZE = 5;  // Starting grid size for desktop
    static const int MOBILE_INITIAL_GRID_SIZE = 3;   // Starting grid size for mobile
    static const int DESKTOP_MAX_GRID_SIZE = 20;     // Maximum grid size for desktop
    static const int MOBILE_MAX_GRID_SIZE = 8;       // Maximum grid size for mobile
    int CalculateMineCount() const;  // Calculate mines based on grid size

    // Mobile tap constants
    static const float LONG_TAP_THRESHOLD;  // Time in seconds for long tap

    // Save/Load functions
    bool SaveGame(const std::string& filename);
    bool LoadGame(const std::string& filename);
};