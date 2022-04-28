#include "cgmath.h"		// slee's simple math library
#include "cgut.h"		// slee's OpenGL utility
#include "circle.h"		// circle class definition

//*************************************
// global constants
static const char* window_name = "cgbase - moving circle";
static const char* vert_shader_path = "../bin/shaders/circ.vert";
static const char* frag_shader_path = "../bin/shaders/circ.frag";
uint				NUM_TESS = 100;		// initial tessellation factor of the circle as a polygon
uint				NUM = 50;		// initial number of circle

//*************************************
// window objects
GLFWwindow* window = nullptr;
ivec2		window_size = cg_default_window_size(); //ivec2(1280, 720);

//*************************************
// OpenGL objects
GLuint	program = 0;		// ID holder for GPU program
GLuint	vertex_array = 0;	// ID holder for vertex array object

//*************************************
// global variables
int		frame = 0;						// index of rendering frames
float	t = 0.0f;						// current simulation parameter
float	t0 = 0.0f;						// prev
bool	b_solid_color = true;			// use circle's color?
bool	b_index_buffer = true;			// use index buffering?
bool	damping = false;
float	u_time = 0.0f;
float windrate = window_size.x / float(window_size.y);
#ifndef GL_ES_VERSION_2_0
bool	b_wireframe = false;
#endif
auto	circles = std::move(create_circles(windrate, NUM));
struct {
	bool add = false, sub = false;
	operator bool() const {
		return add || sub;
	}
} b; // flags of keys for smooth changes

//*************************************
// holder of vertices and indices of a unit circle
std::vector<vertex>	unit_circle_vertices;	// host-side vertices

//*************************************
void update()
{
	t0 = t;
	t = float(glfwGetTime());
	u_time = t * 0.3f;

	float aspect = window_size.x / float(window_size.y);
	mat4 aspect_matrix =
	{
		std::min(1 / aspect,1.0f), 0, 0, 0,
		0, std::min(aspect,1.0f), 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};

	// update common uniform variables in vertex/fragment shaders
	GLint uloc;
	uloc = glGetUniformLocation(program, "b_solid_color");	if (uloc > -1) glUniform1i(uloc, b_solid_color);
	uloc = glGetUniformLocation(program, "u_time");				if (uloc > -1) glUniform1f(uloc, u_time);
	uloc = glGetUniformLocation(program, "aspect_matrix");	if (uloc > -1) glUniformMatrix4fv(uloc, 1, GL_TRUE, aspect_matrix);

	// update vertex buffer by the pressed keys
	void update_num(); // forward declaration
	if (b) update_num();
}

void render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(program);
	glBindVertexArray(vertex_array);

	// render two circles: trigger shader program to process vertex data
	for (auto& c : circles)
	{
		c.overlap(circles, damping);
		c.update(t, windrate, damping, t - t0);

		// update per-circle uniforms
		GLint uloc;
		uloc = glGetUniformLocation(program, "solid_color");		if (uloc > -1) glUniform4fv(uloc, 1, c.color);	// pointer version
		uloc = glGetUniformLocation(program, "model_matrix");		if (uloc > -1) glUniformMatrix4fv(uloc, 1, GL_TRUE, c.model_matrix);

		// per-circle draw calls
		if (b_index_buffer)	glDrawElements(GL_TRIANGLES, NUM_TESS * 3, GL_UNSIGNED_INT, nullptr);
		else				glDrawArrays(GL_TRIANGLES, 0, NUM_TESS * 3); // NUM_TESS = N
	}

	// swap front and back buffers, and display to screen
	glfwSwapBuffers(window);
}

void reshape(GLFWwindow* window, int width, int height)
{
	// set current viewport in pixels (win_x, win_y, win_width, win_height)
	// viewport: the window area that are affected by rendering 
	window_size = ivec2(width, height);
	glViewport(0, 0, width, height);
}

void print_help()
{
	printf("[help]\n");
	printf("- press ESC or 'q' to terminate the program\n");
	printf("- press F1 or 'h' to see help\n");
	printf("- press 'd' to toggle between solid color and texture coordinates\n");
	printf("- press number(3, 4, 5) to change angle\n");
	printf("- press 'g' to include gravity\n");
	printf("- press 'i' to toggle between index buffering and simple vertex buffering\n");
#ifndef GL_ES_VERSION_2_0
	printf("- press 'w' to toggle wireframe\n");
#endif
	printf("\n");
}

std::vector<vertex> create_circle_vertices(uint N)
{
	std::vector<vertex> v = { { vec3(0), vec3(0,0,-1.0f), vec2(0.5f) } }; // origin
	for (uint k = 0; k <= N; k++)
	{
		float t = PI * 2.0f * k / float(N), c = cos(t), s = sin(t);
		v.push_back({ vec3(c,s,0), vec3(0,0,-1.0f), vec2(c,s) * 0.5f + 0.5f });
	}
	return v;
}

void update_vertex_buffer(const std::vector<vertex>& vertices, uint N)
{
	static GLuint vertex_buffer = 0;	// ID holder for vertex buffer
	static GLuint index_buffer = 0;		// ID holder for index buffer

	// clear and create new buffers
	if (vertex_buffer)	glDeleteBuffers(1, &vertex_buffer);	vertex_buffer = 0;
	if (index_buffer)	glDeleteBuffers(1, &index_buffer);	index_buffer = 0;

	// check exceptions
	if (vertices.empty()) { printf("[error] vertices is empty.\n"); return; }

	// create buffers
	if (b_index_buffer)
	{
		std::vector<uint> indices;
		for (uint k = 0; k < N; k++)
		{
			indices.push_back(0);	// the origin
			indices.push_back(k + 1);
			indices.push_back(k + 2);
		}

		// generation of vertex buffer: use vertices as it is
		glGenBuffers(1, &vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * vertices.size(), &vertices[0], GL_STATIC_DRAW);

		// geneation of index buffer
		glGenBuffers(1, &index_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint) * indices.size(), &indices[0], GL_STATIC_DRAW);
	}
	else
	{
		std::vector<vertex> v; // triangle vertices
		for (uint k = 0; k < N; k++)
		{
			v.push_back(vertices.front());	// the origin
			v.push_back(vertices[k + 1]);
			v.push_back(vertices[k + 2]);
		}

		// generation of vertex buffer: use triangle_vertices instead of vertices
		glGenBuffers(1, &vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * v.size(), &v[0], GL_STATIC_DRAW);
	}

	// generate vertex array object, which is mandatory for OpenGL 3.3 and higher
	if (vertex_array) glDeleteVertexArrays(1, &vertex_array);
	vertex_array = cg_create_vertex_array(vertex_buffer, index_buffer);
	if (!vertex_array) { printf("%s(): failed to create vertex aray\n", __func__); return; }
}

void update_num()
{
	circle_t c;
	if (b.add) NUM++; if (b.sub) NUM--;

	do {
		c.radius = randf(0.2f / float(sqrt(NUM)), 0.7f / float(sqrt(NUM)));
		c.center.x = randf(-windrate + c.radius, windrate - c.radius);
		c.center.y = randf(-1.0f + c.radius, 1.0f - c.radius);
	} while (c.collision(circles, windrate));
	c.velocity.x = randf(-0.01f, 0.01f);
	c.velocity.y = randf(-0.01f, 0.01f);
	c.mass = length(c.velocity) * c.radius;
	c.color.r = randf();
	c.color.g = randf();
	c.color.b = randf();
	if (b.add) {
		circles.emplace_back(c);
		b.add = false;
	}
	else if (b.sub && !circles.empty()) {
		circles.pop_back();
		b.sub = false;
	}
	printf("> Number of circles : %d\n", NUM);
}

void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q)	glfwSetWindowShouldClose(window, GL_TRUE);
		else if (key == GLFW_KEY_H || key == GLFW_KEY_F1)	print_help();
		else if (key == GLFW_KEY_KP_ADD || (key == GLFW_KEY_EQUAL && (mods & GLFW_MOD_SHIFT))) b.add = true;
		else if (key == GLFW_KEY_KP_SUBTRACT || key == GLFW_KEY_MINUS) b.sub = true;
		else if (key == GLFW_KEY_I)
		{
			b_index_buffer = !b_index_buffer;
			update_vertex_buffer(unit_circle_vertices, NUM_TESS);
			printf("> using %s buffering\n", b_index_buffer ? "index" : "vertex");
		}
		else if (key == GLFW_KEY_D)
		{
			b_solid_color = !b_solid_color;
			printf("> using %s\n", b_solid_color ? "solid color" : "texture coordinates as color");
		}
		else if (key == GLFW_KEY_G)
		{
			damping = true;
		}
		else if (key == GLFW_KEY_0)
		{
			unit_circle_vertices = create_circle_vertices(NUM_TESS = 256);
			update_vertex_buffer(unit_circle_vertices, NUM_TESS);
		}
		else if (key == GLFW_KEY_3)
		{
			unit_circle_vertices = create_circle_vertices(NUM_TESS = 3);
			update_vertex_buffer(unit_circle_vertices, NUM_TESS);
		}
		else if (key == GLFW_KEY_4)
		{
			unit_circle_vertices = create_circle_vertices(NUM_TESS = 4);
			update_vertex_buffer(unit_circle_vertices, NUM_TESS);
		}
		else if (key == GLFW_KEY_5)
		{
			unit_circle_vertices = create_circle_vertices(NUM_TESS = 5);
			update_vertex_buffer(unit_circle_vertices, NUM_TESS);
		}
#ifndef GL_ES_VERSION_2_0
		else if (key == GLFW_KEY_W)
		{
			b_wireframe = !b_wireframe;
			glPolygonMode(GL_FRONT_AND_BACK, b_wireframe ? GL_LINE : GL_FILL);
			printf("> using %s mode\n", b_wireframe ? "wireframe" : "solid");
		}
#endif
	}
	else if (action == GLFW_RELEASE)
	{
		if (key == GLFW_KEY_KP_ADD || (key == GLFW_KEY_EQUAL && (mods & GLFW_MOD_SHIFT)))	b.add = false;
		else if (key == GLFW_KEY_KP_SUBTRACT || key == GLFW_KEY_MINUS) b.sub = false;
	}
}

void mouse(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		dvec2 pos;
		glfwGetCursorPos(window, &pos.x, &pos.y);
		printf("> Left mouse button pressed at (%d, %d)\n", int(pos.x), int(pos.y));
	}
}

void motion(GLFWwindow* window, double x, double y)
{
}

bool user_init()
{
	// log hotkeys
	print_help();

	// init GL states
	glLineWidth(1.0f);
	glClearColor(39 / 255.0f, 40 / 255.0f, 34 / 255.0f, 1.0f);	// set clear color
	glEnable(GL_CULL_FACE);								// turn on backface culling
	glEnable(GL_DEPTH_TEST);								// turn on depth tests

	// define the position of four corner vertices
	unit_circle_vertices = std::move(create_circle_vertices(NUM_TESS));

	// create vertex buffer; called again when index buffering mode is toggled
	update_vertex_buffer(unit_circle_vertices, NUM_TESS);

	return true;
}

void user_finalize()
{
}

int main(int argc, char* argv[])
{
	// create window and initialize OpenGL extensions
	if (!(window = cg_create_window(window_name, window_size.x, window_size.y))) { glfwTerminate(); return 1; }
	if (!cg_init_extensions(window)) { glfwTerminate(); return 1; }	// init OpenGL extensions

	// initializations and validations of GLSL program
	if (!(program = cg_create_program(vert_shader_path, frag_shader_path))) { glfwTerminate(); return 1; }	// create and compile shaders/program
	if (!user_init()) { printf("Failed to user_init()\n"); glfwTerminate(); return 1; }					// user initialization

	// register event callbacks
	glfwSetWindowSizeCallback(window, reshape);	// callback for window resizing events
	glfwSetKeyCallback(window, keyboard);			// callback for keyboard events
	glfwSetMouseButtonCallback(window, mouse);	// callback for mouse click inputs
	glfwSetCursorPosCallback(window, motion);		// callback for mouse movements

	// enters rendering/event loop
	for (frame = 0; !glfwWindowShouldClose(window); frame++)
	{
		glfwPollEvents();	// polling and processing of events
		update();			// per-frame update
		render();			// per-frame render
	}

	// normal termination
	user_finalize();
	cg_destroy_window(window);

	return 0;
}
