/*
* Gluqlo: Fliqlo for Linux
* https://github.com/alexanderk23/gluqlo
*
* Copyright (c) 2010-2012 Kuźniarski Jacek
* Copyright (c) 2014 Alexander Kovalenko
*
* Permission to use, copy, modify, and distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
* ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <time.h>

#include "SDL.h"
#include "SDL_ttf.h"
#include "SDL_syswm.h"
#include "SDL2_gfxPrimitives.h"
#include "SDL2_rotozoom.h"

#ifndef FONT
#define FONT "/usr/share/gluqlo/gluqlo.ttf"
#endif

const char* TITLE = "Gluqlo 1.1";
const int DEFAULT_WIDTH = 1024;
const int DEFAULT_HEIGHT = 768;

bool twentyfourh = true;
bool leadingzero = false;
bool fullscreen = false;
bool animate = true;
bool anykeyclose = false;

int past_h = -1, past_m = -1;

int width = DEFAULT_WIDTH;
int height = DEFAULT_HEIGHT;
int sethour = 0;
int setmin = 0;
bool settime = false;

TTF_Font *font_time = NULL;
TTF_Font *font_mode = NULL;

const SDL_Color FONT_COLOR = { 0xb7, 0xb7, 0xb7, 0xff };
const SDL_Color BACKGROUND_COLOR = { 0x0f, 0x0f, 0x0f, 0xff };

SDL_Surface *screen;
SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;

SDL_Rect hourBackground;
SDL_Rect minBackground;

SDL_Rect bgrect;
SDL_Surface *bg;

// draw rounded box
// see http://lists.libsdl.org/pipermail/sdl-libsdl.org/2006-December/058868.html
void fill_rounded_box_b(SDL_Surface* dst, SDL_Rect *coords, int r, SDL_Color color) {
	Uint32 pixcolor = SDL_MapRGBA(dst->format, color.r, color.g, color.b, color.a);

	int i, j;
	int rpsqrt2 = (int) (r / sqrt(2));
	int yd = dst->pitch / dst->format->BytesPerPixel;
	int w = coords->w / 2 - 1;
	int h = coords->h / 2 - 1;
	int xo = coords->x + w;
	int yo = coords->y + h;

	w -= r;
	h -= r;

	if(w <= 0 || h <= 0) return;

	SDL_LockSurface(dst);
	Uint32 *pixels = (Uint32*)(dst->pixels);

	int sy = (yo - h) * yd;
	int ey = (yo + h) * yd;
	int sx = xo - w;
	int ex = xo + w;

	for(i = sy; i <= ey; i += yd)
		for(j = sx - r; j <= ex + r; j++)
			pixels[i + j] = pixcolor;

	int d = -r;
	int x2m1 = -1;
	int y = r;

	for(int x = 0; x <= rpsqrt2; x++) {
		x2m1 += 2;
		d += x2m1;
		if(d >= 0) {
			y--;
			d -= y * 2;
		}

		for(i = sx - x; i <= ex + x; i++) {
			pixels[sy - y * yd + i] = pixcolor;
		}

		for(i = sx - y; i <= ex + y; i++) {
			pixels[sy - x * yd + i] = pixcolor;
		}

		for(i = sx - y; i <= ex + y; i++) {
			pixels[ey + x * yd + i] = pixcolor;
		}

		for(i = sx - x; i <= ex + x; i++) {
			pixels[ey + y * yd + i] = pixcolor;
		}
	}

	SDL_UnlockSurface(dst);
}

void render_ampm(SDL_Surface *surface, SDL_Rect *rect, int pm) {
	char mode[3];
	SDL_Rect coords;
	snprintf(mode, 3, "%cM", pm ? 'P' : 'A');
	SDL_Surface *ampm = TTF_RenderText_Blended(font_mode, mode, FONT_COLOR);
	int offset = rect->h * 0.127;
	coords.x = rect->x + rect->h * 0.07;
	coords.y = rect->y + (pm ? rect->h - offset - ampm->h : offset);
	SDL_BlitSurface(ampm, 0, surface, &coords);
	SDL_FreeSurface(ampm);
}



void blit_digits(SDL_Surface *surface, SDL_Rect *rect, int spc, char digits[], SDL_Color color) {
	int min_x, max_x, min_y, max_y, advance;
	int adjust_x = (digits[0] == '1') ? 2.5 * spc : 0; // special case
	int center_x = rect->x + rect->w / 2 - adjust_x;

	SDL_Surface *glyph;
	SDL_Rect coords;

	if(digits[1]) {
		// first digit
		TTF_GlyphMetrics(font_time, digits[0], &min_x, &max_x, &min_y, &max_y, &advance);
		glyph = TTF_RenderGlyph_Blended(font_time, digits[0], color);
		coords.x = center_x - max_x + min_x - spc - (adjust_x * 2);
		coords.y = rect->y + (rect->h - glyph->h) / 2;
		SDL_BlitSurface(glyph, 0, surface, &coords);
		SDL_FreeSurface(glyph);
		// second digit
		TTF_GlyphMetrics(font_time, digits[1], &min_x, &max_x, &min_y, &max_y, &advance);
		glyph = TTF_RenderGlyph_Blended(font_time, digits[1], color);
		coords.y = rect->y + (rect->h - glyph->h) / 2;
		coords.x = center_x + spc / 2;
		SDL_BlitSurface(glyph, 0, surface, &coords);
		SDL_FreeSurface(glyph);
	} else {
		// single digit
		glyph = TTF_RenderGlyph_Blended(font_time, digits[0], color);
		coords.x = center_x - glyph->w / 2;
		coords.y = rect->y + (rect->h - glyph->h) / 2;
		SDL_BlitSurface(glyph, 0, surface, &coords);
		SDL_FreeSurface(glyph);
	}
}


void render_digits(SDL_Surface *surface, SDL_Rect *background, char digits[], char prevdigits[], int maxsteps, int step) {
	SDL_Rect rect, dstrect;
	SDL_Color color;
	double scale;
	Uint8 c;

	// int spc = surface->h * .0125;
	bool is_h = surface->h < surface->w;
	int spc = is_h ? surface->h * .0125 : surface->w * .0125;

	// blit upper halves of current digits
	rect.x = background->x;
	rect.y = background->y;
	rect.w = background->w;
	rect.h = background->h/2;
	SDL_SetClipRect(surface, &rect);
	SDL_BlitSurface(bg, 0, surface, &rect);
	blit_digits(surface, background, spc, digits, FONT_COLOR);
	SDL_SetClipRect(surface, NULL);

	int halfsteps = maxsteps / 2;
	int upperhalf = (step+1) <= halfsteps;
	if(upperhalf) {
		scale = 1.0 - (1.0 * step) / (halfsteps - 1);
		c = 0xb7 - 0xb7 * (1.0 * step) / (halfsteps - 1);
	} else {
		scale = ((1.0 * step) - halfsteps + 1) / halfsteps;
		c = 0xb7 * ((1.0 * step) - halfsteps + 1) / halfsteps;
	}
	color.r = color.g = color.b = c;
	color.a = 0xff;

	// create surface to scale from filled background surface
	SDL_Surface *bgcopy = SDL_ConvertSurface(bg, bg->format, bg->flags);
	rect.x = 0;
	rect.y = 0;
	rect.w = bgcopy->w;
	rect.h = bgcopy->h;
	blit_digits(bgcopy, &rect, spc, upperhalf ? prevdigits : digits, color);

	// scale and blend it to dest
	SDL_Surface *scaled = zoomSurface(bgcopy, 1.0, scale, 1);
	rect.x = 0;
	rect.y = upperhalf ? 0 : scaled->h / 2;
	rect.w = scaled->w;
	rect.h = scaled->h / 2;
	dstrect.x = background->x;
	dstrect.y = background->y + ( upperhalf ? ((background->h - scaled->h) / 2) : background->h / 2);
	dstrect.w = rect.w;
	dstrect.h = rect.h;	
	SDL_SetClipRect(surface, &dstrect);
	SDL_BlitSurface(scaled, &rect, surface, &dstrect);
	SDL_SetClipRect(surface, NULL);
	SDL_FreeSurface(scaled);
	SDL_FreeSurface(bgcopy);

	if(!animate) return;
	// draw divider
	rect.h = (is_h ? surface->h : surface->w) * 0.005;
	rect.w = background->w;
	rect.x = background->x;
	rect.y = background->y + (background->h - rect.h) / 2;
	SDL_FillRect(surface, &rect, SDL_MapRGBA(surface->format, 0, 0, 0, 0xff));
	rect.y += rect.h;
	rect.h = 1;
	SDL_FillRect(surface, &rect, SDL_MapRGBA(surface->format, 0x1a, 0x1a, 0x1a, 0xff));
}

void render_clock(int maxsteps, int step) {
	char buffer[3], buffer2[3];
	struct tm *_time;
	time_t rawtime;
    int hour, minute;

	time(&rawtime);
	_time = localtime(&rawtime);
    if (settime) {
        hour = sethour;
        minute = setmin;
    }
    else {
        hour = _time->tm_hour;
        minute = _time->tm_min;
    }

	// draw hours
	if(hour != past_h) {
		int h = twentyfourh ? hour : (hour + 11) % 12 + 1;
		if(leadingzero) {
			snprintf(buffer, 3, "%02d", h);
			snprintf(buffer2, 3, "%02d", past_h);
		} else {
			snprintf(buffer, 3, "%d", h);
			snprintf(buffer2, 3, "%d", past_h);
		}
		render_digits(screen, &hourBackground, buffer, buffer2, maxsteps, step);
		// draw am/pm
		if(!twentyfourh) render_ampm(screen, &hourBackground, hour >= 12);
	}

	// draw minutes
	if(minute != past_m) {
		snprintf(buffer, 3, "%02d", minute);
		snprintf(buffer2, 3, "%02d", past_m);
		render_digits(screen, &minBackground, buffer, buffer2, maxsteps, step);
	}

	// update screen with pixels
	SDL_UpdateTexture(texture, NULL, screen->pixels, screen->pitch);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);

	if(step == maxsteps-1) {
		past_h = hour;
		past_m = minute;
	}
}

void render_animation() {
	if(!animate) {
		render_clock(20, 19);
		return;
	}

	const int DURATION = 260;
	int start_tick = SDL_GetTicks();
	int end_tick = start_tick + DURATION;
	int current_tick;
	int frame;
	int done = 0;

	while(!done) {
		current_tick = SDL_GetTicks();
		if(current_tick >= end_tick) {
			done = 1;
			current_tick = end_tick;
		}
		frame = 99 * (current_tick-start_tick) / (end_tick-start_tick);
		render_clock(100, frame);
	}
}

Uint32 update_time(Uint32 interval, void *param) {
	SDL_Event e;
	time_t rawtime;
	struct tm *time_i;
    int minute;

	time(&rawtime);
	time_i = localtime(&rawtime);
    minute = settime ? setmin : time_i->tm_min;

	if(minute != past_m) {
		e.type = SDL_USEREVENT;
		e.user.code = 0;
		e.user.data1 = NULL;
		e.user.data2 = NULL;
		SDL_PushEvent(&e);
		interval = 1000 * (60 - time_i->tm_sec) - 250;
	} else {
		interval = 250;
	}

	return interval;
}

int main(int argc, char** argv ) {
	char *wid_env;
	double display_scale_factor = 1;

	Uint32 wid = 0;
	Display *display;
	XWindowAttributes windowAttributes;

	for(int i = 1; i < argc; i++) {
		if(strcmp("--help",argv[i]) == 0 || strcmp("-help", argv[i]) == 0) {
			printf("Usage: %s [OPTION...]\nOptions:\n", argv[0]);
			printf("  -help\t\tDisplay this\n");
			printf("  -root, -f\tFullscreen\n");
			printf("  -noflip\tDisable the flip animation (change time in one frame)\n");
			printf("  -anykeyclose\tClose app when mouse move or any key pressed\n");
			printf("  -ampm\t\tUse 12-hour clock format (AM/PM)\n");
			printf("  -leadingzero\tAlways display hour with two digits\n");
			printf("  -w\t\tCustom width\n");
			printf("  -h\t\tCustom height\n");
			printf("  -r\t\tCustom resolution in WxH format\n");
			printf("  -s\t\tCustom display scale factor\n");
			return 0;
		} else if(strcmp("-root", argv[i]) == 0 || strcmp("-f", argv[i]) == 0 || strcmp("--fullscreen", argv[i]) == 0) {
			fullscreen = true;
		} else if(strcmp("-noflip", argv[i]) == 0) {
			animate = false;
		} else if(strcmp("-anykeyclose", argv[i]) == 0) {
			anykeyclose = true;
		} else if(strcmp("-ampm", argv[i]) == 0) {
			twentyfourh = false;
		} else if(strcmp("-leadingzero", argv[i]) == 0) {
			leadingzero = true;
		} else if(strcmp("-r", argv[i]) == 0 || strcmp("--resolution", argv[i]) == 0) {
			char *resolution = argv[i+1];
			char *val = strtok(resolution, "x");
			width = atoi(val);
			val = strtok(NULL, "x");
			height = atoi(val);
			i++;
		} else if(strcmp("-w", argv[i]) == 0) {
			width = atoi(argv[i+1]);
			i++;
		} else if(strcmp("-h", argv[i]) == 0) {
			height = atoi(argv[i+1]);
			i++;
		} else if(strcmp("-s", argv[i]) == 0) {
			display_scale_factor = atof(argv[i+1]);
			i++;
		} else if(strcmp("-window-id", argv[i]) == 0 || strcmp("--window-id", argv[i]) == 0) {
			wid = strtol(argv[i+1], (char **) NULL, 0);
			i++;
		} else if (strcmp("-time", argv[i]) == 0) {
            char *time = argv[i+1];
            char *val = strtok(time, ":");
            sethour = atoi(val);
            val = strtok(NULL, ":");
            setmin = atoi(val);
            settime = true;
            i++;
        } else {
			printf("Invalid option -- %s\n", argv[i]);
			printf("Try --help for more information.\n");
			return 0;
		}
	}

	/* If no window argument, check environment */
	if(wid == 0) {
		if ((wid_env = getenv("XSCREENSAVER_WINDOW")) != NULL ) {
			wid = strtol(wid_env, (char **) NULL, 0); /* Base 0 autodetects hex/dec */
		}
	}

	/* Get win attrs if we've been given a window, otherwise we'll use our own */
	if(wid != 0) {
		if ((display = XOpenDisplay(NULL)) != NULL) { /* Use the default display */
			XGetWindowAttributes(display, (Window) wid, &windowAttributes);
			XCloseDisplay(display);
			width = windowAttributes.width;
			height = windowAttributes.height;
			fullscreen = windowAttributes.override_redirect;
		}
	}

	if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
		fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
		return 1;
	}
	atexit(SDL_Quit);

	if(!wid) {
		if(!fullscreen) {
			window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_UNDEFINED,
					SDL_WINDOWPOS_UNDEFINED, width, height, 0);
		}
		else {
			window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_UNDEFINED,
					SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
		}
	} else {
		window = SDL_CreateWindowFrom((void *)(Window)wid);
		if(fullscreen) {
			SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		}
	}

	if (!window) {
		fprintf(stderr, "Unable to create window: %s\n", SDL_GetError());
		return 1;
	}

	if(fullscreen || wid) {
		SDL_ShowCursor(SDL_DISABLE);
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, width, height);
	screen = SDL_CreateRGBSurface(0, width, height, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);

	width = screen->w * display_scale_factor;
	height = screen->h * display_scale_factor;

	bool is_horizontal = width > height;
	
	TTF_Init();
	atexit(TTF_Quit);
	font_time = TTF_OpenFont(FONT, (is_horizontal ? height : width) / 1.68 );
	font_mode = TTF_OpenFont(FONT, (is_horizontal ? height : width) / 16.5);
	if (!font_time || !font_mode) {
		fprintf(stderr, "TTF_OpenFont: %s\n", TTF_GetError());
		return 1;
	}

	// clear screen
	SDL_FillRect(screen, 0, SDL_MapRGBA(screen->format, 0, 0, 0, 0xff));

	// calculate box coordinates
	int rectsize;
	int spacing;
	int radius;

	if (is_horizontal) {
		rectsize = height * 0.6;
		spacing = width * .031;
		radius =  height * .05714;
	}
	else {
		rectsize = width * 0.6;
		spacing = height * .031;
		radius =  width * .05714;
	}

	int jitter_width  = 1;
	int jitter_height = 1;
	if (display_scale_factor != 1) {
		jitter_width  = (screen->w - width) * 0.5;
		jitter_height = (screen->h - height) * 0.5;
	}

	hourBackground.w = rectsize;
	hourBackground.h = rectsize;
	minBackground.w = rectsize;
	minBackground.h = rectsize;

	if (is_horizontal) {
		hourBackground.x = 0.5 * (width - (0.031 * width) - (1.2 * height))
										+ jitter_width;
		hourBackground.y = 0.2 * height + jitter_height;

		minBackground.x = hourBackground.x + (0.6 * height) + spacing;
		minBackground.y = hourBackground.y;
	}
	else {
		hourBackground.y = 0.5 * (height - (0.031 * height) - (1.2 * width))
										+ jitter_height;
		hourBackground.x = 0.2 * width + jitter_width;

		minBackground.y = hourBackground.y + (0.6 * width) + spacing;
		minBackground.x = hourBackground.x;
	}

	// create background surface
	bgrect.x = 0;
	bgrect.y = 0;
	bgrect.w = rectsize;
	bgrect.h = rectsize;
	bg = SDL_CreateRGBSurface(0, rectsize, rectsize, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
	fill_rounded_box_b(bg, &bgrect, radius, BACKGROUND_COLOR);

	// draw current time
	render_clock(20, 19);

	// main loop
	bool done = false;
	SDL_Event event;
	SDL_TimerID timer = SDL_AddTimer(60, update_time, NULL);

	int mouse_x = -1;
	int mouse_y = -1;	

	while(!done) {
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
				case SDL_USEREVENT:
					render_animation();
					break;
				case SDL_KEYDOWN:
					if(anykeyclose){
						done = true;
						break;
					}
					switch(event.key.keysym.sym) {
						case SDLK_ESCAPE:
						case SDLK_q:
							done = true;
							break;
						default:
							break;
					}
					break;

				case SDL_MOUSEMOTION:
					if ( (mouse_x == -1) || (mouse_y == -1) ) //lifehack
					{
						mouse_x = event.motion.x;
						mouse_y = event.motion.y;

					}

					if(((mouse_x != event.motion.x) || (mouse_y != event.motion.y)) && anykeyclose)
						done = true;
					break;

				case SDL_QUIT:
					done = true;
					break;
			}
		}
		SDL_Delay(100);
	}

	SDL_RemoveTimer(timer);

	SDL_FreeSurface(bg);
	SDL_FreeSurface(screen);

	TTF_CloseFont(font_time);
	TTF_CloseFont(font_mode);

	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	SDL_Quit();

	return 0;
}
