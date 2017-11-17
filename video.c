#include <assert.h>
#include <epoxy/gl.h>
#include <SDL2/SDL.h>

#include "video.h"
#include "file.h"

#define RES_X 240
#define RES_Y 160

static GLuint load_shader(const char *filename, GLenum kind);
static struct video_color SDL_to_float(SDL_Color col);

static struct video_mem mem;
static SDL_Window *window;

static GLfloat verts[8] = {0, 0, RES_X, 0, 0, RES_Y, RES_X, RES_Y};
static GLuint program, vbo;
struct {
	GLint win_size, viewport, scroll,
	      palette, bitmap, bpp, tilemap;
} uniform;

typedef unsigned int uint;

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

	glUniform2f(uniform.win_size, win_w, win_h);
	glUniform4fv(uniform.palette, 256, mem.palette_raw);
	glUniform1uiv(uniform.bitmap, 64*8, mem.bitmap);
	glUniform1uiv(uniform.tilemap, 32*32, mem.tiles_raw);

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

#ifdef DEBUG
	GLenum err = glGetError();
	if(err) printf("%x\n", err);
#endif

#define U(name) uniform. name = glGetUniformLocation(program, #name)
	U(win_size);
	U(viewport);
	U(scroll);
	U(palette);
	U(bitmap);
	U(bpp);
	U(tilemap);
#undef U

	glUniform2f(uniform.viewport, RES_X, RES_Y);
}

void video_quit()
{
}

void video_loadbmp(const char *path, unsigned bpp)
{
	SDL_Surface *img = SDL_LoadBMP(path);
	if(!img) {
		fprintf(stderr, "%s", SDL_GetError());
		return;
	}
	SDL_Palette *pal = img->format->palette;
	if(!pal) {
		fprintf(stderr, "Attempt to use non-indexed image\n");
		return;
	}
	const uint mask = (1<<bpp)-1;
	const uint bpi = 32/bpp; // bits per int

	uint max = img->w * img->h;
	uint bitmap_max = sizeof(mem.bitmap) * bpi / sizeof(*mem.bitmap);
	printf("Loading image %s, size %d tiles, into %dbpp (max %d) tile page.",
			path, max/64, bpp, 256/bpp);

	if (max > bitmap_max) max = bitmap_max;
	for(unsigned i=0; i<max; ++i) {
		// Find actual x and y
		uint x = i % img->w;
		uint y = i / img->w;
		// Chop image into 8-tall strips
		x += (y/8) * img->w;
		y %= 8;
		// Find position within tile
		uint pos = x%8 + y*8 + (x/8)*64;
		// Pack into lower bit depth
		char px = ((char*)img->pixels)[i];
		mem.bitmap[pos / bpi] |= (px & mask) << ((pos % bpi) * bpp);
	}
	for(int i=0; i<256; ++i) {
		mem.palette[i] = SDL_to_float(pal->colors[i]);
	}
	glUniform1ui(uniform.bpp, bpp);
}

static struct video_color SDL_to_float(SDL_Color col)
{
	return (struct video_color){
		((GLfloat)col.r) / 256.0,
		((GLfloat)col.g) / 256.0,
		((GLfloat)col.b) / 256.0,
		((GLfloat)col.a) / 256.0,
	};
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
