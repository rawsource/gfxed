#ifndef GFXED_CONSTANTS_H
#define GFXED_CONSTANTS_H

// M A C R O S

#define PI 3.14159265
#define DEGRAD PI / 180
#define RADDEG 180 / PI


// T Y P E D E F S

typedef unsigned char BYTE;

using json = nlohmann::json;


// P R O T O T Y P E S


// E N U M S

enum class SelectMode { VERTEX, TRIANGLE, BOX };
enum class CursorMode { CROSSHAIR, CIRCLE };
enum class EditMode { NONE, TWEAK, GRAB, SCALE, ROTATE, COLORIZE };


// V A R I A B L E S

/*

1234567890-=
!@#$%^&*()_+
qwertyuiopasdfghjklzxcvbnm
QWERTYUIOPASDFGHJKLZXCVBNM
[]\;',./{}|:"<>?

*/

const std::string CHARS = " 1234567890qwertyuiopasdfghjklzxcvbnm,./:-";

const int SCREEN_WIDTH = 1920; // 1280;
const int SCREEN_HEIGHT = 1080; // 720;
const int SCREEN_WIDTH_HALF = SCREEN_WIDTH / 2;
const int SCREEN_HEIGHT_HALF = SCREEN_HEIGHT / 2;
const int PALETTE_SIZE = 32;
const float ZOOM_INCREMENT = 0.5;
const float SELECT_RADIUS = 10.0;
const float TWEAK_RADIUS = 10.0;
const float SNAP_RADIUS = 10.0;
const float NODE_SIZE = 2.5;
const int MAX_LAYERS = 9;

// ui colors
const Color CYAN = {0.0, 1.0, 1.0, 1.0};
const Color YELLOW = {1.0, 1.0, 0.0, 1.0};
const Color MAGENTA = {1.0, 0.0, 1.0, 1.0};
const Color BLACK = {0.0, 0.0, 0.0, 1.0};
const Color WHITE = {1.0, 1.0, 1.0, 1.0};
const Color GREY125 = {0.125, 0.125, 0.125, 1.0};
const Color GREY25 = {0.25, 0.25, 0.25, 1.0};
const Color GREY50 = {0.5, 0.5, 0.5, 1.0};
const Color GREY75 = {0.75, 0.75, 0.75, 1.0};

const unsigned int GRIDSIZES[7] = {1, 5, 10, 15, 20, 30, 60};

#endif

