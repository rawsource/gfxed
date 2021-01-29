#ifndef GFXED_MESH_H
#define GFXED_MESH_H

#include <vector>

struct Vertex
{
	float x;
	float y;
	float z;
};

struct Color
{
	float r;
	float g;
	float b;
	float a;
};

struct Mesh
{
	std::string name;
	std::vector<Vertex> vertices;
	std::vector<unsigned int> colors;
	Mesh() : vertices(0), colors(0) {};
};

struct View
{
	bool VERTEX = false;
	bool TRIANGLE = false;
	bool FILL = false;
	bool HIDDEN = false;
};

#endif

