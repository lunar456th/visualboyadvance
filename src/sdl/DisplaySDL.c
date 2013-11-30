// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 2008 VBA-M development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include "DisplaySDL.h"
#include "GameScreen.h"
#include "../common/Settings.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <png.h>

static const int screenWidth = 240;
static const int screenHeigth = 160;

struct Display {
	SDL_Window *window;
	SDL_Renderer *renderer;
	GNode *renderables;

	gboolean fullscreen;
};

static gboolean display_sdl_render_node(GNode *node, gpointer data) {
	Renderable *renderable = (Renderable *)node->data;
	if (renderable && renderable->render) {
		renderable->render(renderable->entity);
	}
}

void display_sdl_render(Display *display) {
	g_assert(display != NULL);

	// Clear the screen
	SDL_RenderClear(display->renderer);

	// Then render the renderables
	g_node_traverse(display->renderables, G_PRE_ORDER, G_TRAVERSE_ALL, -1, display_sdl_render_node, NULL);

	SDL_RenderPresent(display->renderer);
}

static gboolean display_sdl_create_window(Display *display, GError **err) {
	g_return_val_if_fail(err == NULL || *err == NULL, FALSE);
	g_assert(display != NULL);

	guint zoomFactor = settings_zoom_factor();

	int flags = SDL_WINDOW_RESIZABLE;
	flags |= display->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;

	display->window = SDL_CreateWindow("Visual Boy Advance",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screenWidth * zoomFactor, screenHeigth * zoomFactor, flags);
	if (display->window == NULL) {
		g_set_error(err, DISPLAY_ERROR, G_DISPLAY_ERROR_FAILED,
				"Failed to create window: %s", SDL_GetError());
		return FALSE;
	}

	// Let 480*320 be our minimum screen size
	SDL_SetWindowMinimumSize(display->window, screenWidth * 2, screenHeigth * 2);

	display->renderer = SDL_CreateRenderer(display->window, -1, SDL_RENDERER_ACCELERATED);
	if (display->renderer == NULL) {
		g_set_error(err, DISPLAY_ERROR, G_DISPLAY_ERROR_FAILED,
				"Failed to create renderer: %s", SDL_GetError());
		return FALSE;
	}

	return TRUE;
}

Display *display_sdl_init(GError **err) {
	g_return_val_if_fail(err == NULL || *err == NULL, NULL);

	SDL_InitSubSystem(SDL_INIT_VIDEO);

	if (TTF_Init()) {
		g_set_error(err, DISPLAY_ERROR, G_DISPLAY_ERROR_FAILED,
				"Failed to initialize true type rendering: %s", TTF_GetError());
		return FALSE;
	}

	Display *display = g_new(Display, 1);
	display->window = NULL;
	display->renderer = NULL;
	display->renderables = g_node_new(NULL);
	display->fullscreen = settings_is_fullscreen();

	if (!display_sdl_create_window(display, err)) {
		display_sdl_free(display);
		return NULL;
	}

	return display;
}

void display_sdl_free(Display *display) {
	if (display == NULL)
		return;

	SDL_DestroyRenderer(display->renderer);
	SDL_DestroyWindow(display->window);

	SDL_QuitSubSystem(SDL_INIT_VIDEO);

	TTF_Quit();

	g_node_destroy(display->renderables);

	g_free(display);
}

gboolean display_sdl_is_fullscreen(Display *display) {
	g_assert(display != NULL);

	return display->fullscreen;
}

gboolean display_sdl_toggle_fullscreen(Display *display, GError **err) {
	g_return_val_if_fail(err == NULL || *err == NULL, FALSE);
	g_assert(display != NULL);

	if (SDL_SetWindowFullscreen(display->window, display->fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) < 0) {
		return FALSE;
	}

	display->fullscreen = !display->fullscreen;

	return TRUE;
}

void display_sdl_set_window_title(Display *display, const gchar *title) {
	g_assert(display != NULL);

	if (title != NULL) {
		gchar *fullTitle = g_strdup_printf("%s - Visual Boy Advance", title);
		SDL_SetWindowTitle(display->window, fullTitle);
		g_free (fullTitle);
	} else {
		SDL_SetWindowTitle(display->window, "Visual Boy Advance");
	}
}

Renderable *display_sdl_renderable_create(Display *display, gpointer entity, Renderable *parent) {
	g_assert(display != NULL);

	Renderable *renderable = g_new(Renderable, 1);
	renderable->entity = entity;
	renderable->display = display;
	renderable->renderer = display->renderer;
	renderable->render = NULL;
	renderable->parent = parent;
	renderable->x = 0;
	renderable->y = 0;
	renderable->width = 0;
	renderable->height = 0;

	g_node_append_data(display->renderables, renderable);

	return renderable;
}

void display_sdl_renderable_free(Renderable *renderable) {
	if (renderable == NULL)
		return;

	Display *display = renderable->display;

	GNode *node = g_node_find_child(display->renderables, G_TRAVERSE_ALL, renderable);

	g_assert(node != NULL && G_NODE_IS_LEAF(node));

	g_node_destroy(node);

	g_free(renderable);
}

void display_sdl_renderable_set_position(Renderable *renderable, gint x, gint y) {
	g_assert(renderable != NULL);

	renderable->x = x;
	renderable->y = y;
}

void display_sdl_renderable_set_size(Renderable *renderable, gint width, gint height) {
	g_assert(renderable != NULL);

	renderable->width = width;
	renderable->height = height;
}

void display_sdl_renderable_get_absolute_position(Renderable *renderable, gint *x, gint *y) {
	g_assert(renderable != NULL && x != NULL && y != NULL);

	if (renderable->parent == NULL) {
		int windowWidth, windowHeight;
		SDL_GetRendererOutputSize(renderable->renderer, &windowWidth, &windowHeight);

		if (renderable->x >= 0) {
			*x = renderable->x;
		} else {
			*x = windowWidth + renderable->x - renderable->width;
		}

		if (renderable->y >= 0) {
			*y = renderable->y;
		} else {
			*y = windowHeight + renderable->y - renderable->height;
		}

	} else {
		// TODO: Allow right / bottom alignment for nested renderables
		g_assert(renderable->x >= 0 && renderable->y >= 0);

		gint parentX, parentY;
		display_sdl_renderable_get_absolute_position(renderable->parent, &parentX, &parentY);

		*x = parentX + renderable->x;
		*y = parentY + renderable->y;
	}
}

SDL_Texture *display_sdl_load_png(Display *display, const gchar *filename, GError **err) {
	g_return_val_if_fail(err == NULL || *err == NULL, NULL);
	g_assert(display != NULL);

	png_image image;
	memset(&image, 0, sizeof(image));
	image.version = PNG_IMAGE_VERSION;

	if (!png_image_begin_read_from_file(&image, filename)) {
		g_set_error(err, DISPLAY_ERROR, G_DISPLAY_ERROR_FAILED,
				"Failed to load png: %s", image.message);
		return NULL;
	}

	png_bytep buffer = (png_bytep) g_malloc(PNG_IMAGE_SIZE(image));
	image.format = PNG_FORMAT_RGBA;

	if (!png_image_finish_read(&image, NULL, buffer, 0, NULL)) {
		g_free(buffer);
		g_set_error(err, DISPLAY_ERROR, G_DISPLAY_ERROR_FAILED,
				"Failed to load png: %s", image.message);
		return NULL;
	}

	SDL_Texture *texture = SDL_CreateTexture(display->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, image.width, image.height);
	if (texture == NULL) {
		g_free(buffer);
		g_set_error(err, DISPLAY_ERROR, G_DISPLAY_ERROR_FAILED,
				"Failed create texture : %s", SDL_GetError());
		return NULL;
	}

	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
	SDL_UpdateTexture(texture, NULL, buffer, image.width * sizeof(guint32));

	g_free(buffer);
	return texture;
}
