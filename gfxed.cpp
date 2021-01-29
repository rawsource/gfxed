
// IMPORTANT
// fix selection on triangle swap (move forward/backward)
// fix selection on winding change
// copy and delete are bugged and need to be fixed (only allow by triangle)
// no tweak in triangle select mode
// any mode (eg. select or grab) should cancel each other
// left control in box selection should remove from selection
// export per layer
// tile grid setup and preview 3x3

// VERY NICE
// select vertex of single triangle
// make REAL triangle select mode (multi select)
// key i insert mode and tesselate on apply
// drag vertex in direction of normal

// SIMPLE
// ctrl + rotate/scale in increments
// use vim theme colors for editor
// cleanup: improve loaddata file loading handling
// select palette index onclick
// no commands allowed in preview mode

// MISC
// scale and rotate from custom position (mark center with some key combination)
// multi color font strings with color variables
// static font class using displaylists
// implement LuaJIT
// cleanup: use auto keyword where possible
// cleanup: use floats for everything
// refactor into multiple files for faster compilation and project structure
// try to use a finite state machine for all modes
// remove doubles command (triangles)
// re-introduce vertex opacity
// fix bug where it's difficult to snap to the bounds of the grid
// rotate quad to swap triangles

// NEW
// keyframe animations
// multiple file buffers
// command history
// command suggestions and autocomplete (shortest possible sequence of letters)
// set center point of file
// show warnings about files or paths existing or not
// write command should create directories
// show feedback of commands
// do not crash on incomplete or wrong commands


#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <map>
#include <vector>
#include <regex>

#include "json.hpp"

#include "structs.h"
#include "constants.h"


// V A R I A B L E S

int gridindex = 4;
int GRID_SIZE = GRIDSIZES[gridindex];

// mouse
int mx;
int my;
int imx;
int imy;

float fmx;
float fmy;
float ifmx;
float ifmy;

float sx;
float sy;

float zsx;
float zsy;
float zoom = 1.0;
float translatex = 0.0;
float translatey = 0.0;

bool LMB;
bool MMB;
bool RMB;
bool LCTRL;
bool LSHIFT;

// helper
bool SELECT = true;

// ---
int tri;

// ---
bool DRAGGING = false;

// ---
bool MODELINE = false;
std::string modeline_text = "";

// constraints
bool XCONSTRAINT = false;
bool YCONSTRAINT = false;

// ---
float initialscale;
float initialrotation;
float scaledistance;

float scale;
float rotation;

float selectradius = SELECT_RADIUS;
float tweakradius = TWEAK_RADIUS;
float snapradius = SNAP_RADIUS;

SelectMode selectmode = SelectMode::VERTEX;
CursorMode cursor = CursorMode::CROSSHAIR;
EditMode editmode = EditMode::NONE;

Mesh *mesh;
std::vector<int> selection;
std::map<std::string, Mesh> data;

GLuint listindex;

// buffers
std::vector<unsigned int> color_buffer;
std::vector<Vertex> vertex_buffer;

// palette
std::vector<Color> palette;
unsigned int palette_index = 0;

// view
int view = 0;
int last_view = 0;
std::vector<View> views;
bool PREVIEWMODE = false;

// undo
int undolevel = -1;
std::vector<json> undo_buffer;

// cmd history
int historylevel = -1;
std::vector<std::string> cmd_history;

// file
std::string file = "gfxed";

// layer
int layer = 0;
bool LAYERS[MAX_LAYERS] = {false, false, false, false, false, false, false, false, false};

// app
bool quit = false;

// SDL
SDL_Window* window = NULL;
SDL_GLContext context;


// C L A S S E S

bool insidetriangle(float Ax, float Ay, float Bx, float By, float Cx, float Cy, float Px, float Py);

class Font
{
	public:
		Font();
		~Font();
		void load();
		void render(GLfloat x, GLfloat y, std::string text);
	private:
		std::map<int, Mesh> data;
};

// fonts
Font *font;

Font::Font()
{
	// constructor
	load();
}

Font::~Font()
{
	// deconstructor
}

void Font::load()
{

	for ( unsigned int i = 0; i < CHARS.length(); ++i )
	{
		// ascii
		unsigned int ascii = (unsigned char)CHARS[i];

		// filename
		std::string filename = "data/fonts/fixedsys/";
		filename += std::to_string(ascii);
		filename += ".bin";

		// read
		std::ifstream ifs(filename, std::ios::binary);
		ifs.unsetf(std::ios::skipws);
		std::vector<BYTE> vec;
		vec.insert(vec.begin(), std::istream_iterator<BYTE>(ifs), std::istream_iterator<BYTE>());

		// parse
		json j = json::from_msgpack(vec);

		auto vertexdata = j["data"];

		for ( auto& element : vertexdata )
		{
			Vertex v;
			v.x = element["x"];
			v.y = element["y"];
			v.x = v.x - SCREEN_WIDTH_HALF;
			v.y = v.y - SCREEN_HEIGHT_HALF;
			v.z = 0;
			data[ascii].vertices.push_back(v);
		}
	}
}

void Font::render(GLfloat x, GLfloat y, std::string text)
{
	float wx = 0;

	glPushMatrix();

		glTranslatef( x, y, 0 );
		glScalef( 0.035, 0.035, 1 );

		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

		glBegin( GL_TRIANGLES );

			for ( unsigned int i = 0; i < text.length(); ++i )
			{
				// ascii
				unsigned int ascii = (unsigned char)text[i];
				for ( unsigned int j = 0; j < data[ascii].vertices.size(); j++ )
				{
					glVertex2f( data[ascii].vertices[j].x + wx, data[ascii].vertices[j].y );
				}
				wx += 256;
			}

		glEnd();

	glPopMatrix();
}


// F U N C T I O N S

void init_views()
{
	View v;
	v.VERTEX = false;
	v.TRIANGLE = false;
	v.FILL = false;
	v.HIDDEN = false;
	views.push_back(v);
	views.push_back(v);
	// preview mode
	v.FILL = true;
	views.push_back(v);
}

bool less_comp(const Vertex & v1, const Vertex & v2)
{
	if ( v1.x > v2.x )
		return false;
	if ( v1.x < v2.x )
		return true;
	// x's are equal if we reach here
	if ( v1.y > v2.y )
		return false;
	if ( v1.y < v2.y )
		return true;
	// both coordinates equal if we reach here
	return false;
}

bool equal_comp(const Vertex & v1, const Vertex & v2)
{
	if ( v1.x == v2.x and v1.y == v2.y )
		return true;
	return false;
}

std::vector<Vertex> getuniqueselection()
{
	std::vector<Vertex> _vertices;
	for ( unsigned int i = 0; i < selection.size(); i++ ) {
		_vertices.push_back(mesh->vertices[selection[i]]);
	}
	std::sort( _vertices.begin(), _vertices.end(), less_comp );
	std::vector<Vertex>::iterator it = std::unique( _vertices.begin(), _vertices.end(), equal_comp );
	_vertices.resize( std::distance( _vertices.begin(), it ) );
	return _vertices;
}

json serialize()
{
	json res;
	json vertexdata;
	json palettedata;
	json selectiondata;
	// vertices
	for ( unsigned int i = 0; i < mesh->vertices.size(); i++ )
	{
		Vertex v = mesh->vertices[i];
		json o;
		o["x"] = v.x;
		o["y"] = v.y;
		o["z"] = v.z;
		// reference to palette index
		o["c"] = mesh->colors[i];
		vertexdata.push_back(o);
	}
	// palette
	for ( unsigned int i = 0; i < palette.size(); i++ )
	{
		json o;
		Color c = palette[i];
		o["r"] = c.r;
		o["g"] = c.g;
		o["b"] = c.b;
		o["a"] = c.a;
		palettedata.push_back(o);
	}
	// selection
	for ( unsigned int i = 0; i < selection.size(); i++ )
	{
		selectiondata.push_back(selection[i]);
	}
	res["vertexdata"] = vertexdata;
	res["palettedata"] = palettedata;
	res["selectiondata"] = selectiondata;
	return res;
}

void deserialize(json jsondata)
{
	palette.clear();
	selection.clear();
	mesh->colors.clear();
	mesh->vertices.clear();
	auto vertexdata = jsondata["vertexdata"];
	auto palettedata = jsondata["palettedata"];
	auto selectiondata = jsondata["selectiondata"];
	// vertices
	for (auto& element : vertexdata) {
		Vertex v;
		v.x = element["x"];
		v.y = element["y"];
		v.z = element["z"];
		mesh->vertices.push_back(v);
		// reference to palette index
		unsigned int c = element["c"];
		mesh->colors.push_back(c);
	}
	// palette
	for (auto& element : palettedata) {
		Color c;
		c.r = element["r"];
		c.g = element["g"];
		c.b = element["b"];
		c.a = element["a"];
		palette.push_back(c);
	}
	// selection
	for (auto& element : selectiondata) {
		selection.push_back(element);
	}
}

void pushcmdhistory()
{
	historylevel++;
}

void pushundostate()
{
	undolevel++;
	if ( undolevel != (int) undo_buffer.size() ) {
		undo_buffer.erase(undo_buffer.begin() + undolevel, undo_buffer.end());
	}
	undo_buffer.push_back(serialize());
}

void undo(bool revert)
{
	if ( not revert )
		undolevel--;
	if ( undolevel < 0 )
		undolevel = 0;
	json jsondata = undo_buffer[undolevel];
	deserialize( jsondata );
}

void redo()
{
	undolevel++;
	if ( undolevel == (int) undo_buffer.size() )
		undolevel = undo_buffer.size() - 1;
	json jsondata = undo_buffer[undolevel];
	deserialize( jsondata );
}

void datachanged()
{
	pushundostate();
}

void resetundobuffer()
{
	undolevel = -1;
	undo_buffer.clear();
	datachanged();
}

void fixwinding()
{
	for ( unsigned int i = 0; i < mesh->vertices.size(); i += 3 )
	{
		float x1 = mesh->vertices[i].x;
		float y1 = mesh->vertices[i].y;
		float x2 = mesh->vertices[i + 1].x;
		float y2 = mesh->vertices[i + 1].y;
		float x3 = mesh->vertices[i + 2].x;
		float y3 = mesh->vertices[i + 2].y;

		float cx = (x1 + x2 + x3) / 3;
		float cy = (y1 + y2 + y3) / 3;

		if ( insidetriangle( x1, y1, x2, y2, x3, y3, cx, cy ) )
		{
			std::swap( mesh->vertices[i + 1], mesh->vertices[i + 2] );
			std::swap( mesh->colors[i + 1], mesh->colors[i + 2] );
			// WARN: make assumption that selection is of size 1
			if ( selection.size() == 1 ) {
				/*
				if ( selection[i] % 3 == 0 ) {
					SDL_Log("0");
					// selection[0] = i;
				}
				if ( selection[i] % 3 == 1 ) {
					SDL_Log("1");
					//selection[0] = i + 2;
				}
				if ( selection[i] % 3 == 2 ) {
					SDL_Log("2");
					//selection[0] = i + 1;
				}
				*/
			}
			break;
		}
	}
}

std::vector<BYTE> readFile(std::string filename)
{
	// open the file
	std::ifstream ifs(filename, std::ios::binary);

	// stop eating new lines in binary mode
	ifs.unsetf(std::ios::skipws);

	// get its size
	// std::streampos fileSize;

	// ifs.seekg(0, std::ios::end);
	// fileSize = ifs.tellg();
	// ifs.seekg(0, std::ios::beg);

	// reserve capacity
	std::vector<BYTE> vec;
	// vec.reserve(fileSize);

	// read the data
	vec.insert(vec.begin(), std::istream_iterator<BYTE>(ifs), std::istream_iterator<BYTE>());

	return vec;
}

inline bool file_exists(std::string filename)
{
	std::ifstream f(filename.c_str());
	return f.good();
}

std::string get_filename()
{
	std::string filename = "data/";
	filename += file;
	return filename;
}

void defaultpalette()
{
	for ( unsigned i = 0; i < PALETTE_SIZE; i++ )
	{
		Color c;
		c.r = 0.25f;
		c.g = 0.25f;
		c.b = 0.25f;
		c.a = 1;
		palette.push_back(c);
	}
}

void loaddata(std::string name)
{
	auto filename = get_filename();
	if ( ! file_exists(filename) ) {
		defaultpalette();
		return;
	}

	auto v = readFile(filename);
	json j = json::from_msgpack(v);

	auto palettedata = j["palette"];
	for ( auto& element : palettedata )
	{
		Color c;
		c.r = element["r"];
		c.g = element["g"];
		c.b = element["b"];
		c.a = 1;
		palette.push_back(c);
	}

	auto vertexdata = j["data"];
	for ( auto& element : vertexdata )
	{
		Vertex v;
		v.x = element["x"];
		v.y = element["y"];
		v.z = element["z"];
		data[name].vertices.push_back(v);

		// reference to palette index
		unsigned int c = element["c"];
		data[name].colors.push_back(c);
	}

	auto prefs1 =j["prefs1"];
	views[0].VERTEX = prefs1["VERTEX"];
	views[0].TRIANGLE = prefs1["TRIANGLE"];
	views[0].FILL = prefs1["FILL"];
	views[0].HIDDEN = prefs1["HIDDEN"];

	auto prefs2 =j["prefs2"];
	views[1].VERTEX = prefs2["VERTEX"];
	views[1].TRIANGLE = prefs2["TRIANGLE"];
	views[1].FILL = prefs2["FILL"];
	views[1].HIDDEN = prefs2["HIDDEN"];

	if ( palettedata.size() > 0 ) return;

	defaultpalette();
}

bool checklayer(int l)
{
	if ( layer == 0 or PREVIEWMODE ) {
		return true;
	}
	for ( int i = 0; i < MAX_LAYERS; i++ ) {
		if ( l-1 == i and LAYERS[i] ) {
			return true;
		}
	}
	return false;
}

void exportdata()
{
	json data;

	for ( int l = 1; l < MAX_LAYERS+1; l++ )
	{
		for ( unsigned int i = 0; i < mesh->vertices.size(); i++ )
		{
			if ( checklayer( mesh->vertices[i].z ) and l == mesh->vertices[i].z ) {
				json o;
				o["r"] = palette[mesh->colors[i]].r;
				o["g"] = palette[mesh->colors[i]].g;
				o["b"] = palette[mesh->colors[i]].b;
				o["a"] = palette[mesh->colors[i]].a;
				o["x"] = mesh->vertices[i].x;
				o["y"] = mesh->vertices[i].y;
				o["z"] = mesh->vertices[i].z;
				data.push_back(o);
			}
		}
	}
			std::string s = data.dump();
			std::ofstream ofs;
			ofs.open("export.json");
			ofs << s;
			ofs.close();
	
}

void savedata()
{
	json main;
	json data;
	json palettedata;
	json prefs1;
	json prefs2;

	// data
	for (unsigned int i = 0; i < mesh->vertices.size(); i++)
	{
		json o;
		Vertex v = mesh->vertices[i];
		o["x"] = v.x; // std::trunc(100 * v.x) / 100
		o["y"] = v.y;
		o["z"] = v.z;

		// reference to palette index
		o["c"] = mesh->colors[i];

		data.push_back(o);
	}
	main["data"] = data;

	// palette
	for ( auto& element : palette )
	{
		json o;
		o["r"] = element.r;
		o["g"] = element.g;
		o["b"] = element.b;
		palettedata.push_back(o);
	}
	main["palette"] = palettedata;

	// prefs
	prefs1["VERTEX"] = views[0].VERTEX;
	prefs1["TRIANGLE"] = views[0].TRIANGLE;
	prefs1["FILL"] = views[0].FILL;
	prefs1["HIDDEN"] = views[0].HIDDEN;

	prefs2["VERTEX"] = views[1].VERTEX;
	prefs2["TRIANGLE"] = views[1].TRIANGLE;
	prefs2["FILL"] = views[1].FILL;
	prefs2["HIDDEN"] = views[1].HIDDEN;

	main["prefs1"] = prefs1;
	main["prefs2"] = prefs2;

	// serialize to msgpack
	std::vector<uint8_t> v = json::to_msgpack(main);

	auto filename = get_filename();
	std::ofstream ofs(filename, std::ios::out | std::ios::binary);
	ofs.write(reinterpret_cast<const char*> (v.data()), v.size());
	ofs.close();

	exportdata();
}

void setlayer(int l)
{
	for (unsigned int i = 0; i < selection.size(); i++)
	{
		mesh->vertices[selection[i]].z = l;
	}
	selection.clear();
	datachanged();
}

void togglelayer(int l)
{
	if ( LAYERS[l-1] )
		LAYERS[l-1] = false;
	else
		LAYERS[l-1] = true;
}

void showlayer(int l)
{
	if ( layer == l or l == 0 ) {
		layer = l;
		return;
	}
	layer = l;
	selection.clear();
}

float roundUp(float number, float fixedBase)
{
	if (fixedBase != 0 && number != 0) {
		float sign = number > 0 ? 1 : -1;
		number *= sign;
		number /= fixedBase;
		int fixedPoint = (int) ceil(number);
		number = fixedPoint * fixedBase;
		number *= sign;
	}
	return number;
}

void scalefromvector(float amount, bool type)
{
	float offsetx = sx;
	float offsety = sy;
	// selection
	for (unsigned int i = 0; i < selection.size(); i++)
	{
		float vx = mesh->vertices[selection[i]].x - offsetx;
		float vy = mesh->vertices[selection[i]].y - offsety;
		if (! type) {
			vx = vx / amount;
			vy = vy / amount;
		} else {
			vx = vx * amount;
			vy = vy * amount;
		}
		if ( not YCONSTRAINT )
			mesh->vertices[selection[i]].x = vx + offsetx;
		if ( not XCONSTRAINT )
			mesh->vertices[selection[i]].y = vy + offsety;
	}
}

void rotatearoundvector(float angle)
{
	float cx = sx;
	float cy = sy;
	for ( unsigned int i = 0; i < selection.size(); i++ )
	{
		mesh->vertices[selection[i]].x -= cx;
		mesh->vertices[selection[i]].y -= cy;
		float s = sin(angle);
		float c = cos(angle);
		float px = mesh->vertices[selection[i]].x;
		float py = mesh->vertices[selection[i]].y;
		float nx = px * c - py * s;
		float ny = px * s + py * c;
		mesh->vertices[selection[i]].x = nx + cx;
		mesh->vertices[selection[i]].y = ny + cy;
	}
}

void setselectioncenter(float & x1, float & y1)
{
	float minx = 0;
	float maxx = 0;
	float miny = 0;
	float maxy = 0;
	for ( unsigned int i = 0; i < selection.size(); i ++ )
	{
		Vertex v = mesh->vertices[selection[i]];
		if ( i == 0 )
		{
			minx = v.x;
			maxx = v.x;
			miny = v.y;
			maxy = v.y;
			continue;
		}
		if ( v.x < minx ) minx = v.x;
		if ( v.x > maxx ) maxx = v.x;
		if ( v.y < miny ) miny = v.y;
		if ( v.y > maxy ) maxy = v.y;
	}
	x1 = ( minx + maxx ) * 0.5; // /2
	y1 = ( miny + maxy ) * 0.5; // /2
}

bool insidetriangle(float Ax, float Ay, float Bx, float By, float Cx, float Cy, float Px, float Py)
{
	float ax, ay, bx, by, cx, cy, apx, apy, bpx, bpy, cpx, cpy;
	float cCROSSap, bCROSScp, aCROSSbp;

	ax = Cx - Bx; ay = Cy - By;
	bx = Ax - Cx; by = Ay - Cy;
	cx = Bx - Ax; cy = By - Ay;
	apx = Px - Ax; apy = Py - Ay;
	bpx = Px - Bx; bpy = Py - By;
	cpx = Px - Cx; cpy = Py - Cy;

	aCROSSbp = ax*bpy - ay*bpx;
	cCROSSap = cx*apy - cy*apx;
	bCROSScp = bx*cpy - by*cpx;

	return ((aCROSSbp >= 0.0f) && (bCROSScp >= 0.0f) && (cCROSSap >= 0.0f));
}

bool incircle(float x1, float y1, float x2, float y2, float radius)
{
	float dx = x2 - (x1);
	float dy = y2 - (y1);
	return dx * dx + dy * dy <= radius * radius;
}

int vertexhover(float x, float y)
{
	for ( unsigned int i = 0; i < mesh->vertices.size(); i++ )
	{
		if ( incircle( x, y, mesh->vertices[i].x, mesh->vertices[i].y, tweakradius / zoom ) )
		{
			return i;
		}
	}
	return -1;
}

float getdirection(float x1, float y1, float x2, float y2)
{
	return atan2(y2 - y1, x2 - x1);
}

float calculatedistance(float x1, float y1, float x2, float y2)
{
	float dx = abs(x1 - x2);
	float dy = abs(y1 - y2);
	return sqrt((dx * dx) + (dy * dy));
}

bool inselection(int index)
{
	for ( unsigned int i = 0; i < selection.size(); i ++ )
	{
		if ( index == selection[i] ) return true;
	}
	return false;
}

void drawrectangle(float x, float y, float w, float h)
{
	glPushMatrix();
		glTranslatef( x, y, 0 );
		glBegin( GL_QUADS );
			glVertex2f( 0, 0) ;
			glVertex2f( 0, h );
			glVertex2f( w, h );
			glVertex2f( w, 0 );
		glEnd();
	glPopMatrix();
}

void drawcircle(float cx, float cy, float r, int num_segments)
{
	float theta = 2 * 3.1415926 / float(num_segments);
	float c = cosf(theta);
	float s = sinf(theta);
	float t;

	float x = r;
	float y = 0;

	glBegin(GL_LINE_LOOP);
	for (int i = 0; i < num_segments; i++)
	{
		glVertex2f(x + cx, y + cy);

		t = x;
		x = c * x - s * y;
		y = s * t + c * y;
	}
	glEnd();
}

void drawcrosshair(float cx, float cy)
{
	glBegin(GL_LINES);
		glVertex2f(cx - selectradius, cy);
		glVertex2f(cx - 1, cy);
		glVertex2f(cx + selectradius, cy);
		glVertex2f(cx, cy);
		glVertex2f(cx, cy - selectradius);
		glVertex2f(cx, cy - 1);
		glVertex2f(cx, cy + selectradius);
		glVertex2f(cx, cy);
	glEnd();
}

void makelist()
{
	listindex = glGenLists(1);
	glNewList(listindex, GL_COMPILE);
	glBegin(GL_TRIANGLES);
		for ( unsigned int i = 0; i < mesh->vertices.size(); i++ )
		{
			glColor4f(palette[mesh->colors[i]].r, palette[mesh->colors[i]].g, palette[mesh->colors[i]].b, palette[mesh->colors[i]].a);
			glVertex2f(mesh->vertices[i].x, mesh->vertices[i].y);
		}
	glEnd();
	glEndList();
}

bool initGL()
{

	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho( 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f );

	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ) ;
	// glBlendEquation( GL_FUNC_SUBTRACT );

	// glEnable(GL_DEPTH_TEST);

	glClearColor( 0.1, 0.1, 0.1, 1.0 );

	makelist();

	return true;
}

bool initSDL(bool wo)
{
	init_views();

	loaddata("mesh");
	mesh = &data["mesh"];
	resetundobuffer();

	SDL_Init( SDL_INIT_VIDEO );

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 1 );

	window = SDL_CreateWindow( "GFX.Ed v0.1", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN );

	context = SDL_GL_CreateContext( window );

	if ( wo ) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Opacity Mode", "Opacity mode is enabled, so you can trace stuff in the background.", NULL);
		SDL_SetWindowOpacity(window, 0.75);
	}

	SDL_GL_SetSwapInterval( 1 );

	SDL_CaptureMouse( SDL_TRUE );
	// SDL_ShowCursor( SDL_DISABLE );

	SDL_Cursor *cur = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_CROSSHAIR );
	SDL_SetCursor( cur );

	initGL();

	return true;
}

void update()
{
	SDL_GetMouseState( &mx, &my );

	if ( not YCONSTRAINT )
		fmx = (mx / zoom) - translatex;
	if ( not XCONSTRAINT )
		fmy = (my / zoom) - translatey;

	if ( LMB )
	{
		// select vertex
		if ( selectmode == SelectMode::VERTEX and SELECT )
		{
			for ( unsigned int i = 0; i < mesh->vertices.size(); i++ )
			{
				if ( not checklayer( mesh->vertices[i].z ) ) continue;
				float x = mesh->vertices[i].x;
				float y = mesh->vertices[i].y;
				if ( incircle( fmx, fmy, x, y, selectradius / zoom ) )
				{
					// deselect
					if ( LCTRL ) {
						// different in selection
						for ( unsigned int j = 0; j < selection.size(); j ++ ) {
							if ( (int) i == selection[j] ) {
								selection.erase(selection.begin() + j);
							}
						}
					// select
					} else {
						if ( ! inselection(i) )
							selection.push_back(i);
					}
				}
			}
		}
		// select triangle
		if ( selectmode == SelectMode::TRIANGLE )
		{
			if ( tri != -3 ) {
				for (int i = 0; i < 3; i++)
				{
					if ( ! inselection( tri + i ) )
						selection.push_back( tri + i );
				}
			}
			SELECT = false;
		}

	}

	// pan
	if ( MMB )
	{
		translatex = translatex + (fmx - ifmx);
		imx = mx;
		translatey = translatey + (fmy - ifmy);
		imy = my;
	}

	if ( RMB )
	{
		// tweak
		if ( editmode == EditMode::TWEAK )
		{
			for ( unsigned int i = 0; i < selection.size(); i++ )
			{
				float vx = mesh->vertices[selection[i]].x;
				float vy = mesh->vertices[selection[i]].y;
				float ox = fmx - ifmx;
				float oy = fmy - ifmy;
				mesh->vertices[selection[i]].x = vx + ox;
				mesh->vertices[selection[i]].y = vy + oy;
			}
			ifmx = fmx;
			ifmy = fmy;
		}
	}

	// grab
	if ( editmode == EditMode::GRAB )
	{
		for ( unsigned int i = 0; i < selection.size(); i++ )
		{
			float vx = mesh->vertices[selection[i]].x;
			float vy = mesh->vertices[selection[i]].y;
			float ox = fmx - ifmx;
			float oy = fmy - ifmy;
			mesh->vertices[selection[i]].x = vx + ox;
			mesh->vertices[selection[i]].y = vy + oy;
		}
		ifmx = fmx;
		ifmy = fmy;
	}

	// scale
	if ( editmode == EditMode::SCALE )
	{
		initialscale = 1 + (initialscale / 100);
		scale = calculatedistance( imx, imy, mx, my );
		float sd = calculatedistance( sx, sy, mx, my );
		if ( sd < scaledistance ) {
			scale = scale * -1;
		}
		scale = 1 + (scale / 100);
		scale = 1 + (scale - initialscale);
		/*
		// increments
		if ( LCTRL )
			scale = roundUp( scale, 0.1 );
		*/
		scaledistance = sd;
		scalefromvector( scale, true );
		imx = mx;
		imy = my;
		initialscale = 0;
	}

	// rotate
	if ( editmode == EditMode::ROTATE )
	{
		rotatearoundvector( -rotation );
		rotation = getdirection( sx, sy, mx, my );
		rotation = rotation - initialrotation;
		// increments
		/*
		if ( LCTRL )
			rotation = roundUp( rotation, 15 * DEGRAD );
		*/
		rotatearoundvector( rotation );
	}

	tri = -3;
	if ( selectmode == SelectMode::TRIANGLE )
	{
		for ( unsigned int i = 0; i < mesh->vertices.size(); i += 3 )
		{
			if ( not checklayer( mesh->vertices[i].z ) ) continue;
			float x1 = mesh->vertices[i].x;
			float y1 = mesh->vertices[i].y;
			float x2 = mesh->vertices[i + 1].x;
			float y2 = mesh->vertices[i + 1].y;
			float x3 = mesh->vertices[i + 2].x;
			float y3 = mesh->vertices[i + 2].y;
			if ( insidetriangle( x3, y3, x2, y2, x1, y1, fmx, fmy ) )
			{
				tri = i;
				break;
			}
		}
	}

	// colorize
	if ( editmode == EditMode::COLORIZE )
	{
		// get color
		unsigned char pickcol[3];
		glReadPixels( mx , SCREEN_HEIGHT - 1 - my, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pickcol) ;
		palette[palette_index].r = pickcol[0] / 255.0f;
		palette[palette_index].g = pickcol[1] / 255.0f;
		palette[palette_index].b = pickcol[2] / 255.0f;
		palette[palette_index].a = 1;
	}

	// snap
	if ( LCTRL and (editmode == EditMode::GRAB or editmode == EditMode::TWEAK) )
	{
		if ( editmode == EditMode::GRAB or editmode == EditMode::TWEAK )
		{
			// get closest vertex to mouse position
			int closest_vertex = 0; // [= 0] -Werror=maybe-uninitialized
			float closest_distance = 999999999;
			for ( unsigned int i = 0; i < selection.size(); i++ ) {
				auto vertex_distance = calculatedistance(
						fmx, fmy, mesh->vertices[selection[i]].x, mesh->vertices[selection[i]].y);
				if ( vertex_distance < closest_distance ) {
					closest_distance = vertex_distance;
					closest_vertex = selection[i];
				}
			}
			// old position
			float oldx = mesh->vertices[closest_vertex].x;
			float oldy = mesh->vertices[closest_vertex].y;
			// new position
			float newx = roundUp(fmx - GRID_SIZE / 2, GRID_SIZE);
			float newy = roundUp(fmy - GRID_SIZE / 2, GRID_SIZE);
			// grid
			if ( LSHIFT ) {
				for ( unsigned int i = 0; i < selection.size(); i++ ) {
					mesh->vertices[selection[i]].x += newx - oldx;
					mesh->vertices[selection[i]].y += newy - oldy;
				}
			}
			// vertex
			if ( ! LSHIFT ) {
				bool snapped = false;
				for ( unsigned int i = 0; i < mesh->vertices.size(); i++ )
				{
					// don't snap to self
					if ( (int)i == closest_vertex ) continue;
					// only snap to visible layers
					if ( checklayer( mesh->vertices[i].z ) ) {
						if ( incircle( mesh->vertices[closest_vertex].x, mesh->vertices[closest_vertex].y, mesh->vertices[i].x, mesh->vertices[i].y, snapradius / zoom ) )
						{
							newx = mesh->vertices[i].x;
							newy = mesh->vertices[i].y;
							snapped = true;
							break;
						}
					}
				}
				if ( snapped ) {
					for ( unsigned int i = 0; i < selection.size(); i++ ) {
						mesh->vertices[selection[i]].x += newx - oldx;
						mesh->vertices[selection[i]].y += newy - oldy;
					}
				}
			}
		}
	}

}

inline void renderUIStage()
{
	// stage bounds
	glColor3fv( &GREY25.r );
	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	glRectf( -1.0, -1.0, SCREEN_WIDTH + 1.0, SCREEN_HEIGHT + 1.0 );

	if ( not PREVIEWMODE ) {

		// grid
		glColor3fv( &GREY125.r );
		glBegin(GL_LINES);
		for  ( int i = 0; i < SCREEN_HEIGHT / GRID_SIZE; i++ ) {
			glVertex2f( 0, (i+1) * GRID_SIZE );
			glVertex2f( SCREEN_WIDTH, (i+1) * GRID_SIZE );
		}
		for  ( int i = 0; i < SCREEN_WIDTH / GRID_SIZE; i++ ) {
			glVertex2f( (i+1) * GRID_SIZE, 0 ); // could set 0 to 1
			glVertex2f( (i+1) * GRID_SIZE, SCREEN_HEIGHT );
		}
		glEnd();

		// center
		glColor3fv( &GREY25.r );
		glBegin(GL_LINES);
			glVertex2f( SCREEN_WIDTH_HALF, 0 );
			glVertex2f( SCREEN_WIDTH_HALF, SCREEN_HEIGHT );
			glVertex2f( 0, SCREEN_HEIGHT_HALF );
			glVertex2f( SCREEN_WIDTH, SCREEN_HEIGHT_HALF );
		glEnd();

	}
}

inline void renderGeometry()
{
	// colored geometry
	if ( views[view].FILL )
	{
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

		glBegin(GL_TRIANGLES);
			for ( int l = 1; l < MAX_LAYERS+1; l++ )
			{
				for ( unsigned int i = 0; i < mesh->vertices.size(); i++ )
				{
					if ( checklayer( mesh->vertices[i].z ) and l == mesh->vertices[i].z ) {
						glColor4f(palette[mesh->colors[i]].r, palette[mesh->colors[i]].g, palette[mesh->colors[i]].b, palette[mesh->colors[i]].a);
						glVertex2f(mesh->vertices[i].x, mesh->vertices[i].y);
					}
				}
			}
		glEnd();
	}

	if ( not PREVIEWMODE ) {

		// triangles
		if ( views[view].TRIANGLE )
		{
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

			glBegin(GL_TRIANGLES);
				for ( unsigned int i = 0; i < mesh->vertices.size(); i++ )
				{
					if ( checklayer( mesh->vertices[i].z ) ) {
						glColor3fv( &GREY50.r );
						glVertex2f(mesh->vertices[i].x, mesh->vertices[i].y);
					} else if ( not views[view].HIDDEN ) {
						glColor3fv( &GREY25.r );
						glVertex2f(mesh->vertices[i].x, mesh->vertices[i].y);
					}
				}
			glEnd();
		}

		// vertices
		if ( views[view].VERTEX )
		{
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

			// vertices
			for ( unsigned int i = 0; i < mesh->vertices.size(); i++ )
			{
				if ( checklayer( mesh->vertices[i].z ) )
				{
					glColor3fv( &GREY50.r );
					float x = mesh->vertices[i].x;
					float y = mesh->vertices[i].y;
					float x1 = x - NODE_SIZE / zoom;
					float y1 = y - NODE_SIZE / zoom;
					float x2 = x + NODE_SIZE / zoom;
					float y2 = y + NODE_SIZE / zoom;
					glRectf( x1, y1, x2, y2 );
				} else if ( not views[view].HIDDEN ) {
					glColor3fv( &GREY25.r );
					float x = mesh->vertices[i].x;
					float y = mesh->vertices[i].y;
					float x1 = x - NODE_SIZE / zoom;
					float y1 = y - NODE_SIZE / zoom;
					float x2 = x + NODE_SIZE / zoom;
					float y2 = y + NODE_SIZE / zoom;
					glRectf( x1, y1, x2, y2 );
				}
			}
		}

		// visualize selection
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		glColor3fv( &YELLOW.r );
		for ( unsigned int i = 0; i < selection.size(); i++ )
		{
			float x = mesh->vertices[selection[i]].x;
			float y = mesh->vertices[selection[i]].y;
			float x1 = x - NODE_SIZE / zoom;
			float y1 = y - NODE_SIZE / zoom;
			float x2 = x + NODE_SIZE / zoom;
			float y2 = y + NODE_SIZE / zoom;
			glRectf( x1, y1, x2, y2 );
		}

		// selected triangle
		if ( tri != -3 )
		{
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
			glColor3fv( &YELLOW.r );
			glBegin(GL_TRIANGLES);
				for ( int i = 0; i < 3; i++ )
				{
					glVertex2f(mesh->vertices[tri + i].x, mesh->vertices[tri + i].y);
				}
			glEnd();
		}
	}
}

inline void renderUIHelpers()
{
	if ( not PREVIEWMODE ) {

		// tool indicators
		if ( editmode == EditMode::SCALE || editmode == EditMode::ROTATE )
		{
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
			glColor3fv( &MAGENTA.r );
			glBegin(GL_LINES);
				glVertex2f(sx, sy);
				glVertex2f(fmx, fmy);
			glEnd();
		}

		// BOX select mode
		if ( DRAGGING )
		{
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
			glColor3fv( &YELLOW.r );
			glBegin(GL_QUADS);
				glVertex2f(ifmx, ifmy);
				glVertex2f(ifmx, fmy);
				glVertex2f(fmx, fmy);
				glVertex2f(fmx, ifmy);
			glEnd();
		}

	}
}

inline void renderModeLine()
{
	// modeline
	glPushMatrix();

		// view indicator
		glColor3fv( &WHITE.r );
		font->render(10, 10, "view " + std::to_string(view+1));

		// black rect
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		glColor3fv( &BLACK.r );
		glBegin(GL_QUADS);
			glVertex2f(0, SCREEN_HEIGHT - 32);
			glVertex2f(0, SCREEN_HEIGHT);
			glVertex2f(SCREEN_WIDTH, SCREEN_HEIGHT);
			glVertex2f(SCREEN_WIDTH, SCREEN_HEIGHT - 32);
		glEnd();

		// modeline input
		glColor3fv( &WHITE.r );
		if ( MODELINE ) {
			font->render(5, SCREEN_HEIGHT - 20, modeline_text.c_str());
		} else {
			font->render(5, SCREEN_HEIGHT - 20, file);
		}

		// layers

		int w = 220;
		for ( int i = 0; i < MAX_LAYERS; i++ ) {
			glColor3fv( &GREY25.r );
			if ( LAYERS[i] )
				glColor3fv( &GREY125.r );
			drawrectangle( SCREEN_WIDTH - w, SCREEN_HEIGHT - 30, 18, 28 );
			if ( layer == i + 1 ) {
				// active layer indicator
				glColor3fv( &GREY50.r );
				drawrectangle( SCREEN_WIDTH - w, SCREEN_HEIGHT - 4, 18, 2 );
			}
			w = w - 20;
		}

		glColor3fv( &GREY125.r );
		drawrectangle( SCREEN_WIDTH - 40,  SCREEN_HEIGHT - 30, 38, 28 );

		glColor3fv( &GREY50.r );
		font->render( SCREEN_WIDTH - 215, SCREEN_HEIGHT - 20, "1");
		font->render( SCREEN_WIDTH - 195, SCREEN_HEIGHT - 20, "2");
		font->render( SCREEN_WIDTH - 175, SCREEN_HEIGHT - 20, "3");
		font->render( SCREEN_WIDTH - 155, SCREEN_HEIGHT - 20, "4");
		font->render( SCREEN_WIDTH - 135, SCREEN_HEIGHT - 20, "5");
		font->render( SCREEN_WIDTH - 115, SCREEN_HEIGHT - 20, "6");
		font->render( SCREEN_WIDTH - 95, SCREEN_HEIGHT - 20, "7");
		font->render( SCREEN_WIDTH - 75, SCREEN_HEIGHT - 20, "8");
		font->render( SCREEN_WIDTH - 55, SCREEN_HEIGHT - 20, "9");

	glPopMatrix();

	// UI: x
	glPushMatrix();
		glColor3fv( &CYAN.r );
		font->render( SCREEN_WIDTH_HALF, SCREEN_HEIGHT - 20, "x: " + std::to_string(fmx) );
		font->render( SCREEN_WIDTH_HALF + 150, SCREEN_HEIGHT - 20, "y: " + std::to_string(fmy) );
		glColor3fv( &YELLOW.r );
		font->render( SCREEN_WIDTH_HALF + 300, SCREEN_HEIGHT - 20, "verts: " + std::to_string(selection.size()) );
	glPopMatrix();
}

inline void renderPalette()
{

	glPushMatrix();

		// palette
		float h1 = 0.0;
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		for ( unsigned int i = 0; i < palette.size(); i += 2 )
		{
			Color c = palette[i];
			glColor3f( c.r, c.g, c.b );
			drawrectangle( SCREEN_WIDTH - 80, h1, 38, 18 );
			if ( i == palette_index ) {
				glColor3fv( &WHITE.r );
				drawrectangle( SCREEN_WIDTH - 80, h1, 5, 5 ); // should this line exists outside the condition
			}
			c = palette[i+1];
			glColor3f( c.r, c.g, c.b );
			drawrectangle( SCREEN_WIDTH - 40, h1, 38, 18 );
			if ( i+1 == palette_index ) {
				glColor3fv( &WHITE.r );
				drawrectangle( SCREEN_WIDTH - 40, h1, 5, 5 ); // should this line exists outside the condition
			}
			glColor3fv( &WHITE.r );
			font->render( SCREEN_WIDTH - 104, h1 + 4, std::to_string(i) );
			h1 = h1 + 20.0;
		}

	glPopMatrix();
}

void render()
{
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	glPushMatrix();

		glScalef( zoom, zoom, zoom );
		glTranslatef( translatex, translatey, 0.0 );

		renderUIStage();

		/*

		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

		// enable vertex arrays
		glEnableClientState( GL_VERTEX_ARRAY );
		glEnableClientState( GL_COLOR_ARRAY );

		glColorPointer( 4, GL_FLOAT, sizeof(Color), &mesh->colors[0] );
		glVertexPointer( 2, GL_FLOAT, sizeof(Vertex), &mesh->vertices[0] );

		glDrawArrays( GL_TRIANGLES, 0, mesh->vertices.size() );

		// disable vertex arrays
		glDisableClientState( GL_VERTEX_ARRAY );
		glDisableClientState( GL_COLOR_ARRAY );

		*/

		// display lists
		// glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		// glCallList(listindex);

		renderGeometry();

		renderUIHelpers();

	glPopMatrix();

	// fo not render at all below
	if ( PREVIEWMODE )
		return;

	renderModeLine();

	renderPalette();

	glPushMatrix();

		// cursor
		glColor3fv( &YELLOW.r );
		if ( cursor == CursorMode::CIRCLE and SELECT )
		{
			drawcircle(mx, my, selectradius, 16);
		}
		if ( cursor == CursorMode::CROSSHAIR )
		{
			// drawcrosshair(mx, my);
		}

	glPopMatrix();

}

void handleMouseWheel(SDL_Event e)
{
	if ( e.type == SDL_MOUSEWHEEL )
	{
		// no mode
		if ( editmode == EditMode::NONE ) {
			// down
			if ( e.wheel.y < 0 ) {
				selectradius = selectradius - 5.0;
				if ( selectradius < SELECT_RADIUS ) {
					selectradius = SELECT_RADIUS;
				}
			}
			// up
			if ( e.wheel.y > 0 ) {
				selectradius = selectradius + 5.0;
			}
		}
	}

}

void handleMouseDown(SDL_Event e)
{
	if ( e.type == SDL_MOUSEBUTTONDOWN )
	{
		switch ( e.button.button )
		{
			case SDL_BUTTON_MIDDLE:
				MMB = true;
				imx = mx;
				imy = my;
				ifmx = fmx;
				ifmy = fmy;
				break;

			case SDL_BUTTON_LEFT:
				// start box select
				if ( selectmode == SelectMode::BOX ) {
					imx = mx;
					imy = my;
					ifmx = fmx;
					ifmy = fmy;
					DRAGGING = true;
				}
				// no mode active
				if ( (!LSHIFT and !LCTRL) and editmode == EditMode::NONE) {
					selection.clear();
				}
				// mode active
				if (editmode != EditMode::NONE) {
					datachanged();
					SELECT = false;
				}
				LMB = true;
				editmode = EditMode::NONE;
				cursor = CursorMode::CIRCLE;
				fixwinding();
				break;

			case SDL_BUTTON_RIGHT:
				// mode active
				if (editmode != EditMode::NONE) {
					undo(true);
				}
				RMB = true;
				editmode = EditMode::NONE;
				imx = mx;
				imy = my;
				ifmx = fmx;
				ifmy = fmy;
				// tweak
				int found = vertexhover( fmx, fmy );
				if ( found != -1 )
				{
					if ( inselection(found) ) {
						// grab
						editmode = EditMode::GRAB;
					}
					else {
						selection.clear();
						for ( unsigned int i = 0; i < mesh->vertices.size(); i++ )
						{
							if ( not checklayer( mesh->vertices[i].z ) ) continue;
							float x = mesh->vertices[i].x;
							float y = mesh->vertices[i].y;
							if ( incircle( fmx, fmy, x, y, tweakradius / zoom ) ) {
								selection.push_back(i);
							}
						}
						// tweak
						if ( selection.size() > 0 ) {
							editmode = EditMode::TWEAK;
						}
					}
				}
				break;
		}
	}
}

void handleMouseUp(SDL_Event e)
{
	if ( e.type == SDL_MOUSEBUTTONUP )
	{
		switch ( e.button.button )
		{
			case SDL_BUTTON_MIDDLE:
				MMB = false;
				XCONSTRAINT = false;
				YCONSTRAINT = false;
				break;

			case SDL_BUTTON_LEFT:
				LMB = false;
				SELECT = true;
				XCONSTRAINT = false;
				YCONSTRAINT = false;
				// end box select
				if ( selectmode == SelectMode::BOX ) {
					DRAGGING = false;
					// ---
					float x1 = ifmx;
					float x2 = fmx;
					if ( x1 > x2 ) {
						x1 = fmx;
						x2 = ifmx;
					}
					float y1 = ifmy;
					float y2 = fmy;
					if ( y1 > y2 ) {
						y1 = fmy;
						y2 = ifmy;
					}
					// get selection from rectangle
					for (unsigned int i = 0; i < mesh->vertices.size(); i++) {
						Vertex v = mesh->vertices[i];
						if ( inselection(i) ) continue;
						if ( not checklayer( v.z ) ) continue;
						if ( v.x > x1 and v.x < x2 and v.y > y1 and v.y < y2) {
							selection.push_back(i);
						}
					}
				}
				cursor = CursorMode::CROSSHAIR;
				// revert to default select mode
				if ( selectmode == SelectMode::BOX ) {
					selectmode = SelectMode::VERTEX;
				}
				break;

			case SDL_BUTTON_RIGHT:
				RMB = false;
				XCONSTRAINT = false;
				YCONSTRAINT = false;
				if ( editmode != EditMode::NONE ) {
					datachanged();
				}
				editmode = EditMode::NONE;
				fixwinding();
				break;
		}
	}
}

void handleMouseEvents(SDL_Event e)
{
	if ( MODELINE )
		return;

	if ( PREVIEWMODE )
		return;

	handleMouseWheel(e);
	handleMouseDown(e);
	handleMouseUp(e);
}

void handleModeLine(SDL_Event e)
{
	if ( e.type == SDL_KEYDOWN && e.key.repeat == 0 )
	{
		switch ( e.key.keysym.sym )
		{
			// exit
			case SDLK_ESCAPE:
				MODELINE = false;
				SDL_StopTextInput();
				break;

			// confirm
			case SDLK_RETURN:
			{
				std::string cmd = "";

				// palette
				cmd = modeline_text.substr( 0, 9 );
				if ( cmd.size() == 9 and cmd.compare(":palette ") == 0 )
				{
					std::string val = modeline_text.substr( 9, -1 );
					// split by space
					std::regex rgx("\\s+");
					std::sregex_token_iterator iter( val.begin(), val.end(), rgx, -1 );
					std::sregex_token_iterator end;
					// get index
					int index = std::stoi(*iter);
					// set index
					palette_index = index;
					// get color
					if ( std::distance(iter, end) == 2 )
					{
						iter++;
						std::string hexcolor = *iter;
						Color c;
						int r, g, b;
						sscanf(hexcolor.c_str(), "%02x%02x%02x", &r, &g, &b);
						c.r = r / 255.0f;
						c.g = g / 255.0f;
						c.b = b / 255.0f;
						c.a = 1;
						palette[index] = c;
					}
					datachanged();
				}

				// color
				cmd = modeline_text.substr( 0, 7 );
				if ( cmd.size() == 7 and cmd.compare(":color ") == 0 )
				{
					std::string val = modeline_text.substr( 7, -1 );

					// split by space
					std::regex rgx("\\s+");
					std::sregex_token_iterator iter( val.begin(), val.end(), rgx, -1 );
					std::sregex_token_iterator end;
					// get index
					int index = std::stoi(*iter);
					// set
					if ( std::distance(iter, end) == 1 )
					{
						for ( unsigned int i = 0; i < selection.size(); i++ ) {
							mesh->colors[selection[i]] = index;
						}
						datachanged();
					}
					// select
					if ( std::distance(iter, end) == 2 )
					{
						iter++;
						std::string subcmd = *iter;
						if ( subcmd == "select" )
						{
							selection.clear();
							for ( unsigned int i = 0; i < mesh->vertices.size(); i++ ) {
								if ( (int)mesh->colors[i] == index )
									selection.push_back(i);
							}
						}
					}
				}

				// mirror
				// TODO: fix winding
				cmd = modeline_text.substr( 0, 8 );
				if ( cmd.size() == 8 and cmd.compare(":mirror ") == 0 ) {
					std::string val = modeline_text.substr( 8, -1 );
					if ( selection.size() > 1 )
					{
						if ( val == "x" ) {
							setselectioncenter(sx, sy);
							for ( unsigned int i = 0; i < selection.size(); i++ ) {
								float x = mesh->vertices[selection[i]].x;
								mesh->vertices[selection[i]].x = sx - ( x - sx );
							}
						} else if ( val == "y" ) {
							setselectioncenter(sx, sy);
							for ( unsigned int i = 0; i < selection.size(); i++ ) {
								float y = mesh->vertices[selection[i]].y;
								mesh->vertices[selection[i]].y = sy - ( y - sy );
							}
						}
					}
				}

				// rotate
				cmd = modeline_text.substr( 0, 8 );
				if ( cmd.size() == 8 and cmd.compare(":rotate ") == 0 ) {
					float val = std::stof( modeline_text.substr( 8, -1 ) );
					setselectioncenter(sx, sy);
					rotatearoundvector(val * DEGRAD);
					datachanged();
				}

				// scale
				cmd = modeline_text.substr( 0, 7 );
				if ( cmd.size() == 7 and cmd.compare(":scale ") == 0 ) {
					float val = std::stof( modeline_text.substr( 7, -1 ) );
					setselectioncenter(sx, sy);
					if ( val < 0 ) {
						scalefromvector(val, false);
					} else {
						scalefromvector(val, true);
					}
					datachanged();
				}

				// toggle boolean options
				cmd = modeline_text.substr( 0, 8 );
				if ( cmd.size() == 8 and cmd.compare(":toggle ") == 0 ) {
					std::string val = modeline_text.substr( 8, -1 );
					if ( val  == "vertex" ) {
						views[view].VERTEX = !views[view].VERTEX;
					} else if ( val == "triangle" ) {
						views[view].TRIANGLE = !views[view].TRIANGLE;
					} else if ( val == "fill" ) {
						views[view].FILL = !views[view].FILL;
					} else if ( val == "hidden" ) {
						views[view].HIDDEN = !views[view].HIDDEN;
					}
				}

				cmd = modeline_text.substr( 0, 3 );

				// single commands
				if ( cmd.size() == 2 )
				{
					// write
					if ( cmd.compare(":w") == 0 ) {
						if ( file.size() > 0 ) {
							savedata();
						}
					}
					// quit
					if ( cmd.compare(":q") == 0 ) {
						quit = true;
					}
				}
				// multi commands
				if ( cmd.size() == 3 )
				{
					std::string filepath = modeline_text.substr( 3, -1 );
					// write and quit
					if ( cmd.compare(":wq") == 0 ) {
						if ( file.size() > 0 ) {
							savedata();
						}
						quit = true;
					}
					// write <path>
					if ( cmd.compare(":w ") == 0 ) {
						if ( filepath.size() > 0 ) {
							file = filepath;
							savedata();
						}
					}
					// edit <path>
					if ( cmd.compare(":e ") == 0 ) {
						if ( filepath.size() > 0 ) {
							palette.clear();
							mesh->vertices.clear();
							mesh->colors.clear();
							selection.clear();
							resetundobuffer();
							file = filepath;
							loaddata("mesh");
							mesh = &data["mesh"];
							resetundobuffer();
						}
					}
				}
				MODELINE = false;
				SDL_StopTextInput();
				break;
			}

			// backspace
			case SDLK_BACKSPACE:
				if ( modeline_text.size() > 1 ) {
					modeline_text.pop_back();
				}
				break;
		}
	}
	// modeline text input
	if ( e.type == SDL_TEXTINPUT ) {
		std::string input = e.text.text;
		// skip characters that are not available
		for ( auto s:CHARS ) {
			if ( input.compare(std::string(1, s)) == 0 ) {
				modeline_text += input;
			}
		}
	}
}

void handleKeyDownRepeat(SDL_Event e)
{
	if ( e.type != SDL_KEYDOWN )
		return;

	if ( not PREVIEWMODE )
	{
		switch ( e.key.keysym.sym )
		{
			// grid
			case SDLK_LEFTBRACKET:
				gridindex--;
				if ( gridindex < 0 )
					gridindex = 0;
				GRID_SIZE = GRIDSIZES[gridindex];
				break;
			case SDLK_RIGHTBRACKET:
				gridindex++;
				if ( gridindex == 7 )
					gridindex--;
				GRID_SIZE = GRIDSIZES[gridindex];
				break;

			// move selection
			case SDLK_LEFT:
			{
				float offset = GRID_SIZE;
				if ( LSHIFT )
					offset = GRIDSIZES[0];
				for ( unsigned i = 0; i < selection.size(); i++ ) {
					mesh->vertices[selection[i]].x -= offset;
				}
				break;
			}
			case SDLK_RIGHT:
			{
				float offset = GRID_SIZE;
				if ( LSHIFT )
					offset = GRIDSIZES[0];
				for ( unsigned i = 0; i < selection.size(); i++ ) {
					mesh->vertices[selection[i]].x += offset;
				}
				break;
			}
			case SDLK_UP:
			{
				float offset = GRID_SIZE;
				if ( LSHIFT )
					offset = GRIDSIZES[0];
				for ( unsigned i = 0; i < selection.size(); i++ ) {
					mesh->vertices[selection[i]].y -= offset;
				}
				break;
			}
			case SDLK_DOWN:
			{
				float offset = GRID_SIZE;
				if ( LSHIFT )
					offset = GRIDSIZES[0];
				for ( unsigned i = 0; i < selection.size(); i++ ) {
					mesh->vertices[selection[i]].y += offset;
				}
				break;
			}
		}
	}

	switch ( e.key.keysym.sym )
	{
		// zoomin
		case SDLK_EQUALS:
			// set zoom focus
			if ( LSHIFT ) {
				setselectioncenter(zsx, zsy);
				translatex = -(zsx - SCREEN_WIDTH_HALF / zoom );
				translatey = -(zsy - SCREEN_HEIGHT_HALF / zoom );
			}
			// zoom in
			if ( not LSHIFT ) {
				if ( zoom == 0.25 )
					zoom = 0.5;
				else
					zoom = zoom + ZOOM_INCREMENT;
				if ( zsx == 0.0 and zsy == 0.0 ) {
					translatex = ( SCREEN_WIDTH_HALF / zoom ) - SCREEN_WIDTH_HALF;
					translatey = ( SCREEN_HEIGHT_HALF / zoom ) - SCREEN_HEIGHT_HALF;
				}
				else {
					translatex = -( zsx - SCREEN_WIDTH_HALF / zoom );
					translatey = -( zsy - SCREEN_HEIGHT_HALF / zoom );
				}
			}
			break;

		// zoom out
		case SDLK_MINUS:
			if ( LSHIFT ) {
				zoom = 1.0;
				translatex = 0.0;
				translatey = 0.0;
				zsx = 0.0;
				zsy = 0.0;
			}
			if ( not LSHIFT ) {
				zoom = zoom - ZOOM_INCREMENT;
				if ( zoom < ZOOM_INCREMENT ) zoom = 0.25; //ZOOM_INCREMENT;
				if ( zsx == 0.0 and zsy == 0.0 ) {
					translatex = ( SCREEN_WIDTH_HALF / zoom ) - SCREEN_WIDTH_HALF;
					translatey = ( SCREEN_HEIGHT_HALF / zoom ) - SCREEN_HEIGHT_HALF;
				}
				else {
					translatex = -( zsx - SCREEN_WIDTH_HALF / zoom );
					translatey = -( zsy - SCREEN_HEIGHT_HALF / zoom );
				}
			}
			break;
	}
}

void handleKeyDown(SDL_Event e)
{
	if ( e.type == SDL_KEYDOWN && e.key.repeat == 0 )
	{}
	else
		return;

	switch ( e.key.keysym.sym )
	{
		case SDLK_LCTRL:
			LCTRL = true;
			break;

		case SDLK_LSHIFT:
			LSHIFT = true;
			break;

		// mode toggle
		case SDLK_TAB:
			if ( LSHIFT ) {
				if ( PREVIEWMODE ) {
					PREVIEWMODE = false;
					view = last_view;
				} else {
					PREVIEWMODE = true;
					last_view = view;
					view = 2;
				}
				break;
			}
			break;
	}

	if ( PREVIEWMODE )
		return;

	switch ( e.key.keysym.sym )
	{
		// mode toggle
		case SDLK_TAB:
			if ( not LSHIFT ) {
				// toggle view
				if ( view == 1 )
					view = 0;
				else
					view = 1;
				break;
			}
			break;

		// layers
		case SDLK_0:
			showlayer(0);
			break;
		case SDLK_1:
			if ( LSHIFT )
				togglelayer(1);
			else if ( LCTRL ) {
				setlayer(1);
			} else {
				showlayer(1);
			}
			break;
		case SDLK_2:
			if ( LSHIFT )
				togglelayer(2);
			else if ( LCTRL ) {
				setlayer(2);
			} else {
				showlayer(2);
			}
			break;
		case SDLK_3:
			if ( LSHIFT )
				togglelayer(3);
			else if ( LCTRL ) {
				setlayer(3);
			} else {
				showlayer(3);
			}
			break;
		case SDLK_4:
			if ( LSHIFT )
				togglelayer(4);
			else if ( LCTRL ) {
				setlayer(4);
			} else {
				showlayer(4);
			}
			break;
		case SDLK_5:
			if ( LSHIFT )
				togglelayer(5);
			else if ( LCTRL ) {
				setlayer(5);
			} else {
				showlayer(5);
			}
			break;
		case SDLK_6:
			if ( LSHIFT )
				togglelayer(6);
			else if ( LCTRL ) {
				setlayer(6);
			} else {
				showlayer(6);
			}
			break;
		case SDLK_7:
			if ( LSHIFT )
				togglelayer(7);
			else if ( LCTRL ) {
				setlayer(7);
			} else {
				showlayer(7);
			}
			break;
		case SDLK_8:
			if ( LSHIFT )
				togglelayer(8);
			else if ( LCTRL ) {
				setlayer(8);
			} else {
				showlayer(8);
			}
			break;
		case SDLK_9:
			if ( LSHIFT )
				togglelayer(9);
			else if ( LCTRL ) {
				setlayer(9);
			} else {
				showlayer(9);
			}
			break;

		case SDLK_PAGEDOWN:
		{
			// swap layer backwards
			if ( LCTRL )
			{
				if ( layer == 0 ) break;
				if ( layer == 1 ) break;
				for ( unsigned i = 0; i < mesh->vertices.size(); i++ ) {
					if ( mesh->vertices[i].z == layer ) {
						mesh->vertices[i].z--;
					}
					else if ( mesh->vertices[i].z == layer - 1  ) {
						mesh->vertices[i].z++;
					}
				}
				layer--;
			}
			// move triangle backwards
			else
			{
				if ( tri == -3 ) break;
				if ( tri - 3 == 0 ) break;
				auto it1 = mesh->vertices.begin();
				std::swap_ranges( it1 + tri, it1 + tri + 3, it1 + tri - 3 );
				auto it2 = mesh->colors.begin();
				std::swap_ranges( it2 + tri, it2 + tri + 3, it2 + tri - 3 );
			}
			datachanged();
			break;
		}

		case SDLK_PAGEUP:
		{
			// swap layer forwards
			if ( LCTRL )
			{
				if ( layer == 0 ) break;
				if ( layer == MAX_LAYERS ) break;
				for ( unsigned i = 0; i < mesh->vertices.size(); i++ ) {
					if ( mesh->vertices[i].z == layer ) {
						mesh->vertices[i].z++;
					}
					else if ( mesh->vertices[i].z == layer + 1  ) {
						mesh->vertices[i].z--;
					}
				}
				layer++;
			}
			// move triangle forwards
			else
			{
				if ( tri == -3 ) break;
				if ( (unsigned int)tri + 3 == mesh->vertices.size() ) break;
				auto it1 = mesh->vertices.begin();
				std::swap_ranges( it1 + tri, it1 + tri + 3, it1 + tri + 3 );
				auto it2 = mesh->colors.begin();
				std::swap_ranges( it2 + tri, it2 + tri + 3, it2 + tri + 3 );
			}
			datachanged();
			break;
		}

		// history
		case SDLK_u:
			if ( LSHIFT ) {
				redo();
			}
			else {
				undo(false);
			}
			break;

		// toggle select mode
		case  SDLK_z:
			if (selectmode == SelectMode::VERTEX) {
				selectmode = SelectMode::TRIANGLE;
			}
			else {
				selectmode = SelectMode::VERTEX;
			}
			break;

		// box select
		case SDLK_b:
		{
			selectmode = SelectMode::BOX;
			cursor = CursorMode::CROSSHAIR;
			break;
		}

		// quantize
		case SDLK_q:
			if ( selection.size() > 0 ) {
				for ( unsigned int i = 0; i < selection.size(); i++ ) {
					mesh->vertices[selection[i]].x = roundUp(mesh->vertices[selection[i]].x - GRID_SIZE / 2, GRID_SIZE);
					mesh->vertices[selection[i]].y = roundUp(mesh->vertices[selection[i]].y - GRID_SIZE / 2, GRID_SIZE);
				}
				datachanged();
			}
			break;

		// select all
		case SDLK_a:
			if ( selection.size() == 0 ) {
				for ( unsigned int i = 0; i < mesh->vertices.size(); i++ )
				{
					if ( not checklayer( mesh->vertices[i].z ) ) continue;
					selection.push_back(i);
				}
			}
			else {
				selection.clear();
			}
			break;

		// select linked
		case SDLK_l:
		{
			int iter = 0;
			bool done = false;
			if ( selection.size() == 0 ) break;
			while ( not done )
			{
				// select related vertices
				std::vector<int> bin;
				for ( unsigned int i = 0; i < selection.size(); i++ ) {
					if ( selection[i] % 3 == 0 ) {
						// + 1
						if ( not inselection(selection[i] + 1 ) ) {
							bin.push_back( selection[i] + 1 );
						}
						// + 2
						if ( not inselection(selection[i] + 2 ) ) {
							bin.push_back( selection[i] + 2 );
						}
					}
					else if ( selection[i] % 3 == 1 ) {
						// - 1
						if ( not inselection(selection[i] - 1 ) ) {
							bin.push_back( selection[i] - 1 );
						}
						// + 1
						if ( not inselection(selection[i] + 1 ) ) {
							bin.push_back( selection[i] + 1 );
						}
					}
					else if ( selection[i] % 3 == 2 ) {
						// - 1
						if ( not inselection(selection[i] - 1 ) ) {
							bin.push_back( selection[i] - 1 );
						}
						// - 2
						if ( not inselection(selection[i] - 2 ) ) {
							bin.push_back( selection[i] - 2 );
						}
					}
				}
				// find vertices that share coordinates
				unsigned int binsize = bin.size();
				for ( unsigned int i = 0; i < binsize; i++ ) {
					for ( unsigned int j = 0; j < mesh->vertices.size(); j++ )
					{
						if ( bin[i] == (int)j ) continue;
						if ( not checklayer( mesh->vertices[j].z ) ) continue;
						if ( mesh->vertices[j].x == mesh->vertices[bin[i]].x and mesh->vertices[j].y == mesh->vertices[bin[i]].y) {
							bin.push_back(j);
							continue;
						}
					}
				}
				// add to selection
				for ( unsigned int i = 0; i < bin.size(); i++ ) {
					if ( inselection(bin[i]) ) continue;
					selection.push_back(bin[i]);
				}
				// safety stop
				if ( iter == 10 ) {
					done = true;
				}
				iter++;
			}
			if ( LSHIFT )
			{
			}
			break;
		}

		// insert triangle
		case SDLK_f:
		{
			// don't allow layer zero
			if ( layer == 0 ) break;

			Vertex v;
			auto uniques = getuniqueselection();

			selection.clear();

			if ( uniques.size() == 0)
			{
				mesh->colors.push_back(palette_index);
				mesh->colors.push_back(palette_index);
				mesh->colors.push_back(palette_index);
				v.x = fmx - 10.0;
				v.y = fmy - 10.0;
				v.z = layer;
				mesh->vertices.push_back(v);
				selection.push_back(mesh->vertices.size()-1);
				v.x = fmx;
				v.y = fmy + 10.0;
				v.z = layer;
				mesh->vertices.push_back(v);
				selection.push_back(mesh->vertices.size()-1);
				v.x = fmx + 10.0;
				v.y = fmy - 10.0;
				v.z = layer;
				mesh->vertices.push_back(v);
				selection.push_back(mesh->vertices.size()-1);
				datachanged();
			}

			if ( uniques.size() == 2)
			{
				for ( unsigned int i = 0; i < uniques.size(); i++ ) {
					mesh->vertices.push_back(uniques[i]);
					mesh->colors.push_back(palette_index);
				}
				v.x = fmx;
				v.y = fmy;
				v.z = layer;
				mesh->vertices.push_back(v);
				mesh->colors.push_back(palette_index);
				selection.push_back(mesh->vertices.size() - 1);
				ifmx = fmx;
				ifmy = fmy;
				editmode = EditMode::GRAB;
				datachanged();
			}

			if ( uniques.size() == 3)
			{
				for ( unsigned int i = 0; i < uniques.size(); i++ ) {
					mesh->vertices.push_back(uniques[i]);
					mesh->colors.push_back(palette_index);
				}
				datachanged();
			}

			break;
		}

		// duplicate
		case SDLK_d:
			if ( LSHIFT ) {
				if ( selection.size() % 3 == 0 ) {
					color_buffer.clear();
					vertex_buffer.clear();
					for ( unsigned int i = 0; i < mesh->vertices.size(); i++ ) {
						if ( inselection(i) ) {
							color_buffer.push_back(mesh->colors[i]);
							vertex_buffer.push_back(mesh->vertices[i]);
						}
					}
					selection.clear();
					if ( vertex_buffer.size() > 0 ) {
						for ( unsigned int i = 0; i < vertex_buffer.size(); i++ ) {
							mesh->colors.push_back(color_buffer[i]);
							mesh->vertices.push_back(vertex_buffer[i]);
							// don't allow layer zero
							if ( layer != 0 )
								mesh->vertices.back().z = layer;
						}
					}
					for ( unsigned int i = mesh->vertices.size() - vertex_buffer.size(); i < mesh->vertices.size(); i++ ) {
						selection.push_back(i);
					}
					// NOTE: datachanged is triggered by GRAB
					editmode = EditMode::GRAB;
					ifmx = fmx;
					ifmy = fmy;
				}
			}
			break;

		// copy
		case SDLK_c:
			if ( selection.size() % 3 == 0 ) {
				color_buffer.clear();
				vertex_buffer.clear();
				for ( unsigned int i = 0; i < mesh->vertices.size(); i++ ) {
					if ( inselection(i) ) {
						color_buffer.push_back(mesh->colors[i]);
						vertex_buffer.push_back(mesh->vertices[i]);
					}
				}
			}
			break;

		// paste
		case SDLK_v:
			selection.clear();
			if ( vertex_buffer.size() > 0 ) {
				for ( unsigned int i = 0; i < vertex_buffer.size(); i++ ) {
					mesh->colors.push_back(color_buffer[i]);
					mesh->vertices.push_back(vertex_buffer[i]);
					// don't allow layer zero
					if ( layer != 0 )
						mesh->vertices.back().z = layer;
				}
			}
			if ( ! LSHIFT ) {
				for ( unsigned int i = mesh->vertices.size() - vertex_buffer.size(); i < mesh->vertices.size(); i++ ) {
					selection.push_back(i);
				}
				// center on cursor
				float sc1, sc2;
				setselectioncenter(sc1, sc2);
				for ( unsigned int i = 0; i < selection.size(); i++ ) {
					auto &v = mesh->vertices[selection[i]];
					v.x = v.x + (fmx - sc1);
					v.y = v.y + (fmy - sc2);
				}
				// NOTE: datachanged is triggered by GRAB
				editmode = EditMode::GRAB;
				ifmx = fmx;
				ifmy = fmy;
			}
			break;

		// y-axis constraint
		case SDLK_y:
		{
			if ( editmode == EditMode::GRAB or editmode == EditMode::SCALE ) {
				if ( YCONSTRAINT ) {
					YCONSTRAINT = false;
				} else {
					XCONSTRAINT = false;
					YCONSTRAINT = true;
				}
			}
			break;
		}

		// delete and x-axis constraint
		case SDLK_x:
		{
			// constraints
			if ( editmode == EditMode::GRAB or editmode == EditMode::SCALE ) {
				if ( XCONSTRAINT ) {
					XCONSTRAINT = false;
				} else {
					XCONSTRAINT = true;
					YCONSTRAINT = false;
				}
				break;
			}
			std::vector<unsigned int> color_bin;
			std::vector<Vertex> vertex_bin;
			if ( selection.size() % 3 == 0 ) {
				// copy
				color_buffer.clear();
				vertex_buffer.clear();
				for ( unsigned int i = 0; i < mesh->vertices.size(); i++ ) {
					if ( inselection(i) ) {
						color_buffer.push_back(mesh->colors[i]);
						vertex_buffer.push_back(mesh->vertices[i]);
					}
				}
				// cut
				for ( unsigned int i = 0; i < mesh->vertices.size(); i++ ) {
					if ( not inselection(i) ) {
						color_bin.push_back(mesh->colors[i]);
						vertex_bin.push_back(mesh->vertices[i]);
					}
				}
				mesh->colors.swap(color_bin);
				mesh->vertices.swap(vertex_bin);
				// clear selection
				selection.clear();
				datachanged();
			}
			break;
		}

		// grab
		case SDLK_g:
			if ( selection.size() == 0 ) break;
			imx = mx;
			imy = my;
			ifmx = fmx;
			ifmy = fmy;
			editmode = EditMode::GRAB;
			break;

		// scale
		case SDLK_s:
			if ( selection.size() == 0 ) break;
			editmode = EditMode::SCALE;
			scale = 0;
			initialscale = 0;
			imx = mx;
			imy = my;
			setselectioncenter(sx, sy);
			break;

		// rotate
		case SDLK_r:
			if ( selection.size() == 0 ) break;
			editmode = EditMode::ROTATE;
			rotation = 0;
			setselectioncenter(sx, sy);
			initialrotation = getdirection( sx, sy, fmx, fmy );
			break;

		// pick color
		case SDLK_p:
			editmode = EditMode::COLORIZE;
			break;
	}
}

void handleKeyUp(SDL_Event e)
{
	if ( e.type != SDL_KEYUP )
		return;

	switch ( e.key.keysym.sym )
	{
		case SDLK_LSHIFT:
			LSHIFT = false;
			break;

		case SDLK_LCTRL:
			LCTRL = false;
			break;

		// command mode
		case SDLK_SEMICOLON:
		{
			MODELINE = true;
			modeline_text = ":";
			SDL_StartTextInput();
			break;
		}
	}
}

void handleKeyEvents(SDL_Event e)
{
	// modeline keyboard events
	if ( MODELINE )
	{
		handleModeLine(e);
		return;
	}
	handleKeyDownRepeat(e);
	handleKeyDown(e);
	handleKeyUp(e);
}

void close()
{
	SDL_DestroyWindow( window );
	window = NULL;

	// free memory

	SDL_Quit();
}

int main( int argc, char* args[] )
{
	(void) args;
	font = new Font();

	initSDL(argc==2);

	SDL_Event e;

	while ( ! quit )
	{
		while ( SDL_PollEvent( &e ) != 0 )
		{
			handleMouseEvents(e);
			handleKeyEvents(e);

			// sdl events
			if ( e.type == SDL_QUIT )
			{
				quit = true;
			}
		}

		update();
		render();

		SDL_GL_SwapWindow( window );
	}

	close();

	return 0;
}

