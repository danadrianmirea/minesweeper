#pragma once
#include <raylib.h>

// Debug flag - comment out to disable debug logging
#define DEBUG

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

//#define AM_RAY_DEBUG

extern Color black;
extern Color darkGreen;
extern Color grey;
extern Color yellow;
extern const int gameScreenWidth;
extern const int gameScreenHeight;
extern bool exitWindow;
extern bool exitWindowRequested;
extern bool fullscreen;
extern const int minimizeOffset;
extern float borderOffsetWidth;
extern float borderOffsetHeight;
extern const int offset;

// Minesweeper constants
const int GRID_SIZE = 8;
const int CELL_SIZE = 50;
const int NUM_MINES = 10;
const float MUSIC_VOLUME = 0.33f;

// Cell states
enum class CellState {
    HIDDEN,
    REVEALED,
    FLAGGED
};
