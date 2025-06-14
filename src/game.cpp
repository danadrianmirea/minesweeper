#include <vector>
#include <utility>
#include <string>
#include <cmath>  // For sqrtf
#include <iostream>
#include <cstring>  // For memset
#include <fstream>  // For file operations
#include <algorithm>  // For std::max

#include "raylib.h"
#include "globals.h"
#include "game.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

const float Game::LONG_TAP_THRESHOLD = 0.3f;

bool Game::isMobile = false;

Game::Game(int screenWidth, int screenHeight)
    : screenWidth(screenWidth), screenHeight(screenHeight), gameOver(false), gameWon(false),
      gameOverTextTimer(0.0f),  // Initialize game over text timer
      isMenuBarHovered(false), isFileMenuOpen(false), isHelpMenuOpen(false), isOptionsMenuOpen(false), showHelpPopup(false),
      showCustomGamePopup(false), showSavePopup(false), showLoadPopup(false), showWelcomePopup(true),  // Show welcome popup at start
      gameTime(0.0f), remainingMines(0), currentGridSize(isMobile ? MOBILE_INITIAL_GRID_SIZE : DESKTOP_INITIAL_GRID_SIZE), customGridSizeInputLength(0),
      filenameInputLength(0), isTapping(false), tapStartTime(0.0f), tapStartPos({0, 0}), tapRow(-1), tapCol(-1),
      longTapPerformed(false), waitingForNextLevel(false), waitingForGameOver(false), isMusicPlaying(false)
{
#ifdef DEBUG
    std::cout << "Game constructor: Initializing with screen size " << screenWidth << "x" << screenHeight << std::endl;
#endif
#ifdef __EMSCRIPTEN__
    // Check if we're running on a mobile device
    isMobile = EM_ASM_INT({
        return /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent);
    });
#endif
    currentGridSize = isMobile ? MOBILE_INITIAL_GRID_SIZE : DESKTOP_INITIAL_GRID_SIZE;
    screenScale = MIN((float)GetScreenWidth() / gameScreenWidth, (float)GetScreenHeight() / gameScreenHeight);
    targetRenderTex = LoadRenderTexture(gameScreenWidth, gameScreenHeight);
    SetTextureFilter(targetRenderTex.texture, TEXTURE_FILTER_BILINEAR); // Texture scale filter to use
    font = LoadFontEx("Font/monogram.ttf", 64, 0, 0);    
    LoadTextures();
    InitializeGrid();
    Randomize();
#ifdef DEBUG
    InitializeDebugGrid();
#endif

    UpdateScaling();

    // Initialize custom grid size input buffer
    memset(customGridSizeInput, 0, sizeof(customGridSizeInput));
    memset(filenameInput, 0, sizeof(filenameInput));

    // Load and setup background music
    backgroundMusic = LoadMusicStream("data/music.mp3");
    SetMusicVolume(backgroundMusic, MUSIC_VOLUME);
    //PlayMusicStream(backgroundMusic);
    isMusicPlaying = false;

    // Load sound effects
    hitSound = LoadSound("data/hit.mp3");
    actionSound = LoadSound("data/action.mp3");
    SetSoundVolume(hitSound, 0.7f);    // 70% volume for hit sound
    SetSoundVolume(actionSound, 0.5f); // 50% volume for action sound

#ifdef DEBUG
    std::cout << "Game constructor: Initialization complete" << std::endl;
#endif
}

Game::~Game()
{
    UnloadTextures();
    UnloadRenderTexture(targetRenderTex);
    UnloadFont(font);
    StopMusicStream(backgroundMusic);
    UnloadMusicStream(backgroundMusic);
    UnloadSound(hitSound);
    UnloadSound(actionSound);
}

void Game::Update(float dt)
{
    try {
        UpdateUI();
        bool menuHandledClick = HandleMenuInput();

        // Always update the music stream if it's playing
        if (isMusicPlaying)
        {
            UpdateMusicStream(backgroundMusic);
        }

        // If help popup or custom game popup is shown, ignore all game input
        if (showHelpPopup || showCustomGamePopup || showSavePopup || showLoadPopup) {
            return;
        }

        // If waiting for next level or game over input, don't process other game input
        if (waitingForNextLevel || waitingForGameOver) {
            return;
        }

        // Update game time if game is not over and welcome popup is not shown
        if (!gameOver && !gameWon && !showWelcomePopup) {
            gameTime += dt;
        }

        // Update game over text timer
        if (gameOver && !gameWon) {
            gameOverTextTimer += dt;
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
#ifdef DEBUG
                std::cout << "Starting new game after game over" << std::endl;
#endif
                Randomize();
            }
            return;
        }

        if (isMobile) {
            // Handle mobile tap controls
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !menuHandledClick && isInGrid) {
                // Start tracking tap
                isTapping = true;
                tapStartTime = gameTime;
                tapStartPos = mousePos;
                tapRow = row;
                tapCol = col;
                longTapPerformed = false;
            }
            else if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && isTapping) {
                // End tap
                isTapping = false;
                
                // Check if tap was in the same cell
                if (tapRow == row && tapCol == col && IsValidCell(row, col)) {
                    float tapDuration = gameTime - tapStartTime;
                    
                    if (grid[row][col].state == CellState::HIDDEN) {
                        if (tapDuration < LONG_TAP_THRESHOLD) {
                            // Short tap - reveal cell
                            RevealCell(row, col);
                        }
                    } else if (grid[row][col].state == CellState::FLAGGED) {
                        if (tapDuration >= LONG_TAP_THRESHOLD && !longTapPerformed) {
                            // Long tap on flagged cell - unflag it
                            grid[row][col].state = CellState::HIDDEN;
                        }
                    } else if (grid[row][col].state == CellState::REVEALED && grid[row][col].adjacentMines > 0) {
                        // Tap on numbered cell - reveal adjacent cells
                        RevealAdjacentCells(row, col);
                    }
                }
            }
            else if (isTapping && IsValidCell(tapRow, tapCol)) {
                // Check if we're still holding the tap
                float tapDuration = gameTime - tapStartTime;
                
                if (tapDuration >= LONG_TAP_THRESHOLD && !longTapPerformed) {
                    if (grid[tapRow][tapCol].state == CellState::HIDDEN) {
                        // Show flag when timer expires on hidden cell
                        grid[tapRow][tapCol].state = CellState::FLAGGED;
                        longTapPerformed = true;
                    } else if (grid[tapRow][tapCol].state == CellState::FLAGGED) {
                        // Remove flag when timer expires on flagged cell
                        grid[tapRow][tapCol].state = CellState::HIDDEN;
                        longTapPerformed = true;
                    }
                }
            }
        } else {
            // Desktop controls
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !menuHandledClick) {
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
                if (IsValidCell(row, col)) {
                    if (grid[row][col].state == CellState::HIDDEN) {
                        grid[row][col].state = CellState::FLAGGED;
                        PlaySound(actionSound);  // Play action sound for flagging
                    }
                    else if (grid[row][col].state == CellState::FLAGGED) {
                        grid[row][col].state = CellState::HIDDEN;
                        PlaySound(actionSound);  // Play action sound for unflagging
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
    } catch (const std::exception& e) {
#ifdef DEBUG
        std::cerr << "Exception in Game::Update: " << e.what() << std::endl;
#endif
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

    // Draw welcome popup if active
    if (showWelcomePopup) {
        // Draw semi-transparent background
        DrawRectangle(0, 0, gameScreenWidth, gameScreenHeight, (Color){0, 0, 0, 128});
        
        // Calculate text measurements
        const char* title = "Welcome to Minesweeper!";
        const char* welcomeText = "Here are some tips to help you get started:";
        const char* desktopTips[] = {
            "1. The four corner cells are always safe - no mines there!",
            "2. Left-click to reveal a cell, right-click to place/remove a flag",
            "3. Numbers show how many mines are adjacent to that cell",
            "4. When you lose, you can try again with the same grid size",
            "5. Try to reach and beat the 20x20 grid to complete the game!",
            "6. After marking the flags, use both mouse buttons on a number to reveal adjacent cells"
        };
        const char* mobileTips[] = {
            "1. The four corner cells are always safe - no mines there!",
            "2. Numbers show how many mines are adjacent to that cell",
            "3. When you lose, you can try again with the same grid size",
            "4. Try to reach and beat the 8x8 grid to complete the game!",
            "5. Tap a cell to reveal it",
            "6. Hold a cell for 0.3s to place/remove a flag",
            "7. Tap a numbered cell to reveal adjacent cells"
        };
        const char** tips = isMobile ? mobileTips : desktopTips;
        const int numTips = isMobile ? 7 : 6;
        
        // Calculate maximum width needed
        int maxWidth = MeasureText(title, 24);
        maxWidth = MAX(maxWidth, MeasureText(welcomeText, 20));
        for (int i = 0; i < numTips; i++) {
            maxWidth = MAX(maxWidth, MeasureText(tips[i], 20));
        }
        
        // Add padding to width and calculate height
        const int padding = 30;
        const int lineHeight = 35;
        const int popupWidth = maxWidth + padding * 2;
        const int popupHeight = isMobile ? 450 : 400;  // Increase height for mobile to accommodate extra tip
        
        // Draw popup background
        popupRect = {(float)(gameScreenWidth - popupWidth) / 2, (float)(gameScreenHeight - popupHeight) / 2,
                    (float)popupWidth, (float)popupHeight};
        DrawRectangleRec(popupRect, LIGHTGRAY);
        
        // Draw popup title
        int titleWidth = MeasureText(title, 24);
        DrawText(title, popupRect.x + (popupWidth - titleWidth) / 2, popupRect.y + 30, 24, BLACK);
        
        // Draw welcome message
        DrawText(welcomeText, popupRect.x + padding, popupRect.y + 80, 20, BLACK);
        
        // Draw tips
        for (int i = 0; i < numTips; i++) {
            DrawText(tips[i], popupRect.x + padding, popupRect.y + 120 + i * lineHeight, 20, BLACK);
        }
        
        // Draw OK button
        const char* okText = "Let's Play!";
        int okTextWidth = MeasureText(okText, 20);
        okButtonRect = {(float)(popupRect.x + (popupWidth - (okTextWidth + 40)) / 2),  // Center with extra padding
                       (float)(popupRect.y + popupHeight - (isMobile ? 80 : 60)),  // Move button up by 50px for mobile
                       (float)(okTextWidth + 40),  // Add 40 pixels padding (20 on each side)
                       30.0f};
        DrawRectangleRec(okButtonRect, GRAY);
        DrawText(okText, okButtonRect.x + (okButtonRect.width - okTextWidth) / 2, 
                okButtonRect.y + 5, 20, BLACK);
    }

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
        const char* prompt = "Enter grid size (5/20):";
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

    // Draw save popup if active
    if (showSavePopup) {
        // Draw semi-transparent background
        DrawRectangle(0, 0, gameScreenWidth, gameScreenHeight, (Color){0, 0, 0, 128});
        
        // Draw popup background
        const int popupWidth = 400;
        const int popupHeight = 200;
        popupRect = {(float)(gameScreenWidth - popupWidth) / 2, (float)(gameScreenHeight - popupHeight) / 2,
                    (float)popupWidth, (float)popupHeight};
        DrawRectangleRec(popupRect, LIGHTGRAY);
        
        // Draw popup title
        const char* title = "Save Game";
        int titleWidth = MeasureText(title, 24);
        DrawText(title, popupRect.x + (popupWidth - titleWidth) / 2, popupRect.y + 30, 24, BLACK);
        
        // Draw input prompt
        const char* prompt = "Enter filename:";
        DrawText(prompt, popupRect.x + 30, popupRect.y + 80, 20, BLACK);
        
        // Draw input box
        Rectangle inputBox = {popupRect.x + 30, popupRect.y + 110, popupWidth - 60, 30};
        DrawRectangleRec(inputBox, WHITE);
        DrawRectangleLinesEx(inputBox, 2, BLACK);
        
        // Draw input text
        if (filenameInputLength > 0) {
            DrawText(filenameInput, inputBox.x + 5, inputBox.y + 5, 20, BLACK);
        }
        
        // Draw OK button
        const char* okText = "OK";
        int okTextWidth = MeasureText(okText, 20);
        okButtonRect = {popupRect.x + (popupWidth - 100) / 2, popupRect.y + popupHeight - 60, 100, 30};
        DrawRectangleRec(okButtonRect, GRAY);
        DrawText(okText, okButtonRect.x + (okButtonRect.width - okTextWidth) / 2, 
                okButtonRect.y + 5, 20, BLACK);
    }

    // Draw load popup if active
    if (showLoadPopup) {
        // Draw semi-transparent background
        DrawRectangle(0, 0, gameScreenWidth, gameScreenHeight, (Color){0, 0, 0, 128});
        
        // Draw popup background
        const int popupWidth = 400;
        const int popupHeight = 200;
        popupRect = {(float)(gameScreenWidth - popupWidth) / 2, (float)(gameScreenHeight - popupHeight) / 2,
                    (float)popupWidth, (float)popupHeight};
        DrawRectangleRec(popupRect, LIGHTGRAY);
        
        // Draw popup title
        const char* title = "Load Game";
        int titleWidth = MeasureText(title, 24);
        DrawText(title, popupRect.x + (popupWidth - titleWidth) / 2, popupRect.y + 30, 24, BLACK);
        
        // Draw input prompt
        const char* prompt = "Enter filename:";
        DrawText(prompt, popupRect.x + 30, popupRect.y + 80, 20, BLACK);
        
        // Draw input box
        Rectangle inputBox = {popupRect.x + 30, popupRect.y + 110, popupWidth - 60, 30};
        DrawRectangleRec(inputBox, WHITE);
        DrawRectangleLinesEx(inputBox, 2, BLACK);
        
        // Draw input text
        if (filenameInputLength > 0) {
            DrawText(filenameInput, inputBox.x + 5, inputBox.y + 5, 20, BLACK);
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

void Game::Draw(float dt)
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
        const char* text;
        int maxSize = isMobile ? MOBILE_MAX_GRID_SIZE : DESKTOP_MAX_GRID_SIZE;
        if (currentGridSize == maxSize) {
            text = "You Won! Congratulations, you beat the game!";
        } else {
            text = isMobile ? "You Won! Tap to continue to next level" : "You Won! Click to continue to next level";
        }
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
    else if (gameOver && !gameWon) {
        const char* text = isMobile ? "You lost! Tap to try again" : "You lost! Click to try again";
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
        
        // Update timer for text fade effect
        gameOverTextTimer += dt;
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
    DrawRectangle(0, 0, gameScreenWidth, 45, BLACK);
    
    // Draw File menu
    const char* fileText = "File";
    int textWidth = MeasureText(fileText, 30);  // Increased font size from 20 to 30
    fileMenuRect = {110, 7, (float)textWidth + 30, 30};  // Increased height from 20 to 30, added more padding
    
    // Draw File menu button
    Color fileButtonColor = isFileMenuOpen ? DARKGRAY : BLACK;
    DrawRectangleRec(fileMenuRect, fileButtonColor);
    DrawText(fileText, 120, 7, 30, WHITE);  // Increased font size from 20 to 30
    
    // Draw File menu dropdown if open
    if (isFileMenuOpen)
    {
        const char* newGameText = "New Game";
        const char* customGameText = "Custom Game";
        int newGameTextWidth = MeasureText(newGameText, 30);  // Increased font size
        int customGameTextWidth = MeasureText(customGameText, 30);  // Increased font size
        
#ifndef __EMSCRIPTEN__
        const char* saveGameText = "Save Game";
        const char* loadGameText = "Load Game";
        const char* quitText = "Quit";
        int saveGameTextWidth = MeasureText(saveGameText, 30);  // Increased font size
        int loadGameTextWidth = MeasureText(loadGameText, 30);  // Increased font size
        int quitTextWidth = MeasureText(quitText, 30);  // Increased font size
        
        // Find maximum width among all menu items
        float menuWidth = (float)(std::max({newGameTextWidth, customGameTextWidth, saveGameTextWidth, loadGameTextWidth, quitTextWidth}) + 30);
#else
        float menuWidth = (float)(std::max(newGameTextWidth, customGameTextWidth) + 30);
#endif
        
        // Draw New Game option
        newGameOptionRect = {fileMenuRect.x, fileMenuRect.y + fileMenuRect.height, 
                           menuWidth, 35};  // Increased height from 25 to 35
        DrawRectangleRec(newGameOptionRect, BLACK);
        DrawText(newGameText, newGameOptionRect.x + 10, newGameOptionRect.y + 2, 30, WHITE);  // Increased font size

        if(!isMobile)        
        {
            // Draw Custom Game option
            customGameOptionRect = {fileMenuRect.x, newGameOptionRect.y + newGameOptionRect.height,
                                menuWidth, 35};  // Increased height from 25 to 35
            DrawRectangleRec(customGameOptionRect, BLACK);
            DrawText(customGameText, customGameOptionRect.x + 10, customGameOptionRect.y + 2, 30, WHITE);  // Increased font size
        }

#ifndef __EMSCRIPTEN__
        // Draw Save Game option
        Rectangle saveGameOptionRect = {fileMenuRect.x, customGameOptionRect.y + customGameOptionRect.height,
                                     menuWidth, 35};  // Increased height from 25 to 35
        DrawRectangleRec(saveGameOptionRect, BLACK);
        DrawText(saveGameText, saveGameOptionRect.x + 10, saveGameOptionRect.y + 2, 30, WHITE);  // Increased font size

        // Draw Load Game option
        Rectangle loadGameOptionRect = {fileMenuRect.x, saveGameOptionRect.y + saveGameOptionRect.height,
                                     menuWidth, 35};  // Increased height from 25 to 35
        DrawRectangleRec(loadGameOptionRect, BLACK);
        DrawText(loadGameText, loadGameOptionRect.x + 10, loadGameOptionRect.y + 2, 30, WHITE);  // Increased font size
        
        // Draw Quit option
        quitOptionRect = {fileMenuRect.x, loadGameOptionRect.y + loadGameOptionRect.height,
                         menuWidth, 35};  // Increased height from 25 to 35
        DrawRectangleRec(quitOptionRect, BLACK);
        DrawText(quitText, quitOptionRect.x + 10, quitOptionRect.y + 2, 30, WHITE);  // Increased font size
#endif
    }

    // Draw Options menu
    const char* optionsText = "Options";
    int optionsTextWidth = MeasureText(optionsText, 30);  // Increased font size
    optionsMenuRect = {fileMenuRect.x + fileMenuRect.width + 20, 7, (float)optionsTextWidth + 30, 30};  // Increased height and padding
    
    // Draw Options menu button
    Color optionsButtonColor = isOptionsMenuOpen ? DARKGRAY : BLACK;
    DrawRectangleRec(optionsMenuRect, optionsButtonColor);
    DrawText(optionsText, optionsMenuRect.x + 10, 7, 30, WHITE);  // Increased font size
    
    // Draw Options menu dropdown if open
    if (isOptionsMenuOpen)
    {
        const char* toggleMusicText = "Toggle Music";
        int toggleMusicTextWidth = MeasureText(toggleMusicText, 30);  // Increased font size
        float menuWidth = (float)(toggleMusicTextWidth + 30);

        // Draw Toggle Music option
        toggleMusicOptionRect = {optionsMenuRect.x, optionsMenuRect.y + optionsMenuRect.height,
                               menuWidth, 35};  // Increased height from 25 to 35
        DrawRectangleRec(toggleMusicOptionRect, BLACK);
        DrawText(toggleMusicText, toggleMusicOptionRect.x + 10, toggleMusicOptionRect.y + 2, 30, WHITE);  // Increased font size
    }

    // Draw Help menu
    const char* helpText = "Help";
    int helpTextWidth = MeasureText(helpText, 30);  // Increased font size
    helpMenuRect = {optionsMenuRect.x + optionsMenuRect.width + 20, 7, (float)helpTextWidth + 30, 30};  // Increased height and padding
    
    // Draw Help menu button
    Color helpButtonColor = isHelpMenuOpen ? DARKGRAY : BLACK;
    DrawRectangleRec(helpMenuRect, helpButtonColor);
    DrawText(helpText, helpMenuRect.x + 10, 7, 30, WHITE);  // Increased font size
    
    // Draw Help menu dropdown if open
    if (isHelpMenuOpen)
    {
        const char* aboutText = "About";
        int aboutTextWidth = MeasureText(aboutText, 30);  // Increased font size
        aboutOptionRect = {helpMenuRect.x, helpMenuRect.y + helpMenuRect.height,
                          (float)aboutTextWidth + 30, 35};  // Increased height and padding
        DrawRectangleRec(aboutOptionRect, BLACK);
        DrawText(aboutText, aboutOptionRect.x + 10, aboutOptionRect.y + 2, 30, WHITE);  // Increased font size
    }
}

bool Game::HandleMenuInput()
{
    Vector2 mousePos = GetMousePosition();
    
    // Convert screen coordinates to game coordinates
    float gameX = (mousePos.x - (GetScreenWidth() - (gameScreenWidth * scale)) * 0.5f) / scale;
    float gameY = (mousePos.y - (GetScreenHeight() - (gameScreenHeight * scale)) * 0.5f) / scale;
    
    // Check if mouse is over menu bar
    isMenuBarHovered = (gameY >= 0 && gameY <= 45);
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        // Handle custom game popup first
        if (showCustomGamePopup) {
            if (CheckCollisionPointRec({gameX, gameY}, okButtonRect)) {
                // Only process if there's actual input
                if (customGridSizeInputLength > 0) {
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
                    if (size < 5) {
                        currentGridSize = 5;  // Minimum size
                    } else if (size > 20) {
                        currentGridSize = 20;  // Maximum size
                    } else {
                        currentGridSize = size;
                    }
                    Randomize();
                }
                
                showCustomGamePopup = false;
                memset(customGridSizeInput, 0, sizeof(customGridSizeInput));
                customGridSizeInputLength = 0;
                return true;
            }
            // Click outside the popup to cancel
            else if (!CheckCollisionPointRec({gameX, gameY}, popupRect)) {
                showCustomGamePopup = false;
                memset(customGridSizeInput, 0, sizeof(customGridSizeInput));
                customGridSizeInputLength = 0;
                return true;
            }
            // Click inside popup but not on OK button
            else {
                return true;
            }
        }

        // Handle waiting for next level input
        if (waitingForNextLevel) {
            Randomize();
            return true;
        }

        // Handle waiting for game over input
        if (waitingForGameOver) {
            Randomize();
            return true;
        }

        // Handle welcome popup - click anywhere to dismiss
        if (showWelcomePopup) {
            showWelcomePopup = false;
            PlayMusicStream(backgroundMusic);
            isMusicPlaying = true;
            return true;
        }

        if (CheckCollisionPointRec({gameX, gameY}, fileMenuRect))
        {
            isFileMenuOpen = !isFileMenuOpen;
            isHelpMenuOpen = false;
            isOptionsMenuOpen = false;
            return true;
        }
        else if (CheckCollisionPointRec({gameX, gameY}, optionsMenuRect))
        {
            isOptionsMenuOpen = !isOptionsMenuOpen;
            isFileMenuOpen = false;
            isHelpMenuOpen = false;
            return true;
        }
        else if (CheckCollisionPointRec({gameX, gameY}, helpMenuRect))
        {
            isHelpMenuOpen = !isHelpMenuOpen;
            isFileMenuOpen = false;
            isOptionsMenuOpen = false;
            return true;
        }
        else if (isFileMenuOpen)
        {
            // Calculate option rectangles
            Rectangle saveGameOptionRect = {fileMenuRect.x, customGameOptionRect.y + customGameOptionRect.height,
                                         customGameOptionRect.width, 25};
            Rectangle loadGameOptionRect = {fileMenuRect.x, saveGameOptionRect.y + saveGameOptionRect.height,
                                         customGameOptionRect.width, 25};

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
#ifndef __EMSCRIPTEN__
            else if (CheckCollisionPointRec({gameX, gameY}, saveGameOptionRect))
            {
                showSavePopup = true;
                isFileMenuOpen = false;
                memset(filenameInput, 0, sizeof(filenameInput));
                filenameInputLength = 0;
                return true;
            }
            else if (CheckCollisionPointRec({gameX, gameY}, loadGameOptionRect))
            {
                showLoadPopup = true;
                isFileMenuOpen = false;
                memset(filenameInput, 0, sizeof(filenameInput));
                filenameInputLength = 0;
                return true;
            }
            else if (CheckCollisionPointRec({gameX, gameY}, quitOptionRect))
            {
                exitWindowRequested = true;
                isFileMenuOpen = false;
                return true;
            }
#endif
            else
            {
                isFileMenuOpen = false;
                return true;
            }
        }
        else if (isOptionsMenuOpen)
        {
            if (CheckCollisionPointRec({gameX, gameY}, toggleMusicOptionRect))
            {
                // Toggle background music
                if (isMusicPlaying) {
                    PauseMusicStream(backgroundMusic);
                    isMusicPlaying = false;
                } else {
                    ResumeMusicStream(backgroundMusic);
                    isMusicPlaying = true;
                    UpdateMusicStream(backgroundMusic);  // Ensure music starts playing immediately
                }
                isOptionsMenuOpen = false;
                return true;
            }
            else
            {
                isOptionsMenuOpen = false;
                return true;
            }
        }
        else if (showSavePopup)
        {
            // Click outside the popup to cancel
            if (!CheckCollisionPointRec({gameX, gameY}, popupRect))
            {
                showSavePopup = false;
                return true;
            }
            // Click OK button to save
            else if (CheckCollisionPointRec({gameX, gameY}, okButtonRect))
            {
                // Save game state
                std::string filename(filenameInput);
                if (!filename.empty()) {
                    SaveGame(filename);
                }
                showSavePopup = false;
                return true;
            }
        }
        else if (showLoadPopup)
        {
            // Click outside the popup to cancel
            if (!CheckCollisionPointRec({gameX, gameY}, popupRect))
            {
                showLoadPopup = false;
                return true;
            }
            // Click OK button to load
            else if (CheckCollisionPointRec({gameX, gameY}, okButtonRect))
            {
                // Load game state
                std::string filename(filenameInput);
                if (!filename.empty()) {
                    LoadGame(filename);
                }
                showLoadPopup = false;
                return true;
            }
        }
        else if (showHelpPopup)
        {
            // Click anywhere to dismiss the help popup
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
            // Only process if there's actual input
            if (customGridSizeInputLength > 0) {
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
                if (size < 5) {
                    currentGridSize = 5;  // Minimum size
                } else if (size > 20) {
                    currentGridSize = 20;  // Maximum size
                } else {
                    currentGridSize = size;
                }
                Randomize();
            }
            
            showCustomGamePopup = false;
            memset(customGridSizeInput, 0, sizeof(customGridSizeInput));
            customGridSizeInputLength = 0;
            return true;
        }
    }
    
    // Handle text input for save/load popups
    if (showSavePopup || showLoadPopup) {
        int key = GetCharPressed();
        while (key > 0) {
            if (filenameInputLength < sizeof(filenameInput) - 1) {
                filenameInput[filenameInputLength] = (char)key;
                filenameInputLength++;
            }
            key = GetCharPressed();
        }
        
        // Handle backspace
        if (IsKeyPressed(KEY_BACKSPACE) && filenameInputLength > 0) {
            filenameInputLength--;
            filenameInput[filenameInputLength] = '\0';
        }
        
        // Handle enter key
        if (IsKeyPressed(KEY_ENTER)) {
            if (showSavePopup) {
                // Save game state
                std::string filename(filenameInput);
                if (!filename.empty()) {
                    SaveGame(filename);
                }
                showSavePopup = false;
            }
            else if (showLoadPopup) {
                // Load game state
                std::string filename(filenameInput);
                if (!filename.empty()) {
                    LoadGame(filename);
                }
                showLoadPopup = false;
            }
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

void Game::Randomize() {
    try {
#ifdef DEBUG
        std::cout << "Randomizing game with grid size: " << currentGridSize << std::endl;
#endif
        // Only increase grid size if we won and we're not at max size
        int maxSize = isMobile ? MOBILE_MAX_GRID_SIZE : DESKTOP_MAX_GRID_SIZE;
        if (gameWon && currentGridSize < maxSize) {
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
        gameOverTextTimer = 0.0f;  // Reset game over text timer
        gameTime = 0.0f;  // Reset timer
        waitingForNextLevel = false;  // Reset waiting state
        waitingForGameOver = false;  // Reset game over waiting state
        
        // Update scaling to adjust view for new grid size
        UpdateScaling();
#ifdef DEBUG
        std::cout << "Game randomized successfully" << std::endl;
        
        // Save the game state to debug file
        if (SaveGame("debug")) {
            // Print mine positions for debugging
            std::cout << "Mine positions:" << std::endl;
            for (int row = 0; row < currentGridSize; ++row) {
                for (int col = 0; col < currentGridSize; ++col) {
                    std::cout << (grid[row][col].hasMine ? "1" : "0") << " ";
                }
                std::cout << std::endl;
            }
        }
#endif
    } catch (const std::exception& e) {
#ifdef DEBUG
        std::cerr << "Exception in Randomize: " << e.what() << std::endl;
#endif
    } catch (...) {
#ifdef DEBUG
        std::cerr << "Unknown exception in Randomize" << std::endl;
#endif
    }
}

void Game::InitializeGrid() {
    try {
#ifdef DEBUG
        std::cout << "Initializing grid with size: " << currentGridSize << std::endl;
#endif
        grid.resize(currentGridSize, std::vector<Cell>(currentGridSize));
        for (int row = 0; row < currentGridSize; ++row) {
            for (int col = 0; col < currentGridSize; ++col) {
                grid[row][col] = { false, CellState::HIDDEN, 0 };
            }
        }
#ifdef DEBUG
        std::cout << "Grid initialized successfully" << std::endl;
#endif
    } catch (const std::exception& e) {
#ifdef DEBUG
        std::cerr << "Exception in InitializeGrid: " << e.what() << std::endl;
#endif
    } catch (...) {
#ifdef DEBUG
        std::cerr << "Unknown exception in InitializeGrid" << std::endl;
#endif
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
        
        // Skip if this is a corner cell
        bool isCorner = (row == 0 && col == 0) || // top-left
                       (row == 0 && col == currentGridSize - 1) || // top-right
                       (row == currentGridSize - 1 && col == 0) || // bottom-left
                       (row == currentGridSize - 1 && col == currentGridSize - 1); // bottom-right
        
        if (!isCorner && !grid[row][col].hasMine) {
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
    try {
#ifdef DEBUG
        std::cout << "Revealing cell at row=" << row << ", col=" << col << std::endl;
#endif
        if (!IsValidCell(row, col) || grid[row][col].state != CellState::HIDDEN) {
            return;
        }

        grid[row][col].state = CellState::REVEALED;
        remainingCells--;

        if (grid[row][col].hasMine) {
#ifdef DEBUG
            std::cout << "Mine hit at row=" << row << ", col=" << col << std::endl;
#endif
            PlaySound(hitSound);  // Play hit sound when mine is revealed
            gameOver = true;
            gameWon = false;
            waitingForGameOver = true;  // Set flag to wait for player input
            RevealAllMines();
            return;
        }

        PlaySound(actionSound);  // Play action sound for successful reveal

        if (grid[row][col].adjacentMines == 0) {
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    int newRow = row + dr;
                    int newCol = col + dc;
                    if (IsValidCell(newRow, newCol) && 
                        grid[newRow][newCol].state == CellState::HIDDEN) {
                        RevealCell(newRow, newCol);
                    }
                }
            }
        }

        CheckWinCondition();
    } catch (const std::exception& e) {
#ifdef DEBUG
        std::cerr << "Exception in RevealCell: " << e.what() << " at row=" << row << ", col=" << col << std::endl;
#endif
    }
}

void Game::RevealAllMines() {
    try {
#ifdef DEBUG
        std::cout << "Starting RevealAllMines with grid size: " << currentGridSize << std::endl;
        std::cout << "Grid dimensions: rows=" << grid.size() << ", cols=" << (grid.empty() ? 0 : grid[0].size()) << std::endl;
#endif
        
        int minesRevealed = 0;
        for (int row = 0; row < currentGridSize; ++row) {
            for (int col = 0; col < currentGridSize; ++col) {
                try {
                    if (grid[row][col].hasMine) {
#ifdef DEBUG
                        std::cout << "Revealing mine at row=" << row << ", col=" << col 
                                 << ", current state=" << static_cast<int>(grid[row][col].state) << std::endl;
#endif
                        grid[row][col].state = CellState::REVEALED;
                        minesRevealed++;
                    }
                } catch (const std::exception& e) {
#ifdef DEBUG
                    std::cerr << "Error processing cell at row=" << row << ", col=" << col 
                              << ": " << e.what() << std::endl;
#endif
                } catch (...) {
#ifdef DEBUG
                    std::cerr << "Unknown error processing cell at row=" << row << ", col=" << col << std::endl;
#endif
                }
            }
        }
#ifdef DEBUG
        std::cout << "RevealAllMines completed. Total mines revealed: " << minesRevealed << std::endl;
#endif
    } catch (const std::exception& e) {
#ifdef DEBUG
        std::cerr << "Exception in RevealAllMines: " << e.what() << std::endl;
#endif
    } catch (...) {
#ifdef DEBUG
        std::cerr << "Unknown exception in RevealAllMines" << std::endl;
#endif
    }
}

bool Game::IsValidCell(int row, int col) const {
    return row >= 0 && row < currentGridSize && col >= 0 && col < currentGridSize;
}

void Game::CheckWinCondition() {
    if (remainingCells == 0) {
        gameOver = true;
        gameWon = true;
        waitingForNextLevel = true;  // Set flag to wait for player input
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
    if (grid[row][col].state == CellState::REVEALED || grid[row][col].state == CellState::FLAGGED) {
        cellColor = (Color){135, 206, 235, 255};  // Sky blue for revealed and flagged cells
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

void Game::RevealNeighboringMines(int row, int col)
{
#ifdef DEBUG
    std::cout << "Revealing neighboring mines around cell (" << row << ", " << col << ")" << std::endl;
#endif

    try {
        // Check all 8 neighboring cells
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if (i == 0 && j == 0) continue; // Skip the center cell
                
                int newRow = row + i;
                int newCol = col + j;
                
                if (IsValidCell(newRow, newCol)) {
                    if (grid[newRow][newCol].hasMine) {
                        grid[newRow][newCol].state = CellState::REVEALED;
#ifdef DEBUG
                        std::cout << "Revealed mine at (" << newRow << ", " << newCol << ")" << std::endl;
#endif
                    }
                }
            }
        }
    } catch (const std::exception& e) {
#ifdef DEBUG
        std::cout << "Error in RevealNeighboringMines: " << e.what() << std::endl;
#endif
    }
}

void Game::RevealAdjacentCells(int row, int col)
{
#ifdef DEBUG
    std::cout << "Revealing adjacent cells for cell (" << row << ", " << col << ")" << std::endl;
#endif

    try {
        // Count flagged neighbors
        int flaggedNeighbors = 0;
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if (i == 0 && j == 0) continue; // Skip the center cell
                
                int newRow = row + i;
                int newCol = col + j;
                
                if (IsValidCell(newRow, newCol) && grid[newRow][newCol].state == CellState::FLAGGED) {
                    flaggedNeighbors++;
                }
            }
        }

        // If the number of flagged neighbors matches the adjacent mines count
        if (flaggedNeighbors == grid[row][col].adjacentMines) {
            // Check if any flagged cell is not a mine (mistake)
            bool mistakeMade = false;
            for (int i = -1; i <= 1; i++) {
                for (int j = -1; j <= 1; j++) {
                    if (i == 0 && j == 0) continue;
                    
                    int newRow = row + i;
                    int newCol = col + j;
                    
                    if (IsValidCell(newRow, newCol) && 
                        grid[newRow][newCol].state == CellState::FLAGGED && 
                        !grid[newRow][newCol].hasMine) {
                        mistakeMade = true;
                        break;
                    }
                }
                if (mistakeMade) break;
            }

            if (mistakeMade) {
                // Reveal only the neighboring mines to show the mistake
                RevealNeighboringMines(row, col);
                gameOver = true;
                waitingForGameOver = true;  // Set flag to wait for player input
                return;
            }

            // Reveal all non-flagged adjacent cells
            for (int i = -1; i <= 1; i++) {
                for (int j = -1; j <= 1; j++) {
                    if (i == 0 && j == 0) continue;
                    
                    int newRow = row + i;
                    int newCol = col + j;
                    
                    if (IsValidCell(newRow, newCol) && 
                        grid[newRow][newCol].state != CellState::FLAGGED) {
                        if (grid[newRow][newCol].hasMine) {
                            // Hit a mine - reveal only neighboring mines
                            RevealNeighboringMines(newRow, newCol);
                            gameOver = true;
                            waitingForGameOver = true;  // Set flag to wait for player input
                            return;
                        }
                        RevealCell(newRow, newCol);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
#ifdef DEBUG
        std::cout << "Error in RevealAdjacentCells: " << e.what() << std::endl;
#endif
    }
}

void Game::ResetToInitialSize() {
    currentGridSize = isMobile ? MOBILE_INITIAL_GRID_SIZE : DESKTOP_INITIAL_GRID_SIZE;
    Randomize();
}

#ifdef DEBUG
void Game::InitializeDebugGrid() {
    try {
        std::cout << "Initializing debug grid with predefined mine pattern" << std::endl;
        
        // Predefined mine pattern (1 = mine, 0 = safe)
        const int debugMines[5][5] = {
            {0, 0, 0, 0, 0},
            {0, 1, 0, 0, 0},
            {0, 1, 0, 0, 0},
            {0, 0, 0, 0, 0},
            {0, 0, 1, 0, 0}
        };

        // Set grid size to match debug pattern
        currentGridSize = 5;
        grid.clear();
        grid.resize(currentGridSize, std::vector<Cell>(currentGridSize));

        // Initialize grid with debug pattern
        for (int row = 0; row < currentGridSize; ++row) {
            for (int col = 0; col < currentGridSize; ++col) {
                grid[row][col] = { 
                    debugMines[row][col] == 1,  // hasMine
                    CellState::HIDDEN,          // state
                    0                           // adjacentMines (will be calculated)
                };
            }
        }

        // Calculate adjacent mines
        CalculateAdjacentMines();
        remainingCells = currentGridSize * currentGridSize - CalculateMineCount();
        gameOver = false;
        gameWon = false;
        gameTime = 0.0f;

        // Update scaling for the new grid
        UpdateScaling();

        std::cout << "Debug grid initialized successfully" << std::endl;
        std::cout << "Mine positions:" << std::endl;
        for (int row = 0; row < currentGridSize; ++row) {
            for (int col = 0; col < currentGridSize; ++col) {
                std::cout << (grid[row][col].hasMine ? "1" : "0") << " ";
            }
            std::cout << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in InitializeDebugGrid: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in InitializeDebugGrid" << std::endl;
    }
}
#endif

bool Game::SaveGame(const std::string& filename) {
    try {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
#ifdef DEBUG
            std::cerr << "Failed to open file for saving: " << filename << std::endl;
#endif
            return false;
        }

        // Save grid size
        file.write(reinterpret_cast<const char*>(&currentGridSize), sizeof(currentGridSize));
        
        // Save grid state
        for (int row = 0; row < currentGridSize; ++row) {
            for (int col = 0; col < currentGridSize; ++col) {
                file.write(reinterpret_cast<const char*>(&grid[row][col].hasMine), sizeof(bool));
                file.write(reinterpret_cast<const char*>(&grid[row][col].state), sizeof(CellState));
                file.write(reinterpret_cast<const char*>(&grid[row][col].adjacentMines), sizeof(int));
            }
        }
        
        // Save game state
        file.write(reinterpret_cast<const char*>(&gameOver), sizeof(bool));
        file.write(reinterpret_cast<const char*>(&gameWon), sizeof(bool));
        file.write(reinterpret_cast<const char*>(&gameTime), sizeof(float));
        file.write(reinterpret_cast<const char*>(&remainingCells), sizeof(int));
        file.write(reinterpret_cast<const char*>(&remainingMines), sizeof(int));
        
        file.close();
#ifdef DEBUG
        std::cout << "Game saved to " << filename << std::endl;
#endif
        return true;
    } catch (const std::exception& e) {
#ifdef DEBUG
        std::cerr << "Error saving game: " << e.what() << std::endl;
#endif
        return false;
    }
}

bool Game::LoadGame(const std::string& filename) {
    try {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
#ifdef DEBUG
            std::cerr << "Failed to open file for loading: " << filename << std::endl;
#endif
            return false;
        }

        // Load grid size
        int loadedGridSize;
        file.read(reinterpret_cast<char*>(&loadedGridSize), sizeof(loadedGridSize));
        
        // Resize grid
        currentGridSize = loadedGridSize;
        grid.clear();
        grid.resize(currentGridSize, std::vector<Cell>(currentGridSize));
        
        // Load grid state
        for (int row = 0; row < currentGridSize; ++row) {
            for (int col = 0; col < currentGridSize; ++col) {
                file.read(reinterpret_cast<char*>(&grid[row][col].hasMine), sizeof(bool));
                file.read(reinterpret_cast<char*>(&grid[row][col].state), sizeof(CellState));
                file.read(reinterpret_cast<char*>(&grid[row][col].adjacentMines), sizeof(int));
            }
        }
        
        // Load game state
        file.read(reinterpret_cast<char*>(&gameOver), sizeof(bool));
        file.read(reinterpret_cast<char*>(&gameWon), sizeof(bool));
        file.read(reinterpret_cast<char*>(&gameTime), sizeof(float));
        file.read(reinterpret_cast<char*>(&remainingCells), sizeof(int));
        file.read(reinterpret_cast<char*>(&remainingMines), sizeof(int));
        
        file.close();
        
        // Update scaling for the loaded grid
        UpdateScaling();
#ifdef DEBUG
        std::cout << "Game loaded from " << filename << std::endl;
#endif
        return true;
    } catch (const std::exception& e) {
#ifdef DEBUG
        std::cerr << "Error loading game: " << e.what() << std::endl;
#endif
        return false;
    }
}