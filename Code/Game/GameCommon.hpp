#pragma once

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/Primitives/mat44.hpp"
class Renderer;
class Input;
class BitmapFont;
class Window;
class Console;
class Camera;
extern const    char* APP_NAME;
extern unsigned char  g_isCheating;

extern bool        g_isQuitting;
extern Renderer*   g_theRenderer;
extern Input*      g_theInput;
extern Window*     g_theWindow;
extern BitmapFont* g_defaultFont;
extern Console*    g_theConsole;
extern Camera*     g_theUiCamera;

constexpr float CLIENT_ASPECT = 1.77f;
constexpr float SCREEN_HALF_HEIGHT = 3.f;
constexpr float SCREEN_HALF_WIDTH = SCREEN_HALF_HEIGHT * CLIENT_ASPECT;


constexpr float SCREEN_WIDTH = SCREEN_HALF_WIDTH * 2.f; // units
constexpr float SCREEN_HEIGHT = SCREEN_HALF_HEIGHT * 2.f; // units

extern const mat44 gGameCoordsTransform;