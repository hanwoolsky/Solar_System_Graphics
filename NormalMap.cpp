#include "cgmath.h"		// slee's simple math library
#define STB_IMAGE_IMPLEMENTATION
#include "cgut.h"		// slee's OpenGL utility
#include "trackball.h"

//*************************************
// global constants
static const char*	window_name			= "cgbase - texture";
static const char*	vert_shader_path	= "../bin/shaders/texture.vert";
static const char*	frag_shader_path	= "../bin/shaders/texture.frag";
static const char*	image_path		= "../bin/images/earth.jpg";
static const char*	normal_image_path = "../bin/images/earth-normal.jpg";
static const char* bump_image_path = "../bin/images/earth-bump.jpg";

//*************************************
// common structures
struct camera
{
	vec3	eye = vec3(3, 0, 0);
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
	vec4	position = vec4(2.0f, 0.0f, 0.0f, 1.0f);   // directional light
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
GLFWwindow*	window = nullptr;
ivec2		window_size = cg_scale_by_dpi( 512, 512 ); // use ivec2(width,height) to ignore dpi-aware scale

//*************************************
// OpenGL objects
GLuint	program	= 0;		// ID holder for GPU program
GLuint  vertex_array = 0;	// ID holder for vertex array object
GLuint	LENA = 0;			// RGB texture object
GLuint	NORM = 0;			// RGB texture object
GLuint	BUMP = 0;			// RGB texture object

//*************************************
// global variables
int		frame = 0;			// index of rendering frames
uint	mode = 0;			// texture display mode: 0=texcoord, 1=RGB, 2=Gray, 3=Alpha
int		mousebtn = -1;
#ifndef GL_ES_VERSION_2_0
bool	b_wireframe = false;
bool	shift = false;
bool	ctrl = false;
#endif

//*************************************
// scene objects
camera cam;
trackball tb;
light_t	light;
material_t material;

//*************************************
void update()
{
	glUseProgram( program );
	glUniform1i( glGetUniformLocation( program, "mode"), mode );
	// update projection matrix
	cam.aspect = window_size.x / float(window_size.y);
	cam.projection_matrix = mat4::perspective(cam.fovy, cam.aspect, cam.dnear, cam.dfar);

	// build the model matrix for oscillating scale
	float t = float(glfwGetTime());

	float c = cos(t), s = sin(t);
	mat4 rotation_matrix =
	{
		c,-s, 0, 0,
		s, c, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};

	mat4 model_matrix = rotation_matrix;

	// update uniform variables in vertex/fragment shaders
	GLint uloc;
	uloc = glGetUniformLocation(program, "view_matrix");			if (uloc > -1) glUniformMatrix4fv(uloc, 1, GL_TRUE, cam.view_matrix);
	uloc = glGetUniformLocation(program, "projection_matrix");		if (uloc > -1) glUniformMatrix4fv(uloc, 1, GL_TRUE, cam.projection_matrix);
	uloc = glGetUniformLocation(program, "model_matrix");			if (uloc > -1) glUniformMatrix4fv(uloc, 1, GL_TRUE, model_matrix);

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
	// clear screen (with background color) and clear depth buffer
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	// bind program
	glUseProgram( program );
	//glBindVertexArray(vertex_array);
	// bind textures
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, LENA );
	glUniform1i( glGetUniformLocation( program, "TEX0"), 0 );

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, NORM);
	glUniform1i(glGetUniformLocation(program, "NORM"), 1);

	// bind vertex array object
	glBindVertexArray( vertex_array );

	// render quad vertices
	glDrawArrays( GL_TRIANGLES, 0, 15984 );

	// swap front and back buffers, and display to screen
	glfwSwapBuffers( window );
}

void reshape( GLFWwindow* window, int width, int height )
{
	// set current viewport in pixels (win_x, win_y, win_width, win_height)
	// viewport: the window area that are affected by rendering 
	window_size = ivec2( width, height );
	glViewport( 0, 0, width, height );
}

void print_help()
{
	printf( "[help]\n" );
	printf( "- press ESC or 'q' to terminate the program\n" );
	printf( "- press F1 or 'h' to see help\n" );
	printf( "- press 'd' to toggle display mode (0: texcoord, 1: RGB, 2: Gray, 3: Alpha\n" );
	printf( "\n" );
}

void keyboard( GLFWwindow* window, int key, int scancode, int action, int mods )
{
	if(action==GLFW_PRESS)
	{
		if(key==GLFW_KEY_ESCAPE||key==GLFW_KEY_Q)	glfwSetWindowShouldClose( window, GL_TRUE );
		else if(key==GLFW_KEY_H||key==GLFW_KEY_F1)	print_help();
		else if(key==GLFW_KEY_D)
		{
			mode = (mode+1)%4;
			printf( "using mode %d: %s\n", mode, mode==0?"texcoord":mode==1?"RGB":mode==2?"Gray":"Alpha" );
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
}

void mouse(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT || GLFW_MOUSE_BUTTON_RIGHT || GLFW_MOUSE_BUTTON_MIDDLE) {
		dvec2 pos; glfwGetCursorPos(window, &pos.x, &pos.y);
		printf("%f %f\n", pos.x, pos.y);
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

// this function will be avaialble as cg_create_texture() in other samples
GLuint create_texture( const char* image_path, bool mipmap=true, GLenum wrap=GL_CLAMP_TO_EDGE, GLenum filter=GL_LINEAR )
{
	// load image
	image*	i = cg_load_image( image_path ); if(!i) return 0; // return null texture; 0 is reserved as a null texture
	int		w=i->width, h=i->height, c=i->channels;

	// induce internal format and format from image
	GLint	internal_format = c==1?GL_R8:c==2?GL_RG8:c==3?GL_RGB8:GL_RGBA8;
	GLenum	format = c==1?GL_RED:c==2?GL_RG:c==3?GL_RGB:GL_RGBA;

	// create a src texture (lena texture)
	GLuint texture;
	glGenTextures( 1, &texture ); if(texture==0){ printf("%s(): failed in glGenTextures()\n", __func__ ); return 0; }
	glBindTexture( GL_TEXTURE_2D, texture );
	glTexImage2D( GL_TEXTURE_2D, 0, internal_format, w, h, 0, format, GL_UNSIGNED_BYTE, i->ptr );
	if(i){ delete i; i=nullptr; } // release image

	// build mipmap
	if( mipmap )
	{
		int mip_levels=0; for( int k=w>h?w:h; k; k>>=1 ) mip_levels++;
		for( int l=1; l < mip_levels; l++ )
			glTexImage2D( GL_TEXTURE_2D, l, internal_format, (w>>l)==0?1:(w>>l), (h>>l)==0?1:(h>>l), 0, format, GL_UNSIGNED_BYTE, nullptr );
		if(glGenerateMipmap) glGenerateMipmap(GL_TEXTURE_2D);
	}

	// set up texture parameters
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, !mipmap?filter:filter==GL_LINEAR?GL_LINEAR_MIPMAP_LINEAR:GL_NEAREST_MIPMAP_NEAREST );

	return texture;
}

bool user_init()
{
	// log hotkeys
	print_help();

	// init GL states
	glClearColor( 39/255.0f, 40/255.0f, 34/255.0f, 1.0f );	// set clear color
	glEnable( GL_CULL_FACE );			// turn on backface culling
	glEnable( GL_DEPTH_TEST );			// turn on depth tests
	glEnable( GL_TEXTURE_2D );			// enable texturing
	glActiveTexture( GL_TEXTURE0 );		// notify GL the current texture slot is 0
	glActiveTexture(GL_TEXTURE1);		// notify GL the current texture slot is 1
	glActiveTexture(GL_TEXTURE2);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	vertex corners[2701];
	int k = 0;
	
	for (uint i = 0; i <= 72; i++) {
		for (uint j = 0; j <= 36; j++) {
			float theta = PI * 2.0f * i / float(72), c_theta = cos(theta), s_theta = sin(theta);
			float phi = PI * j / float(36), c_phi = cos(phi), s_phi = sin(phi);
			corners[k].pos = vec3(s_phi * c_theta, s_phi * s_theta, c_phi);
			corners[k].norm = vec3(s_phi * c_theta, s_phi * s_theta, c_phi);
			corners[k].tex = vec2(theta / (2 * PI), 1 - phi / PI);
			k++;
		}
	}

	vertex vertices[15552];
	int m = 0;
	for (uint i = 0; i < 72; i++) {
		for (uint j = 0; j < 36; j++) {
			int Vert = i * 37 + j;
			int Hori = (i + 1) * 37 + j - 1;
			vertices[m] = corners[Hori];
			vertices[m+1] = corners[Vert];
			vertices[m+2] = corners[Hori + 1];
			vertices[m+3] = corners[Hori + 1];
			vertices[m+4] = corners[Vert];
			vertices[m+5] = corners[Vert + 1];
			m += 6;
		}
	}

	// generation of vertex buffer is the same, but use vertices instead of corners
	GLuint vertex_buffer;
	glGenBuffers( 1, &vertex_buffer );
	glBindBuffer( GL_ARRAY_BUFFER, vertex_buffer );
	glBufferData( GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// generate vertex array object, which is mandatory for OpenGL 3.3 and higher
	if(vertex_array) glDeleteVertexArrays(1,&vertex_array);
	vertex_array = cg_create_vertex_array( vertex_buffer );
	if(!vertex_array){ printf("%s(): failed to create vertex aray\n",__func__); return false; }

	// load the Lena image to a texture
	LENA = create_texture( image_path, true ); if(!LENA) return false;
	NORM = create_texture(normal_image_path, true); if (!NORM) return false;
	BUMP = create_texture(bump_image_path, true); if (!BUMP) return false;

	// set default display mode to lena
	keyboard( window, GLFW_KEY_D, 0, GLFW_PRESS, 0 );

	return true;
}

void user_finalize()
{
}

int main( int argc, char* argv[] )
{
	// create window and initialize OpenGL extensions
	if(!(window = cg_create_window( window_name, window_size.x, window_size.y ))){ glfwTerminate(); return 1; }
	if(!cg_init_extensions( window )){ glfwTerminate(); return 1; }	// version and extensions

	// initializations and validations
	if(!(program=cg_create_program( vert_shader_path, frag_shader_path ))){ glfwTerminate(); return 1; }	// create and compile shaders/program
	if(!user_init()){ printf( "Failed to user_init()\n" ); glfwTerminate(); return 1; }					// user initialization

	// register event callbacks
	glfwSetWindowSizeCallback( window, reshape );	// callback for window resizing events
    glfwSetKeyCallback( window, keyboard );			// callback for keyboard events
	glfwSetMouseButtonCallback( window, mouse );	// callback for mouse click inputs
	glfwSetCursorPosCallback( window, motion );		// callback for mouse movement

	// enters rendering/event loop
	for( frame=0; !glfwWindowShouldClose(window); frame++ )
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
