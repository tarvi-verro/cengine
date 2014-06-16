
#include "xf-escg.h"	/* lF_RED _lF */
#include "ce-aux.h"	/* __init lprintf */
#include "ce-mod.h"	/* ce_mod_add */
#include <unistd.h>	/* sleep */
#include <assert.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

void root_win_swapbuffers();

int scn_loop()
{
	lprintf(INF "Reached the loop woo!\n");

	glClearColor(1.f, 0.f, 1.0f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
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
