#include <vector>
#include <utility>
#include <string>
#include <cmath>  // For sqrtf
#include <iostream>
#include <cstring>  // For memset

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
      showCustomGamePopup(false), gameTime(0.0f), remainingMines(0), currentGridSize(INITIAL_GRID_SIZE),
      customGridSizeInputLength(0)
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

    // Initialize custom grid size input buffer
    memset(customGridSizeInput, 0, sizeof(customGridSizeInput));
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

    // If help popup or custom game popup is shown, ignore all game input
    if (showHelpPopup || showCustomGamePopup) {
        return;
    }

    // Update game time if game is not over
    if (!gameOver && !gameWon) {
        gameTime += dt;
    }

    // Update remaining mines count
    int flaggedCount = 0;
    for (int row = 0; row < currentGridSize; ++row) {
        for (int col = 0; col < currentGridSize; ++col) {
            if (grid[row][col].state == CellState::FLAGGED) {
                flaggedCount++;
            }
        }
    }
    remainingMines = CalculateMineCount() - flaggedCount;

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
        bool isInGrid = (gameX >= gridOffset.x && gameX < gridOffset.x + currentGridSize * cellSize &&
                        gameY >= gridOffset.y && gameY < gridOffset.y + currentGridSize * cellSize);

        if (gameOver) {
            // If game is over, any click in the game area starts a new game
            if (isInGrid) {
                Randomize();
            }
            return;
        }

        if (IsValidCell(row, col)) {
            if (grid[row][col].state == CellState::HIDDEN) {
                RevealCell(row, col);
            }
            else if (grid[row][col].state == CellState::REVEALED && grid[row][col].adjacentMines > 0) {
                // Check if right button is also pressed
                if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
                    RevealAdjacentCells(row, col);
                }
            }
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
            else if (grid[row][col].state == CellState::REVEALED && grid[row][col].adjacentMines > 0) {
                // Check if left button is also pressed
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                    RevealAdjacentCells(row, col);
                }
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


    // Draw game stats
    const int padding = 10;
    const int fontSize = 20;
    const int statsHeight = 30;
    
    // Draw remaining mines
    std::string minesText = "Mines: " + std::to_string(remainingMines);
    DrawText(minesText.c_str(), gridOffset.x, gridOffset.y - statsHeight, fontSize, WHITE);
    
    // Draw timer
    std::string timeText = "Timer: " + std::to_string((int)gameTime);
    int timeTextWidth = MeasureText(timeText.c_str(), fontSize);
    DrawText(timeText.c_str(), gridOffset.x + currentGridSize * cellSize - timeTextWidth, gridOffset.y - statsHeight, fontSize, WHITE);

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

    // Draw custom game popup if active
    if (showCustomGamePopup)
    {
        // Draw semi-transparent background
        DrawRectangle(0, 0, gameScreenWidth, gameScreenHeight, (Color){0, 0, 0, 128});
        
        // Draw popup background
        const int popupWidth = 400;
        const int popupHeight = 200;
        popupRect = {(float)(gameScreenWidth - popupWidth) / 2, (float)(gameScreenHeight - popupHeight) / 2,
                    (float)popupWidth, (float)popupHeight};
        DrawRectangleRec(popupRect, LIGHTGRAY);
        
        // Draw popup title
        const char* title = "Custom Game";
        int titleWidth = MeasureText(title, 24);
        DrawText(title, popupRect.x + (popupWidth - titleWidth) / 2, popupRect.y + 30, 24, BLACK);
        
        // Draw input prompt
        const char* prompt = "Enter grid size:";
        DrawText(prompt, popupRect.x + 30, popupRect.y + 80, 20, BLACK);
        
        // Draw input box
        Rectangle inputBox = {popupRect.x + 30, popupRect.y + 110, popupWidth - 60, 30};
        DrawRectangleRec(inputBox, WHITE);
        DrawRectangleLinesEx(inputBox, 2, BLACK);
        
        // Draw input text
        if (customGridSizeInputLength > 0) {
            DrawText(customGridSizeInput, inputBox.x + 5, inputBox.y + 5, 20, BLACK);
        }
        
        // Draw OK button
        const char* okText = "OK";
        int okTextWidth = MeasureText(okText, 20);
        okButtonRect = {popupRect.x + (popupWidth - 100) / 2, popupRect.y + popupHeight - 60, 100, 30};
        DrawRectangleRec(okButtonRect, GRAY);
        DrawText(okText, okButtonRect.x + (okButtonRect.width - okTextWidth) / 2, 
                okButtonRect.y + 5, 20, BLACK);
    }

    DrawMenuBar();
}

void Game::Draw()
{
    // Update scale based on current window size
    scale = MIN((float)GetScreenWidth() / gameScreenWidth, (float)GetScreenHeight() / gameScreenHeight);
    
    // Render to texture
    BeginTextureMode(targetRenderTex);
    ClearBackground(RAYWHITE);
    
    // Draw background
    DrawTexturePro(backgroundTexture,
        (Rectangle){0, 0, (float)backgroundTexture.width, (float)backgroundTexture.height},
        (Rectangle){0, 0, (float)gameScreenWidth, (float)gameScreenHeight},
        (Vector2){0, 0}, 0.0f, WHITE);
    
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
        const char* text = "You lost! Click to try again";
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
    DrawRectangle(0, 0, gameScreenWidth, 30, BLACK);
    
    // Draw File menu
    const char* fileText = "File";
    int textWidth = MeasureText(fileText, 20);
    fileMenuRect = {110, 5, (float)textWidth + 20, 20};
    
    // Draw File menu button
    Color fileButtonColor = isFileMenuOpen ? DARKGRAY : BLACK;
    DrawRectangleRec(fileMenuRect, fileButtonColor);
    DrawText(fileText, 120, 5, 20, WHITE);
    
    // Draw File menu dropdown if open
    if (isFileMenuOpen)
    {
        const char* newGameText = "New Game";
        const char* customGameText = "Custom Game";
        const char* quitText = "Quit";
        int newGameTextWidth = MeasureText(newGameText, 20);
        int customGameTextWidth = MeasureText(customGameText, 20);
        int quitTextWidth = MeasureText(quitText, 20);
        float menuWidth = (float)MAX(MAX(newGameTextWidth, customGameTextWidth), quitTextWidth) + 20;
        
        // Draw New Game option
        newGameOptionRect = {fileMenuRect.x, fileMenuRect.y + fileMenuRect.height, 
                           menuWidth, 25};
        DrawRectangleRec(newGameOptionRect, BLACK);
        DrawText(newGameText, newGameOptionRect.x + 10, newGameOptionRect.y + 2, 20, WHITE);
        
        // Draw Custom Game option
        customGameOptionRect = {fileMenuRect.x, newGameOptionRect.y + newGameOptionRect.height,
                              menuWidth, 25};
        DrawRectangleRec(customGameOptionRect, BLACK);
        DrawText(customGameText, customGameOptionRect.x + 10, customGameOptionRect.y + 2, 20, WHITE);
        
        // Draw Quit option
        quitOptionRect = {fileMenuRect.x, customGameOptionRect.y + customGameOptionRect.height,
                         menuWidth, 25};
        DrawRectangleRec(quitOptionRect, BLACK);
        DrawText(quitText, quitOptionRect.x + 10, quitOptionRect.y + 2, 20, WHITE);
    }

    // Draw Help menu
    const char* helpText = "Help";
    int helpTextWidth = MeasureText(helpText, 20);
    helpMenuRect = {fileMenuRect.x + fileMenuRect.width + 20, 5, (float)helpTextWidth + 20, 20};
    
    // Draw Help menu button
    Color helpButtonColor = isHelpMenuOpen ? DARKGRAY : BLACK;
    DrawRectangleRec(helpMenuRect, helpButtonColor);
    DrawText(helpText, helpMenuRect.x + 10, 5, 20, WHITE);
    
    // Draw Help menu dropdown if open
    if (isHelpMenuOpen)
    {
        const char* aboutText = "About";
        int aboutTextWidth = MeasureText(aboutText, 20);
        aboutOptionRect = {helpMenuRect.x, helpMenuRect.y + helpMenuRect.height,
                          (float)aboutTextWidth + 20, 25};
        DrawRectangleRec(aboutOptionRect, BLACK);
        DrawText(aboutText, aboutOptionRect.x + 10, aboutOptionRect.y + 2, 20, WHITE);
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
                ResetToInitialSize();
                isFileMenuOpen = false;
                return true;
            }
            else if (CheckCollisionPointRec({gameX, gameY}, customGameOptionRect))
            {
                showCustomGamePopup = true;
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
        else if (showCustomGamePopup)
        {
            if (CheckCollisionPointRec({gameX, gameY}, okButtonRect))
            {
                // Parse custom grid size
                std::string input(customGridSizeInput);
                int size = 0;
                
                // Check for NxN format
                size_t xPos = input.find('x');
                if (xPos != std::string::npos) {
                    input = input.substr(0, xPos);
                }
                
                // Try to convert to number
                try {
                    size = std::stoi(input);
                } catch (...) {
                    size = 0;
                }
                
                // Validate size
                if (size >= 3 && size <= 30) {
                    currentGridSize = size;
                    Randomize();
                } else {
                    ResetToInitialSize();
                }
                
                showCustomGamePopup = false;
                memset(customGridSizeInput, 0, sizeof(customGridSizeInput));
                customGridSizeInputLength = 0;
                return true;
            }
        }
        else if (showHelpPopup && CheckCollisionPointRec({gameX, gameY}, okButtonRect))
        {
            showHelpPopup = false;
            return true;
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
    }
    
    // Handle text input for custom game popup
    if (showCustomGamePopup) {
        int key = GetCharPressed();
        while (key > 0) {
            if ((key >= '0' && key <= '9') || key == 'x' || key == 'X') {
                if (customGridSizeInputLength < sizeof(customGridSizeInput) - 1) {
                    customGridSizeInput[customGridSizeInputLength] = (char)key;
                    customGridSizeInputLength++;
                }
            }
            key = GetCharPressed();
        }
        
        // Handle backspace
        if (IsKeyPressed(KEY_BACKSPACE) && customGridSizeInputLength > 0) {
            customGridSizeInputLength--;
            customGridSizeInput[customGridSizeInputLength] = '\0';
        }
        
        // Handle enter key
        if (IsKeyPressed(KEY_ENTER)) {
            // Parse custom grid size
            std::string input(customGridSizeInput);
            int size = 0;
            
            // Check for NxN format
            size_t xPos = input.find('x');
            if (xPos != std::string::npos) {
                input = input.substr(0, xPos);
            }
            
            // Try to convert to number
            try {
                size = std::stoi(input);
            } catch (...) {
                size = 0;
            }
            
            // Validate size
            if (size >= 3 && size <= 30) {
                currentGridSize = size;
                Randomize();
            } else {
                ResetToInitialSize();
            }
            
            showCustomGamePopup = false;
            memset(customGridSizeInput, 0, sizeof(customGridSizeInput));
            customGridSizeInputLength = 0;
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
    // Only increase grid size if we won
    if (gameWon) {
        currentGridSize++;
    }
    // Don't reset grid size on loss - keep the same size
    
    // Clear the grid before initializing
    grid.clear();
    
    InitializeGrid();
    PlaceMines();
    CalculateAdjacentMines();
    remainingCells = currentGridSize * currentGridSize - CalculateMineCount();
    gameOver = false;
    gameWon = false;
    gameTime = 0.0f;  // Reset timer
    
    // Update scaling to adjust view for new grid size
    UpdateScaling();
}

void Game::InitializeGrid() {
    grid.resize(currentGridSize, std::vector<Cell>(currentGridSize));
    for (int row = 0; row < currentGridSize; ++row) {
        for (int col = 0; col < currentGridSize; ++col) {
            grid[row][col] = { false, CellState::HIDDEN, 0 };
        }
    }
}

int Game::CalculateMineCount() const {
    // Calculate total cells
    int totalCells = currentGridSize * currentGridSize;
    
    // Use approximately 15% of cells as mines
    // This gives us a good balance between challenge and playability
    int mineCount = static_cast<int>(totalCells * 0.15f);
    
    // Ensure we have at least 1 mine
    return MAX(1, mineCount);
}

void Game::PlaceMines() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, currentGridSize - 1);

    int minesToPlace = CalculateMineCount();
    int minesPlaced = 0;
    while (minesPlaced < minesToPlace) {
        int row = dis(gen);
        int col = dis(gen);
        if (!grid[row][col].hasMine) {
            grid[row][col].hasMine = true;
            minesPlaced++;
        }
    }
    remainingMines = minesToPlace;
}

void Game::CalculateAdjacentMines() {
    for (int row = 0; row < currentGridSize; ++row) {
        for (int col = 0; col < currentGridSize; ++col) {
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
    for (int row = 0; row < currentGridSize; ++row) {
        for (int col = 0; col < currentGridSize; ++col) {
            if (grid[row][col].hasMine) {
                grid[row][col].state = CellState::REVEALED;
            }
        }
    }
}

bool Game::IsValidCell(int row, int col) const {
    return row >= 0 && row < currentGridSize && col >= 0 && col < currentGridSize;
}

void Game::CheckWinCondition() {
    if (remainingCells == 0) {
        gameOver = true;
        gameWon = true;
    }
}

void Game::DrawGrid() const {
    // Draw black background for the entire game area
    DrawRectangle(gridOffset.x, gridOffset.y, 
                 currentGridSize * cellSize, currentGridSize * cellSize, BLACK);
    
    for (int row = 0; row < currentGridSize; ++row) {
        for (int col = 0; col < currentGridSize; ++col) {
            DrawCell(row, col);
        }
    }
}

void Game::DrawCell(int row, int col) const {
    // Calculate cell position
    float x = gridOffset.x + col * cellSize;
    float y = gridOffset.y + row * cellSize;
    
    // Draw cell background
    Color cellColor = (Color){0, 255, 255, 255};  // Aqua blue for hidden cells
    if (grid[row][col].state == CellState::REVEALED) {
        cellColor = (Color){135, 206, 235, 255};  // Sky blue for revealed cells
    }
    DrawRectangle(x, y, cellSize-1, cellSize-1, cellColor);    
    
    // Draw cell content
    if (grid[row][col].state == CellState::REVEALED) {
        if (grid[row][col].hasMine) {
            // Draw bomb texture
            Rectangle source = { 0, 0, (float)bombTexture.width, (float)bombTexture.height };
            Rectangle dest = { x, y, cellSize-2, cellSize-2};
            DrawTexturePro(bombTexture, source, dest, Vector2{0, 0}, 0, WHITE);
        }
        else if (grid[row][col].adjacentMines > 0) {
            // Draw number texture
            Rectangle source = { 0, 0, (float)numberTextures[grid[row][col].adjacentMines - 1].width, 
                               (float)numberTextures[grid[row][col].adjacentMines - 1].height };
            Rectangle dest = { x, y, cellSize-2, cellSize-2};
            DrawTexturePro(numberTextures[grid[row][col].adjacentMines - 1], source, dest, Vector2{0, 0}, 0, WHITE);
        }
    }
    else if (grid[row][col].state == CellState::FLAGGED) {
        // Draw flag texture
        Rectangle source = { 0, 0, (float)flagTexture.width, (float)flagTexture.height };
        Rectangle dest = { x, y, cellSize-2, cellSize-2};
        DrawTexturePro(flagTexture, source, dest, Vector2{0, 0}, 0, WHITE);
    }
}

void Game::UpdateScaling() {
    const int padding = 20;
    const int menuHeight = 30;
    const int statsHeight = 30;  // Height for stats area
    const int totalVerticalPadding = menuHeight + statsHeight + padding * 2; // Menu height + stats height + top padding + bottom padding
    
    // Calculate the maximum possible cell size that fits in the window
    float maxCellWidth = (float)gameScreenWidth / currentGridSize;
    float maxCellHeight = (float)(gameScreenHeight - totalVerticalPadding) / currentGridSize;
    cellSize = MIN(maxCellWidth, maxCellHeight);
    
    // Calculate the total grid size
    float totalGridSize = cellSize * currentGridSize;
    
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

    // Load background texture
    backgroundTexture = LoadTexture("data/background.jpg");
}

void Game::UnloadTextures() {
    UnloadTexture(bombTexture);
    UnloadTexture(flagTexture);
    for (int i = 0; i < 8; i++) {
        UnloadTexture(numberTextures[i]);
    }
    UnloadTexture(backgroundTexture);
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

void Game::ResetToInitialSize() {
    currentGridSize = INITIAL_GRID_SIZE;
    Randomize();
}