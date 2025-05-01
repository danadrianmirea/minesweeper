#include <raylib.h>
#include "globals.h"

Color black = BLACK;
Color darkGreen = DARKGREEN;
Color grey = GRAY;
Color yellow = YELLOW;
int windowWidth = 1920;
int windowHeight = 1080;
const int gameScreenWidth = 960;
const int gameScreenHeight = 540;
bool exitWindowRequested = false;
bool exitWindow = false;
bool fullscreen = false;
const int minimizeOffset = 20;
float borderOffsetWidth = 0.0f;
float borderOffsetHeight = 0.0f;
const int offset = 10;

// Game elements
const char* BOMB_CHAR = "X";
const char* FLAG_CHAR = "F";