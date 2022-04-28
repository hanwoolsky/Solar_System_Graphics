#include "cgmath.h"		// slee's simple math library
#define STB_IMAGE_IMPLEMENTATION
#include "cgut.h"		// slee's OpenGL utility
#include "sphere.h"
#include "torus.h"
#include "trackball.h"
#include "satellite.h"

//*************************************
// global constants
static const char* window_name = "PA4 - Full Solar System";
static const char* vert_shader_path = "../bin/shaders/texphong.vert";
static const char* frag_shader_path = "../bin/shaders/texphong.frag";
static const char* mesh_texture_path[9] = {"../bin/textures/sun.jpg", "../bin/textures/mercury.jpg","../bin/textures/venus.jpg", "../bin/textures/earth.jpg", "../bin/textures/mars.jpg",
											"../bin/textures/jupiter.jpg", "../bin/textures/saturn.jpg", "../bin/textures/uranus.jpg", "../bin/textures/neptune.jpg"};
static const char* mesh_normal_texture_path[9] = {"../bin/textures/sun.jpg", "../bin/textures/mercury-normal.jpg","../bin/textures/venus-normal.jpg", "../bin/textures/earth-normal.jpg", "../bin/textures/mars-normal.jpg",
											"../bin/textures/jupiter.jpg", "../bin/textures/saturn.jpg", "../bin/textures/uranus.jpg", "../bin/textures/neptune.jpg"};
static const char* mesh_ring_texture_path[2] = { "../bin/textures/saturn-ring.jpg", "../bin/textures/uranus-ring.jpg" };
static const char* mesh_ring_alpha_texture_path[2] = { "../bin/textures/saturn-ring-alpha.jpg", "../bin/textures/uranus-ring-alpha.jpg" };
static const char* mesh_satellite_texture_path[4] = { "../bin/textures/moon.jpg", "../bin/textures/mercury.jpg",  "../bin/textures/mercury.jpg",  "../bin/textures/mercury.jpg" }; // instead of jupiter's satellite
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

struct light_t
{
	vec4	position = vec4(0.0f, 0.0f, 0.0f, 1.0f);   // directional light
	vec4	ambient = vec4(0.2f, 0.2f, 0.2f, 1.0f);
	vec4	diffuse = vec4(0.8f, 0.8f, 0.8f, 1.0f);
	vec4	specular = vec4(1.0f, 1.0f, 1.0f, 1.0f);
};

struct material_t
{
	vec4	ambient = vec4(0.2f, 0.2f, 0.2f, 1.0f);
	vec4	diffuse = vec4(0.8f, 0.8f, 0.8f, 1.0f);
	vec4	specular = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	float	shininess = 1000.0f;
};

//*************************************
// window objects
GLFWwindow* window = nullptr;
ivec2		window_size = cg_default_window_size(); //ivec2(1280, 720);

//*************************************
// OpenGL objects
GLuint program = 0;
GLuint vertex_array = 0;
GLuint torus_vertex_array = 0;
GLuint sate_vertex_array = 0;
GLuint PLANETTEX[9] = { 0 };
GLuint PLANETNORMTEX[9] = { 0 };
GLuint RINGTEX[2] = { 0 };
GLuint RINGALPHATEX[2] = { 0 };
GLuint SATELLITE[4] = { 0 };

//*************************************
// global variables
int		frame = 0;
int 	color = 0;
int		k = 0;
int		r = 0;
int		s = 0;
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
bool	shift = false;
bool	ctrl = false;
bool	b_normal = false;
#endif
auto	spheres = std::move(create_spheres());
auto	satellites = std::move(create_satellite());

//*************************************
// scene objects
camera cam;
trackball tb;
light_t		light;
material_t	material;

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
	uloc = glGetUniformLocation(program, "view_matrix");			if (uloc > -1) glUniformMatrix4fv(uloc, 1, GL_TRUE, cam.view_matrix);
	uloc = glGetUniformLocation(program, "projection_matrix");		if (uloc > -1) glUniformMatrix4fv(uloc, 1, GL_TRUE, cam.projection_matrix);

	// setup light properties
	glUniform4fv(glGetUniformLocation(program, "light_position"), 1, light.position);
	glUniform4fv(glGetUniformLocation(program, "Ia"), 1, light.ambient);
	glUniform4fv(glGetUniformLocation(program, "Id"), 1, light.diffuse);
	glUniform4fv(glGetUniformLocation(program, "Is"), 1, light.specular);

	// setup material properties
	glUniform4fv(glGetUniformLocation(program, "Ka"), 1, material.ambient);
	glUniform4fv(glGetUniformLocation(program, "Kd"), 1, material.diffuse);
	glUniform4fv(glGetUniformLocation(program, "Ks"), 1, material.specular);
	glUniform1f(glGetUniformLocation(program, "shininess"), material.shininess);
}

void render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(program);
	glBindVertexArray(vertex_array);
	
	bool sun = true;
	bool earth = true;
	bool ring = false;
	for (auto& p : spheres){
		float ro_time = float(glfwGetTime()) - tmp_time;
		tmp_time = float(glfwGetTime());
		theta += b_rotate ? ro_time : 0;
		p.update(theta, b_rotate);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, PLANETTEX[k % 9]);
		glUniform1i(glGetUniformLocation(program, "TEX"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, PLANETNORMTEX[k % 9]);
		glUniform1i(glGetUniformLocation(program, "NORM"), 1);

		if (k % 9 == 0) sun = true;
		else sun = false;
		if (b_normal && (k % 9 == 1 || k % 9 == 2 || k % 9 == 3 || k % 9 == 4)) earth = true;
		else earth = false;
		glUniform1i(glGetUniformLocation(program, "SUN"), sun);
		glUniform1i(glGetUniformLocation(program, "EARTH"), earth);
		glUniform1f(glGetUniformLocation(program, "alpha"), 1.0f);
		// update per-sphere uniform
		GLint uloc;
		uloc = glGetUniformLocation(program, "model_matrix");		if (uloc > -1) glUniformMatrix4fv(uloc, 1, GL_TRUE, p.model_matrix);
		glDrawArrays(GL_TRIANGLES, 0, 15552);
		
		glEnable(GL_BLEND);
		if (p.ring) {
			glUniform1f(glGetUniformLocation(program, "alpha"), 0.5f);
			glBindVertexArray(torus_vertex_array);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, RINGTEX[r++ % 2]);
			glDrawArrays(GL_TRIANGLES, 0, 15984);

			glBindVertexArray(vertex_array);
			glBindTexture(GL_TEXTURE_2D, PLANETTEX[k % 9]);
			glDrawArrays(GL_TRIANGLES, 0, 15552);
		}
		glDisable(GL_BLEND);

		if (p.satellite.size()) {
			for (auto& sate : p.satellite) {
				sate.model_matrix = p.model_matrix * mat4::translate(sate.rotat_radius * cos(theta + sate.phi), sate.rotat_radius * sin(theta + sate.phi), 0) * mat4::rotate( vec3(0.0f, 0.0f, 1.0f), sate.revol_velocity) * mat4::scale(sate.revol_radius);
				uloc = glGetUniformLocation(program, "model_matrix");		if (uloc > -1) glUniformMatrix4fv(uloc, 1, GL_TRUE, sate.model_matrix);
				glBindVertexArray(vertex_array);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, SATELLITE[s++ % 4]);
				glDrawArrays(GL_TRIANGLES, 0, 15552);

				glBindVertexArray(vertex_array);
				glBindTexture(GL_TEXTURE_2D, PLANETTEX[k % 9]);
				glDrawArrays(GL_TRIANGLES, 0, 15552);
			}
		}
		k++;
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
	printf("- press 'r' to stop rotate\n");
	printf("- press 'n' to see normal mapping\n");
#ifndef GL_ES_VERSION_2_0
	printf("- press 'w' to toggle wireframe\n");
#endif
	printf("\n");
}

void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q)	glfwSetWindowShouldClose(window, GL_TRUE);
		else if (key == GLFW_KEY_H || key == GLFW_KEY_F1)	print_help();
		else if (key == GLFW_KEY_HOME)					cam = camera();
		else if (key == GLFW_KEY_R)
		{
			b_rotate = !b_rotate;
			printf("> %s\n", b_rotate ? "rotate" : "stop");
		}
		else if (key == GLFW_KEY_N)
		{
			b_normal = !b_normal;
			printf("> %s\n", b_normal ? "normal mapping" : "texture");
		}
#ifndef GL_ES_VERSION_2_0
		else if (key == GLFW_KEY_W)
		{
			b_wireframe = !b_wireframe;
			glPolygonMode(GL_FRONT_AND_BACK, b_wireframe ? GL_LINE : GL_FILL);
			printf("> using %s mode\n", b_wireframe ? "wireframe" : "solid");
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

void update_vertex_buffer(uint H, uint V) {
	int a = 0;
	int b = 0;

	sphere_t s;
	vertex corners[2701];
	for (uint i = 0; i <= H; i++) {
		for (uint j = 0; j <= V; j++) {
			float theta = PI * 2.0f * i / float(H), c_theta = cos(theta), s_theta = sin(theta);
			float phi = PI * j / float(V), c_phi = cos(phi), s_phi = sin(phi);
			corners[a].pos = vec3(s.rotat_radius * s_phi * c_theta, s.rotat_radius * s_phi * s_theta, s.rotat_radius * c_phi);
			corners[a].norm = vec3(s_phi * c_theta, s_phi * s_theta, c_phi);
			corners[a].tex = vec2(theta / (2 * PI), 1 - phi / PI);
			a++;
		}
	}

	vertex vertices[15552];
	for (uint i = 0; i < H; i++) {
		for (uint j = 0; j < V; j++) {
			Vert = i * (V + 1) + j;
			Hori = (i + 1) * (V + 1) + j - 1;
			vertices[b] = corners[Hori];
			vertices[b + 1] = corners[Vert];
			vertices[b + 2] = corners[Hori + 1];
			vertices[b + 3] = corners[Hori + 1];
			vertices[b + 4] = corners[Vert];
			vertices[b + 5] = corners[Vert + 1];
			b += 6;
		}
	}

	// generation of vertex buffer is the same, but use vertices instead of corners
	GLuint vertex_buffer;
	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// generate vertex array object, which is mandatory for OpenGL 3.3 and higher
	if (vertex_array) glDeleteVertexArrays(1, &vertex_array);
	vertex_array = cg_create_vertex_array(vertex_buffer);
	if (!vertex_array) { printf("%s(): failed to create vertex aray\n", __func__); return ; }

	// load the Planet image to a texture
	for (int i = 0; i < 9; i++) {
		PLANETTEX[i] = cg_create_texture(mesh_texture_path[i], true); if (!PLANETTEX[i]) return ;
	}
	for (int i = 0; i < 9; i++) {
		PLANETNORMTEX[i] = cg_create_texture(mesh_normal_texture_path[i], true); if (!PLANETNORMTEX[i]) return;
	}
}

void update_torus_vertex_buffer(uint H, uint V) {
	int a = 0;
	int b = 0;

	torus_t t;
	vertex torus[2701];
	for (uint i = 0; i <= H; i++) {
		for (uint j = 0; j <= V; j++) {
			float theta = PI * 2.0f * i / float(H), c_theta = cos(theta), s_theta = sin(theta);
			float phi = PI * j / float(V), c_phi = cos(phi), s_phi = sin(phi);
			torus[a].pos = vec3((t.Radius + t.radius * s_phi) * c_theta, (t.Radius + t.radius * s_phi) * s_theta, t.height * t.radius * c_phi);
			torus[a].norm = vec3(s_phi * c_theta, s_phi * s_theta, c_phi);
			torus[a].tex = vec2(theta / (2 * PI), 1 - phi / PI);
			a++;
		}
	}
	
	vertex torus_vertices[15984];
	b = 0;
	for (uint i = 0; i < 72; i++) {
		for (uint j = 0; j < 37; j++) {
			Vert = i * (V + 1) + j;
			Hori = (i + 1) * (V + 1) + j - 1;
			torus_vertices[b] = torus[Hori];
			torus_vertices[b + 1] = torus[Vert];
			torus_vertices[b + 2] = torus[Hori + 1];
			torus_vertices[b + 3] = torus[Hori + 1];
			torus_vertices[b + 4] = torus[Vert];
			torus_vertices[b + 5] = torus[Vert + 1];
			b += 6;
		}
	}

	GLuint torus_vertex_buffer;
	glGenBuffers(1, &torus_vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, torus_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(torus_vertices), torus_vertices, GL_STATIC_DRAW);

	if (torus_vertex_array) glDeleteVertexArrays(1, &torus_vertex_array);
	torus_vertex_array = cg_create_vertex_array(torus_vertex_buffer);
	if (!torus_vertex_array) { printf("%s(): failed to create vertex aray\n", __func__); return ; }

	for (int i = 0; i < 2; i++) {
		RINGTEX[i] = cg_create_texture(mesh_ring_texture_path[i], true); if (!RINGTEX[i]) return ;
	}

	for (int i = 0; i < 2; i++) {
		RINGALPHATEX[i] = cg_create_texture(mesh_ring_alpha_texture_path[i], true); if (!RINGALPHATEX[i]) return;
	}
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
	glEnable(GL_TEXTURE_2D);			// enable texturing
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glActiveTexture(GL_TEXTURE0);		// notify GL the current texture slot is 0
	glActiveTexture(GL_TEXTURE1);
	
	update_vertex_buffer(2 * NUM_TESS, NUM_TESS);
	update_torus_vertex_buffer(2 * NUM_TESS, NUM_TESS);
	for (int i = 0; i < 4; i++) {
		SATELLITE[i] = cg_create_texture(mesh_satellite_texture_path[i], true); if (!SATELLITE[i]) return false;
	}
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