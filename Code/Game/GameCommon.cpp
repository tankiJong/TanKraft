#include "GameCommon.hpp"

const char* APP_NAME = "Proto - Tanki Zhang";

bool        g_isQuitting  = false;   // ...becomes App::m_isQuitting
Renderer*   g_theRenderer = nullptr;
Input*      g_theInput    = nullptr;
BitmapFont* g_defaultFont = nullptr;
Window*     g_theWindow   = nullptr;
Console*    g_theConsole  = nullptr;

Camera*     g_theUiCamera = nullptr;

const mat44 gGameCoordsTransform = {
    0, -1, 0, 0,
    0,  0, 1, 0,
    1,  0, 0, 0,
    0,  0, 0, 1,
  };