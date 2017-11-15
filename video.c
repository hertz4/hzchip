#include <assert.h>
#include <epoxy/gl.h>
#include <SDL2/SDL.h>

#include "video.h"
#include "file.h"

#define RES_X 240
#define RES_Y 160

static GLuint load_shader(const char *filename, GLenum kind);

static struct video_mem mem;
static SDL_Window *window;

static GLfloat verts[8] = {0, 0, RES_X, 0, 0, RES_Y, RES_X, RES_Y};
static GLuint program, vbo, texo;
static GLint win_size, bg_color, bitmap;

struct video_mem *const video_mem()
{
	return &mem;
}

void video_sync()
{
	int win_w, win_h;

	SDL_GetWindowSize(window, &win_w, &win_h);
	glViewport(0, 0, win_w, win_h);

	glUseProgram(program);
	glUniform2f(win_size, win_w, win_h);
	glUniform3f(bg_color, mem.bg_col.r, mem.bg_col.g, mem.bg_col.b);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	SDL_GL_SwapWindow(window);
}

void video_init()
{
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	memset(&mem, 0, sizeof(mem));

	window = SDL_CreateWindow(
		 "it works",
		 SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		 800, 600,
		 SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

	SDL_GL_SetSwapInterval(1);
	SDL_GLContext context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, context);

#ifdef DEBUG
	{
		int v = epoxy_gl_version();
		printf("Initialized GL Version: %d.%d\n", v/10, v%10);
	}
#endif

	program = glCreateProgram();
	GLuint vertex = load_shader("vertex.glsl", GL_VERTEX_SHADER);
	GLuint fragment = load_shader("fragment.glsl", GL_FRAGMENT_SHADER);
	if (!vertex || !fragment) {
		return;
	}
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);
	glUseProgram(program);

	glDeleteShader(vertex);
	glDeleteShader(fragment);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, NULL);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

	glGenTextures(1, &texo);
	glBindTexture(GL_TEXTURE_2D, texo);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	static GLbyte rando[64*64];
	for(int i=0; i<sizeof(rando); ++i) rando[i] = rand()>>16;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 64, 64, 0, GL_RED, GL_BYTE, rando);
	bitmap = glGetUniformLocation(program, "bitmap");
	glUniform1i(bitmap, 0);

#ifdef DEBUG
	GLenum err = glGetError();
	if(err) printf("%x\n", err);
#endif

	GLint screen_size = glGetUniformLocation(program, "screen_size");
	glUniform2f(screen_size, RES_X, RES_Y);

	win_size = glGetUniformLocation(program, "win_size");
	bg_color = glGetUniformLocation(program, "bg_color");
}

void video_quit()
{
}

static GLuint load_shader(const char *filename, GLenum kind)
{
	const char *source = file_to_string(filename);

	GLuint shader = glCreateShader(kind);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint compiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	GLchar *log;
	GLint len;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
	if (len) {
		log = malloc(len);
		glGetShaderInfoLog(shader, len, NULL, log);
		printf("%s:\n\t%s", filename, log);
		free(log);
	}

	return compiled ? shader : 0;
}
