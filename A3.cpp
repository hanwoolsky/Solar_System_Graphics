#include "cgmath.h"		// slee's simple math library
#include "cgut.h"		// slee's OpenGL utility
#include "sphere.h"		// sphere class definition
#include "torus.h"
#include "trackball.h"	// virtual trackball

//*************************************
// global constants
static const char* window_name = "PA3 - Planet in Space";
static const char* vert_shader_path = "../bin/shaders/trackball.vert";
static const char* frag_shader_path = "../bin/shaders/trackball.frag";
uint				NUM_TESS = 36;


//*************************************
// common structures
struct camera
{
	vec3	eye = vec3(15, 0, 0);
	vec3	at = vec3(0, 0, 0);
	vec3	up = vec3(0, 0, 1);
	mat4	view_matrix = mat4::look_at(eye, at, up);
	
	float	fovy = PI / 4.0f; // must be in radian
	float	aspect;
	float	dnear = 1.0f;
	float	dfar = 1000.0f;
	mat4	projection_matrix;
};

//*************************************
// window objects
GLFWwindow* window = nullptr;
ivec2		window_size = cg_default_window_size(); //ivec2(1280, 720);

//*************************************
// OpenGL objects
GLuint	program = 0;		// ID holder for GPU program
GLuint	vertex_array = 0;	// ID holder for vertex array object
GLuint  torus_vertex_array = 0;

//*************************************
// global variables
int		frame = 0;
int 	color = 0;
int		mousebtn = -1;
uint	Vert = 0;
uint	Hori = 0;
uint	tor_Vert = 0;
uint	tor_Hori = 0;
float   tmp_time = 0.0f;
float	theta = 0.0f;
#ifndef GL_ES_VERSION_2_0
bool	b_wireframe = false;
bool	b_rotate = true;
bool	b_torus = false;
bool	b_solid_color = false;
bool	shift = false;
bool	ctrl = false;
#endif
auto	spheres = std::move(create_spheres());

//*************************************
// scene objects
camera		cam;
trackball	tb;

//*************************************
// holder of vertices and indices of a unit sphere
std::vector<vertex>	unit_sphere_vertices;	// host-side vertices
std::vector<vertex>	unit_torus_vertices;	// host-side vertices

//*************************************
void update()
{
	// update projection matrix
	cam.aspect = window_size.x / float(window_size.y);
	cam.projection_matrix = mat4::perspective(cam.fovy, cam.aspect, cam.dnear, cam.dfar);

	// build the model matrix for oscillating scale
	float t = float(glfwGetTime());

	// update uniform variables in vertex/fragment shaders
	GLint uloc;
	uloc = glGetUniformLocation(program, "b_solid_color");			if (uloc > -1) glUniform1i(uloc, b_solid_color);
	uloc = glGetUniformLocation(program, "view_matrix");			if (uloc > -1) glUniformMatrix4fv(uloc, 1, GL_TRUE, cam.view_matrix);
	uloc = glGetUniformLocation(program, "projection_matrix");		if (uloc > -1) glUniformMatrix4fv(uloc, 1, GL_TRUE, cam.projection_matrix);
}

void render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(program);
	glBindVertexArray(vertex_array);

	torus_t t;

	for (auto& p : spheres){
		float ro_time = float(glfwGetTime()) - tmp_time;
		tmp_time = float(glfwGetTime());
		theta += b_rotate ? ro_time : 0;
		p.update(theta, b_rotate);

		// update per-sphere uniforms
		GLint uloc;
		uloc = glGetUniformLocation(program, "model_matrix");		if (uloc > -1) glUniformMatrix4fv(uloc, 1, GL_TRUE, p.model_matrix);
		glDrawElements(GL_TRIANGLES, NUM_TESS * NUM_TESS * 12, GL_UNSIGNED_INT, nullptr);

		if (b_torus && p.ring) {
			glBindVertexArray(torus_vertex_array);
			t.update(theta);

			glDrawElements(GL_TRIANGLES, NUM_TESS * NUM_TESS * 12, GL_UNSIGNED_INT, nullptr);
		}
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
#ifndef GL_ES_VERSION_2_0
	printf("- press 'w' to toggle wireframe\n");
	printf("- press 'd' to toggle(tc.xy, 0) > space\n");
	printf("- press 't' to make torus\n");
	printf("- press 'r' to stop rotate\n");
	printf("- rignt click or shift + left click to zooming\n");
	printf("- middle click or ctrl + left click to panning\n");
#endif
	printf("\n");
}

std::vector<vertex> create_sphere_vertices(uint H, uint V)
{
	sphere_t s;
	std::vector<vertex> v;
	for (uint i = 0; i <= H; i++) {
		for (uint j = 0; j <= V; j++) {
			float theta = PI * 2.0f * i / float(H), c_theta = cos(theta), s_theta = sin(theta);
			float phi = PI * j / float(V), c_phi = cos(phi), s_phi = sin(phi);
			v.push_back({ vec3(s.rotat_radius * s_phi * c_theta, s.rotat_radius * s_phi * s_theta, s.rotat_radius * c_phi), vec3(s_phi * c_theta, s_phi * s_theta, c_phi), vec2(theta / (2 * PI), 1 - phi / PI) });
		}
	}
	return v;
}

std::vector<vertex> create_torus_vertices(uint H, uint V)
{
	torus_t t;
	std::vector<vertex> u;
	for (uint i = 0; i <= H; i++) {
		for (uint j = 0; j <= V; j++) {
			float theta = PI * 2.0f * i / float(H), c_theta = cos(theta), s_theta = sin(theta);
			float phi = PI * 2.0f * j / float(V), c_phi = cos(phi), s_phi = sin(phi);
			u.push_back({ vec3((t.Radius + t.radius * s_phi) * c_theta, (t.Radius + t.radius * s_phi) * s_theta, t.height * t.radius * c_phi), vec3(s_phi * c_theta, s_phi * s_theta, c_phi), vec2(theta / (2 * PI), 1 - phi / PI) });
		}
	}
	return u;
}

void update_vertex_buffer(const std::vector<vertex>& vertices, uint H, uint V)
{
	static GLuint vertex_buffer = 0;	// ID holder for vertex buffer
	static GLuint index_buffer = 0;		// ID holder for index buffer

	// clear and create new buffers
	if (vertex_buffer)	glDeleteBuffers(1, &vertex_buffer);	vertex_buffer = 0;
	if (index_buffer)	glDeleteBuffers(1, &index_buffer);	index_buffer = 0;

	// check exceptions
	if (vertices.empty()) { printf("[error] vertices is empty.\n"); return; }

	std::vector<uint> indices;
	for (uint i = 0; i < H; i++) {
		for (uint j = 0; j < V; j++) {
			Vert = i * (V + 1) + j;
			Hori = (i + 1) * (V + 1) + j - 1;
			indices.push_back(Hori);
			indices.push_back(Vert);
			indices.push_back(Hori + 1);
			indices.push_back(Hori + 1);
			indices.push_back(Vert);
			indices.push_back(Vert + 1);
		}
	}

	// generation of vertex buffer: use vertices as it is
	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * vertices.size(), &vertices[0], GL_STATIC_DRAW);

	// geneation of index buffer
	glGenBuffers(1, &index_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint) * indices.size(), &indices[0], GL_STATIC_DRAW);

	// generate vertex array object, which is mandatory for OpenGL 3.3 and higher
	if (vertex_array) glDeleteVertexArrays(1, &vertex_array);
	vertex_array = cg_create_vertex_array(vertex_buffer, index_buffer);
	if (!vertex_array) { printf("%s(): failed to create vertex aray\n", __func__); return; }
}

void update_torus_vertex_buffer(const std::vector<vertex>& tor_vertices, uint H, uint V)
{
	static GLuint torus_vertex_buffer = 0;	// ID holder for vertex buffer
	static GLuint torus_index_buffer = 0;		// ID holder for index buffer

	// clear and create new buffers
	if (torus_vertex_buffer)	glDeleteBuffers(1, &torus_vertex_buffer);	torus_vertex_buffer = 0;
	if (torus_index_buffer)	glDeleteBuffers(1, &torus_index_buffer);	torus_index_buffer = 0;

	// check exceptions
	if (tor_vertices.empty()) { printf("[error] tor_vertices is empty.\n"); return; }

	std::vector<uint> tor_indices;
	for (uint i = 0; i < (H+1); i++) {
		for (uint j = 0; j < (V+1); j++) {
			tor_Vert = i * (V + 1) + j;
			tor_Hori = (i + 1) * (V + 1) + j - 1;
			tor_indices.push_back(tor_Hori);
			tor_indices.push_back(tor_Vert);
			tor_indices.push_back(tor_Hori + 1);
			tor_indices.push_back(tor_Hori + 1);
			tor_indices.push_back(tor_Vert);
			tor_indices.push_back(tor_Vert + 1);
		}
	}

	// generation of vertex buffer: use tor_vertices as it is
	glGenBuffers(1, &torus_vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, torus_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * tor_vertices.size(), &tor_vertices[0], GL_STATIC_DRAW);

	// geneation of index buffer
	glGenBuffers(1, &torus_index_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, torus_index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint) * tor_indices.size(), &tor_indices[0], GL_STATIC_DRAW);

	// generate vertex array object, which is mandatory for OpenGL 3.3 and higher
	if (torus_vertex_array) glDeleteVertexArrays(1, &torus_vertex_array);
	torus_vertex_array = cg_create_vertex_array(torus_vertex_buffer, torus_index_buffer);
	if (!torus_vertex_array) { printf("%s(): failed to create vertex aray\n", __func__); return; }
}

void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q)	glfwSetWindowShouldClose(window, GL_TRUE);
		else if (key == GLFW_KEY_H || key == GLFW_KEY_F1)	print_help();
		else if (key == GLFW_KEY_HOME)					cam = camera();
		else if (key == GLFW_KEY_D)
		{
			b_solid_color = !b_solid_color;
			printf("> %s\n", b_solid_color ? "tex" : "space");
		}
		else if (key == GLFW_KEY_R)
		{
			b_rotate = !b_rotate;
			printf("> %s\n", b_rotate ? "rotate" : "stop");
		}
#ifndef GL_ES_VERSION_2_0
		else if (key == GLFW_KEY_W)
		{
			b_wireframe = !b_wireframe;
			glPolygonMode(GL_FRONT_AND_BACK, b_wireframe ? GL_LINE : GL_FILL);
			printf("> using %s mode\n", b_wireframe ? "wireframe" : "solid");
		}
		else if (key == GLFW_KEY_T)
		{
			b_torus = !b_torus;
			printf("> %s\n", b_torus ? "made torus" : "only sphere");
		}
		else if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT) shift = true;
		else if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL) ctrl = true;
#endif
	}
	else if (action == GLFW_RELEASE) {
		shift = false;
		ctrl = false;
	}
}

void mouse(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT || GLFW_MOUSE_BUTTON_RIGHT || GLFW_MOUSE_BUTTON_MIDDLE) {
		dvec2 pos; glfwGetCursorPos(window, &pos.x, &pos.y);
		vec2 npos = cursor_to_ndc(pos, window_size);
		if (action == GLFW_PRESS)			tb.begin(cam.view_matrix, npos);
		else if (action == GLFW_RELEASE)	tb.end(cam.eye, cam.at);
		mousebtn = button;
	}
}

void motion(GLFWwindow* window, double x, double y)
{
	if (!tb.is_tracking()) return;
	vec2 npos = cursor_to_ndc(dvec2(x, y), window_size);

	if (mousebtn == GLFW_MOUSE_BUTTON_LEFT && !shift && !ctrl) cam.view_matrix = tb.update(npos);
	else if (mousebtn == GLFW_MOUSE_BUTTON_RIGHT || (mousebtn == GLFW_MOUSE_BUTTON_LEFT && shift))
		cam.view_matrix = tb.zooming(npos, cam.eye, cam.at, cam.up);
	else if (mousebtn == GLFW_MOUSE_BUTTON_MIDDLE || (mousebtn == GLFW_MOUSE_BUTTON_LEFT && ctrl))
		cam.view_matrix = tb.panning(npos, cam.eye, cam.at, cam.up);
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
	unit_sphere_vertices = std::move(create_sphere_vertices(2*NUM_TESS, NUM_TESS));
	unit_torus_vertices = std::move(create_torus_vertices(2 * NUM_TESS, NUM_TESS));

	// create vertex buffer; called again when index buffering mode is toggled
	update_vertex_buffer(unit_sphere_vertices, 2 * NUM_TESS, NUM_TESS);
	update_torus_vertex_buffer(unit_torus_vertices, 2 * NUM_TESS, NUM_TESS);
	
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