
#include "xf-escg.h"	/* lF_RED _lF */
#include "ce-aux.h"	/* __init lprintf */
#include "ce-mod.h"	/* ce_mod_add */
#include <unistd.h>	/* sleep */
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

void root_win_swapbuffers();

static GLuint prg;

static int scn_loop()
{
	lprintf(INF "Reached the loop woo!\n");

	glClearColor(1.f, 0.f, 1.0f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(prg);

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	// An array of 3 vectors which represents 3 vertices
	static const GLfloat g_vertex_buffer_data[] = {
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		0.0f,  1.0f, 0.0f,
	};

	// This will identify our vertex buffer
	GLuint vertexbuffer;

	// Generate 1 buffer, put the resulting identifier in vertexbuffer
	glGenBuffers(1, &vertexbuffer);

	// The following commands will talk about our 'vertexbuffer' buffer
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);

	// Give our vertices to OpenGL.
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data),
			g_vertex_buffer_data, GL_STATIC_DRAW);

	// 1rst attribute buffer : vertices
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glVertexAttribPointer(
			0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
			);

	// Draw the triangle !
	// Starting from vertex 0; 3 vertices total -> 1 triangle
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glDisableVertexAttribArray(0);
	root_win_swapbuffers();
	sleep(2);
	return 0;
}

extern int (*control)();

static int load()
{
	control = scn_loop;
	lprintf(INF ""lF_WHI lBLD_"scn~tri selected."_lBLD _lF"\n");

	/* Shader creation */
	const char *vertex_shader_src =
		"#version 330\n"
		"layout(location = 0) in vec4 position;\n"
		"void main()\n"
		"{\n"
		"	gl_Position = position;\n"
		"}\n";
	const char *fragment_shader_src =
		"#version 330\n"
		"out vec4 outputColor;\n"
		"void main()\n"
		"{\n"
		"	outputColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);\n"
		"}\n";
	GLint s, i;
	GLchar *a;

	GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vertex_shader_src, NULL);
	glCompileShader(vertex_shader);
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &s);
	if (s == false) {
		glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &i);
		a = malloc(sizeof(*a) * (i + 1));
		glGetShaderInfoLog(vertex_shader, i, NULL, a);
		lprintf(ERR "Failed compiling vertex shader: %s\n", a);
		free(a);
		return -1;
	}

	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fragment_shader_src, NULL);
	glCompileShader(fragment_shader);
	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &s);
	if (s == false) {
		glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &i);
		a = malloc(sizeof(*a) * (i + 1));
		glGetShaderInfoLog(fragment_shader, i, NULL, a);
		lprintf(ERR "Failed compiling fragment shader: %s\n", a);
		free(a);
		return -2;
	}

	/* Program creation */
	prg = glCreateProgram();
	glAttachShader(prg, vertex_shader);
	glAttachShader(prg, fragment_shader);
	glLinkProgram(prg);
	glGetProgramiv(prg, GL_LINK_STATUS, &s);
	if (s == false) {
		glGetProgramiv(prg, GL_INFO_LOG_LENGTH, &i);
		a = malloc(sizeof(*a) * (i + 1));
		glGetProgramInfoLog(prg, i, NULL, a);
		lprintf(ERR "Failed linking program.\n");
		free(a);
		return -3;
	}
	glDetachShader(prg, vertex_shader);
	glDetachShader(prg, fragment_shader);
	return 0;
}

static int unload()
{
	return 0;
}

static int tri_mod_id = -1;

static void __init code_load()
{
	struct ce_mod m = {
		.comment = "An example scene containing a triangle.",
		.def = "scn-tri | scn~tri; control=tri-loop",
		.use = "gl-context 3.3",
		.load = load,
		.unload = unload,
	};
	tri_mod_id = ce_mod_add(&m);
	assert(tri_mod_id >= 0);
}

static void __exit code_unload()
{
	ce_mod_rm(tri_mod_id);
}
