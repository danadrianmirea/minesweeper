#include <vector>
#include <utility>
#include <string>
#include <cmath>  // For sqrtf
#include <iostream>

#include "raylib.h"
#include "globals.h"
#include "game.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

bool Game::isMobile = false;

Game::Game(int screenWidth, int screenHeight)
    : screenWidth(screenWidth), screenHeight(screenHeight), gameOver(false), gameWon(false),
      isMenuBarHovered(false), isFileMenuOpen(false), isHelpMenuOpen(false), showHelpPopup(false),
      gameTime(0.0f), remainingMines(NUM_MINES)
{
#ifdef __EMSCRIPTEN__
    // Check if we're running on a mobile device
    isMobile = EM_ASM_INT({
        return /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent);
    });
#endif
    screenScale = MIN((float)GetScreenWidth() / gameScreenWidth, (float)GetScreenHeight() / gameScreenHeight);
    targetRenderTex = LoadRenderTexture(gameScreenWidth, gameScreenHeight);
    SetTextureFilter(targetRenderTex.texture, TEXTURE_FILTER_BILINEAR); // Texture scale filter to use
    font = LoadFontEx("Font/monogram.ttf", 64, 0, 0);    
    LoadTextures();
    InitializeGrid();
    Randomize();
    UpdateScaling();
}

Game::~Game()
{
    UnloadTextures();
    UnloadRenderTexture(targetRenderTex);
    UnloadFont(font);
}

void Game::Update(float dt)
{
    UpdateUI();
    bool menuHandledClick = HandleMenuInput();

    // If help popup is shown, ignore all game input
    if (showHelpPopup) {
        return;
    }

    // Update game time if game is not over
    if (!gameOver && !gameWon) {
        gameTime += dt;
    }

    // Update remaining mines count
    int flaggedCount = 0;
    for (int row = 0; row < GRID_SIZE; ++row) {
        for (int col = 0; col < GRID_SIZE; ++col) {
            if (grid[row][col].state == CellState::FLAGGED) {
                flaggedCount++;
            }
        }
    }
    remainingMines = NUM_MINES - flaggedCount;

    // Update scaling if window size changed
    if (IsWindowResized()) {
        UpdateScaling();
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !menuHandledClick) {
        Vector2 mousePos = GetMousePosition();
        
        // Convert screen coordinates to game coordinates
        float gameX = (mousePos.x - (GetScreenWidth() - (gameScreenWidth * scale)) * 0.5f) / scale;
        float gameY = (mousePos.y - (GetScreenHeight() - (gameScreenHeight * scale)) * 0.5f) / scale;
        
        // Skip click handling if in menu bar area
        if (gameY < 30) return;
        
        // Calculate grid position
        int col = (gameX - gridOffset.x) / cellSize;
        int row = (gameY - gridOffset.y) / cellSize;

        // Check if click is within the game grid
        bool isInGrid = (gameX >= gridOffset.x && gameX < gridOffset.x + GRID_SIZE * cellSize &&
                        gameY >= gridOffset.y && gameY < gridOffset.y + GRID_SIZE * cellSize);

        if (gameOver) {
            // If game is over, any click in the game area starts a new game
            if (isInGrid) {
                Randomize();
            }
            return;
        }

        if (IsValidCell(row, col) && grid[row][col].state == CellState::HIDDEN) {
            RevealCell(row, col);
        }
        else if (IsValidCell(row, col) && grid[row][col].state == CellState::REVEALED && grid[row][col].adjacentMines > 0) {
            RevealAdjacentCells(row, col);
        }
    }
    else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && !menuHandledClick) {
        if (gameOver) return;

        Vector2 mousePos = GetMousePosition();
        
        // Convert screen coordinates to game coordinates
        float gameX = (mousePos.x - (GetScreenWidth() - (gameScreenWidth * scale)) * 0.5f) / scale;
        float gameY = (mousePos.y - (GetScreenHeight() - (gameScreenHeight * scale)) * 0.5f) / scale;
        
        // Skip click handling if in menu bar area
        if (gameY < 30) return;
        
        // Calculate grid position
        int col = (gameX - gridOffset.x) / cellSize;
        int row = (gameY - gridOffset.y) / cellSize;

        if (IsValidCell(row, col)) {
            if (grid[row][col].state == CellState::HIDDEN) {
                grid[row][col].state = CellState::FLAGGED;
            }
            else if (grid[row][col].state == CellState::FLAGGED) {
                grid[row][col].state = CellState::HIDDEN;
            }
        }
    }
}

void Game::UpdateUI()
{
#ifndef EMSCRIPTEN_BUILD
    if (IsKeyPressed(KEY_ENTER) && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)))
    {
        if (fullscreen)
        {
            fullscreen = false;
            ToggleBorderlessWindowed();
        }
        else
        {
            fullscreen = true;
            ToggleBorderlessWindowed();
        }
    }
#endif
}

void Game::DrawUI() {
    DrawMenuBar();

    // Draw game stats
    const int padding = 10;
    const int fontSize = 20;
    const int statsHeight = 30;
    
    // Draw remaining mines
    std::string minesText = "Mines: " + std::to_string(remainingMines);
    DrawText(minesText.c_str(), padding, 35, fontSize, BLACK);
    
    // Draw timer
    std::string timeText = "Timer: " + std::to_string((int)gameTime);
    int timeTextWidth = MeasureText(timeText.c_str(), fontSize);
    DrawText(timeText.c_str(), gameScreenWidth - timeTextWidth - padding, 35, fontSize, BLACK);

    // Draw help popup if active
    if (showHelpPopup)
    {
        // Draw semi-transparent background
        DrawRectangle(0, 0, gameScreenWidth, gameScreenHeight, (Color){0, 0, 0, 128});
        
        // Draw popup background
        const int popupWidth = 500;  // Increased from 400
        const int popupHeight = 400; // Increased from 300
        popupRect = {(float)(gameScreenWidth - popupWidth) / 2, (float)(gameScreenHeight - popupHeight) / 2,
                    (float)popupWidth, (float)popupHeight};
        DrawRectangleRec(popupRect, LIGHTGRAY);
        
        // Draw popup title
        const char* title = "How to Play Minesweeper";
        int titleWidth = MeasureText(title, 24);
        DrawText(title, popupRect.x + (popupWidth - titleWidth) / 2, popupRect.y + 30, 24, BLACK);
        
        // Draw instructions
        const char* instructions[] = {
            "1. Left-click to reveal a cell",
            "2. Right-click to place/remove a flag",
            "3. Numbers show how many mines are adjacent",
            "4. Flag all mines to win",
            "5. Clicking a mine ends the game",
            "6. Click both left+right on a number to reveal",
            "   adjacent cells if correct flags are placed"
        };
        
        int lineHeight = 35;  // Increased from 30
        for (int i = 0; i < 7; i++)
        {
            DrawText(instructions[i], popupRect.x + 30, popupRect.y + 80 + i * lineHeight, 20, BLACK);
        }
        
        // Draw OK button
        const char* okText = "OK";
        int okTextWidth = MeasureText(okText, 20);
        okButtonRect = {popupRect.x + (popupWidth - 100) / 2, popupRect.y + popupHeight - 60, 100, 30};
        DrawRectangleRec(okButtonRect, GRAY);
        DrawText(okText, okButtonRect.x + (okButtonRect.width - okTextWidth) / 2, 
                okButtonRect.y + 5, 20, BLACK);
    }
}

void Game::Draw()
{
    // Update scale based on current window size
    scale = MIN((float)GetScreenWidth() / gameScreenWidth, (float)GetScreenHeight() / gameScreenHeight);
    
    // Render to texture
    BeginTextureMode(targetRenderTex);
    ClearBackground(RAYWHITE);
    

    
    DrawGrid();
    
    // Draw game state message
    if (gameWon) {
        const char* text = "You Won!";
        int fontSize = 40;
        int textWidth = MeasureText(text, fontSize);
        int padding = 20;
        int rectWidth = textWidth + padding * 2;
        int rectHeight = fontSize + padding * 2;
        int rectX = (gameScreenWidth - rectWidth) / 2;
        int rectY = (gameScreenHeight - rectHeight) / 2;
        
        // Draw rounded rectangle background
        DrawRectangleRounded((Rectangle){(float)rectX, (float)rectY, (float)rectWidth, (float)rectHeight}, 0.3f, 8, BLACK);
        // Draw text
        DrawText(text, (gameScreenWidth - textWidth) / 2, gameScreenHeight / 2 - fontSize / 2, fontSize, WHITE);
    }
    else if (gameOver) {
        const char* text = "Game Over!";
        int fontSize = 40;
        int textWidth = MeasureText(text, fontSize);
        int padding = 20;
        int rectWidth = textWidth + padding * 2;
        int rectHeight = fontSize + padding * 2;
        int rectX = (gameScreenWidth - rectWidth) / 2;
        int rectY = (gameScreenHeight - rectHeight) / 2;
        
        // Draw rounded rectangle background
        DrawRectangleRounded((Rectangle){(float)rectX, (float)rectY, (float)rectWidth, (float)rectHeight}, 0.3f, 8, BLACK);
        // Draw text
        DrawText(text, (gameScreenWidth - textWidth) / 2, gameScreenHeight / 2 - fontSize / 2, fontSize, WHITE);
    }

    DrawUI();
    

    EndTextureMode();
 
    // Draw the scaled texture to screen
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(targetRenderTex.texture, 
        (Rectangle){0.0f, 0.0f, (float)targetRenderTex.texture.width, (float)-targetRenderTex.texture.height},
        (Rectangle){(GetScreenWidth() - (gameScreenWidth * scale)) * 0.5f, 
                   (GetScreenHeight() - (gameScreenHeight * scale)) * 0.5f,
                   gameScreenWidth * scale, gameScreenHeight * scale},
        (Vector2){0, 0}, 0.0f, WHITE);
    EndDrawing();
}

void Game::DrawMenuBar()
{
    // Draw menu bar background
    DrawRectangle(0, 0, gameScreenWidth, 30, LIGHTGRAY);
    
    // Draw File menu
    const char* fileText = "File";
    int textWidth = MeasureText(fileText, 20);
    fileMenuRect = {110, 5, (float)textWidth + 20, 20};
    
    // Draw File menu button
    Color fileButtonColor = isFileMenuOpen ? GRAY : LIGHTGRAY;
    DrawRectangleRec(fileMenuRect, fileButtonColor);
    DrawText(fileText, 120, 5, 20, BLACK);
    
    // Draw File menu dropdown if open
    if (isFileMenuOpen)
    {
        const char* newGameText = "New Game";
        const char* quitText = "Quit";
        int newGameTextWidth = MeasureText(newGameText, 20);
        int quitTextWidth = MeasureText(quitText, 20);
        float menuWidth = (float)MAX(newGameTextWidth, quitTextWidth) + 20;
        
        // Draw New Game option
        newGameOptionRect = {fileMenuRect.x, fileMenuRect.y + fileMenuRect.height, 
                           menuWidth, 25};
        DrawRectangleRec(newGameOptionRect, LIGHTGRAY);
        DrawText(newGameText, newGameOptionRect.x + 10, newGameOptionRect.y + 2, 20, BLACK);
        
        // Draw Quit option
        quitOptionRect = {fileMenuRect.x, newGameOptionRect.y + newGameOptionRect.height,
                         menuWidth, 25};
        DrawRectangleRec(quitOptionRect, LIGHTGRAY);
        DrawText(quitText, quitOptionRect.x + 10, quitOptionRect.y + 2, 20, BLACK);
    }

    // Draw Help menu
    const char* helpText = "Help";
    int helpTextWidth = MeasureText(helpText, 20);
    helpMenuRect = {fileMenuRect.x + fileMenuRect.width + 20, 5, (float)helpTextWidth + 20, 20};
    
    // Draw Help menu button
    Color helpButtonColor = isHelpMenuOpen ? GRAY : LIGHTGRAY;
    DrawRectangleRec(helpMenuRect, helpButtonColor);
    DrawText(helpText, helpMenuRect.x + 10, 5, 20, BLACK);
    
    // Draw Help menu dropdown if open
    if (isHelpMenuOpen)
    {
        const char* aboutText = "About";
        int aboutTextWidth = MeasureText(aboutText, 20);
        aboutOptionRect = {helpMenuRect.x, helpMenuRect.y + helpMenuRect.height,
                          (float)aboutTextWidth + 20, 25};
        DrawRectangleRec(aboutOptionRect, LIGHTGRAY);
        DrawText(aboutText, aboutOptionRect.x + 10, aboutOptionRect.y + 2, 20, BLACK);
    }
}

bool Game::HandleMenuInput()
{
    Vector2 mousePos = GetMousePosition();
    
    // Convert screen coordinates to game coordinates
    float gameX = (mousePos.x - (GetScreenWidth() - (gameScreenWidth * scale)) * 0.5f) / scale;
    float gameY = (mousePos.y - (GetScreenHeight() - (gameScreenHeight * scale)) * 0.5f) / scale;
    
    // Check if mouse is over menu bar
    isMenuBarHovered = (gameY >= 0 && gameY <= 30);
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (CheckCollisionPointRec({gameX, gameY}, fileMenuRect))
        {
            isFileMenuOpen = !isFileMenuOpen;
            isHelpMenuOpen = false;
            return true;
        }
        else if (CheckCollisionPointRec({gameX, gameY}, helpMenuRect))
        {
            isHelpMenuOpen = !isHelpMenuOpen;
            isFileMenuOpen = false;
            return true;
        }
        else if (isFileMenuOpen)
        {
            if (CheckCollisionPointRec({gameX, gameY}, newGameOptionRect))
            {
                Randomize();
                isFileMenuOpen = false;
                return true;
            }
            else if (CheckCollisionPointRec({gameX, gameY}, quitOptionRect))
            {
                exitWindowRequested = true;
                isFileMenuOpen = false;
                return true;
            }
            else
            {
                isFileMenuOpen = false;
                return true;
            }
        }
        else if (isHelpMenuOpen)
        {
            if (CheckCollisionPointRec({gameX, gameY}, aboutOptionRect))
            {
                showHelpPopup = true;
                isHelpMenuOpen = false;
                return true;
            }
            else
            {
                isHelpMenuOpen = false;
                return true;
            }
        }
        else if (showHelpPopup && CheckCollisionPointRec({gameX, gameY}, okButtonRect))
        {
            showHelpPopup = false;
            return true;
        }
    }
    return false;
}

std::string Game::FormatWithLeadingZeroes(int number, int width)
{
    std::string numberText = std::to_string(number);
    int leadingZeros = width - numberText.length();
    numberText = std::string(leadingZeros, '0') + numberText;
    return numberText;
}

void Game::Randomize()
{
    InitializeGrid();
    PlaceMines();
    CalculateAdjacentMines();
    remainingCells = GRID_SIZE * GRID_SIZE - NUM_MINES;
    gameOver = false;
    gameWon = false;
    gameTime = 0.0f;  // Reset timer
}

void Game::InitializeGrid() {
    grid.resize(GRID_SIZE, std::vector<Cell>(GRID_SIZE));
    for (int row = 0; row < GRID_SIZE; ++row) {
        for (int col = 0; col < GRID_SIZE; ++col) {
            grid[row][col] = { false, CellState::HIDDEN, 0 };
        }
    }
}

void Game::PlaceMines() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, GRID_SIZE - 1);

    int minesPlaced = 0;
    while (minesPlaced < NUM_MINES) {
        int row = dis(gen);
        int col = dis(gen);
        if (!grid[row][col].hasMine) {
            grid[row][col].hasMine = true;
            minesPlaced++;
        }
    }
}

void Game::CalculateAdjacentMines() {
    for (int row = 0; row < GRID_SIZE; ++row) {
        for (int col = 0; col < GRID_SIZE; ++col) {
            if (!grid[row][col].hasMine) {
                int count = 0;
                for (int dr = -1; dr <= 1; ++dr) {
                    for (int dc = -1; dc <= 1; ++dc) {
                        int newRow = row + dr;
                        int newCol = col + dc;
                        if (IsValidCell(newRow, newCol) && grid[newRow][newCol].hasMine) {
                            count++;
                        }
                    }
                }
                grid[row][col].adjacentMines = count;
            }
        }
    }
}

void Game::RevealCell(int row, int col) {
    if (!IsValidCell(row, col) || grid[row][col].state != CellState::HIDDEN) {
        return;
    }

    grid[row][col].state = CellState::REVEALED;
    remainingCells--;

    if (grid[row][col].hasMine) {
        gameOver = true;
        gameWon = false;
        RevealAllMines();
        return;
    }

    if (grid[row][col].adjacentMines == 0) {
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                RevealCell(row + dr, col + dc);
            }
        }
    }

    CheckWinCondition();
}

void Game::RevealAllMines() {
    for (int row = 0; row < GRID_SIZE; ++row) {
        for (int col = 0; col < GRID_SIZE; ++col) {
            if (grid[row][col].hasMine) {
                grid[row][col].state = CellState::REVEALED;
            }
        }
    }
}

bool Game::IsValidCell(int row, int col) const {
    return row >= 0 && row < GRID_SIZE && col >= 0 && col < GRID_SIZE;
}

void Game::CheckWinCondition() {
    if (remainingCells == 0) {
        gameOver = true;
        gameWon = true;
    }
}

void Game::DrawGrid() const {
    for (int row = 0; row < GRID_SIZE; ++row) {
        for (int col = 0; col < GRID_SIZE; ++col) {
            DrawCell(row, col);
        }
    }
}

void Game::DrawCell(int row, int col) const {
    // Calculate cell position
    float x = gridOffset.x + col * cellSize;
    float y = gridOffset.y + row * cellSize;
    
    // Draw cell background
    Color cellColor = LIGHTGRAY;
    if (grid[row][col].state == CellState::REVEALED) {
        cellColor = WHITE;
    }
    DrawRectangle(x, y, cellSize - 1, cellSize - 1, cellColor);
    
    // Draw cell content
    if (grid[row][col].state == CellState::REVEALED) {
        if (grid[row][col].hasMine) {
            // Draw bomb texture
            Rectangle source = { 0, 0, (float)bombTexture.width, (float)bombTexture.height };
            Rectangle dest = { x, y, cellSize, cellSize };
            DrawTexturePro(bombTexture, source, dest, Vector2{0, 0}, 0, WHITE);
        }
        else if (grid[row][col].adjacentMines > 0) {
            // Draw number texture
            Rectangle source = { 0, 0, (float)numberTextures[grid[row][col].adjacentMines - 1].width, 
                               (float)numberTextures[grid[row][col].adjacentMines - 1].height };
            Rectangle dest = { x, y, cellSize, cellSize };
            DrawTexturePro(numberTextures[grid[row][col].adjacentMines - 1], source, dest, Vector2{0, 0}, 0, WHITE);
        }
    }
    else if (grid[row][col].state == CellState::FLAGGED) {
        // Draw flag texture
        Rectangle source = { 0, 0, (float)flagTexture.width, (float)flagTexture.height };
        Rectangle dest = { x, y, cellSize, cellSize };
        DrawTexturePro(flagTexture, source, dest, Vector2{0, 0}, 0, WHITE);
    }
    
    // Draw cell border
    DrawRectangleLines(x, y, cellSize, cellSize, DARKGRAY);
}

void Game::UpdateScaling() {
    const int padding = 20;
    const int menuHeight = 30;
    const int statsHeight = 30;  // Height for stats area
    const int totalVerticalPadding = menuHeight + statsHeight + padding * 2; // Menu height + stats height + top padding + bottom padding
    
    // Calculate the maximum possible cell size that fits in the window
    float maxCellWidth = (float)gameScreenWidth / GRID_SIZE;
    float maxCellHeight = (float)(gameScreenHeight - totalVerticalPadding) / GRID_SIZE;
    cellSize = MIN(maxCellWidth, maxCellHeight);
    
    // Calculate the total grid size
    float totalGridSize = cellSize * GRID_SIZE;
    
    // Calculate the offset to center the grid horizontally and place it below the menu and stats with padding
    gridOffset.x = (gameScreenWidth - totalGridSize) / 2;
    gridOffset.y = menuHeight + statsHeight + padding + (gameScreenHeight - totalVerticalPadding - totalGridSize) / 2;
}

void Game::LoadTextures() {
    // Load bomb and flag textures
    bombTexture = LoadTexture("data/bomb.png");
    flagTexture = LoadTexture("data/flag.png");
    
    // Load number textures
    for (int i = 0; i < 8; i++) {
        char path[20];
        sprintf(path, "data/%d.png", i + 1);
        numberTextures[i] = LoadTexture(path);
    }
}

void Game::UnloadTextures() {
    UnloadTexture(bombTexture);
    UnloadTexture(flagTexture);
    for (int i = 0; i < 8; i++) {
        UnloadTexture(numberTextures[i]);
    }
}

void Game::RevealAdjacentCells(int row, int col) {
    if (!IsValidCell(row, col) || grid[row][col].state != CellState::REVEALED || grid[row][col].adjacentMines == 0) {
        return;
    }

    int flaggedCount = 0;
    // Count flagged neighbors
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            int newRow = row + dr;
            int newCol = col + dc;
            if (IsValidCell(newRow, newCol) && grid[newRow][newCol].state == CellState::FLAGGED) {
                flaggedCount++;
            }
        }
    }

    // If the number of flags matches the number of adjacent mines, reveal all non-flagged neighbors
    if (flaggedCount == grid[row][col].adjacentMines) {
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                int newRow = row + dr;
                int newCol = col + dc;
                if (IsValidCell(newRow, newCol) && grid[newRow][newCol].state == CellState::HIDDEN) {
                    RevealCell(newRow, newCol);
                }
            }
        }
    }
}