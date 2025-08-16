#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>

#define SIMULATION_DEBUG    0
#define SIMULATION_LENGTH   3000
#define SIMULATION_WIDTH    750
#define SIMULATION_HEIGHT   500
#define SIMULATION_PRERUN   0
#define SAVE_EVERY					100

#define GRID_SIZE           50
#define PARTICLE_COUNT      50000
#define SIMULATION_SPEED    1
#define DISTROBUTION        25
#define MAX_VELOCITY        4       // just guessed here

#define U                   1
#define L                   (SIMULATION_HEIGHT / 6)

#define SQ(_n)              ((_n) * (_n))
#define TX(_p)              (((_p).position_x) - SIMULATION_WIDTH / 2)	// true x
#define TY(_p)              (((_p).position_y) - SIMULATION_HEIGHT / 2)	// true y

// referenced from https://github.com/Inseckto/HSV-to-RGB/blob/master/HSV2RGB.c
void hsv2rgb(float H, float *red, float *green, float *blue)
{
	float s = 1.0f; // full saturation
	float v = 1.0f; // full brightness
	float r, g, b;

	float h = H / 60.0f; // sector 0 to 5
	int i = (int)h;
	float f = h - i;
	float p = v * (1 - s);
	float q = v * (1 - f * s);
	float t = v * (1 - (1 - f) * s);

	switch(i % 6)
	{
		case 0: r = v, g = t, b = p; break;
		case 1: r = q, g = v, b = p; break;
		case 2: r = p, g = v, b = t; break;
		case 3: r = p, g = q, b = v; break;
		case 4: r = t, g = p, b = v; break;
		case 5: r = v, g = p, b = q; break;
	}

	*red   = r * 255;
	*green = g * 255;
	*blue  = b * 255;
}

typedef struct
{
	double original_y;
	double position_x;
	double position_y;
} particle_t;

typedef struct
{
	int tick;

	particle_t particles[PARTICLE_COUNT];

	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Surface *surface;
} state_t;

// getVelocity_x and getVelocity_y were implemented directly
// according to the formula derived in Section 5.3 EXCEPT for
// the fact that if a particle ends up inside the sphere that
// getVelocity_x returns -0.5 to push it out. note that this
// isn't needed in the mathematical formula because since it
// would be physically impossible to penetrate the cylinder
double getVelocity_x(particle_t particle)
{
	return ((SQ(TX(particle)) + SQ(TY(particle))) >= SQ(L))
					? (U + SQ(L) * (SQ(TY(particle)) - SQ(TX(particle))) / SQ(SQ(TX(particle)) + SQ(TY(particle))))
					: -0.5;
}

double getVelocity_y(particle_t particle)
{
	return ((SQ(TX(particle)) + SQ(TY(particle))) >= SQ(L))
					? (-SQ(L) * 2 * TX(particle) * TY(particle) / SQ(SQ(TX(particle)) + SQ(TY(particle))))
					: 0;
}

void tick(state_t *state)
{
	SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, 255);
	SDL_RenderClear(state->renderer);

	SDL_SetRenderDrawColor(state->renderer, 25, 25, 25, 255);
	for(size_t x = 0; SIMULATION_WIDTH / GRID_SIZE > x; x++)
	{
		uint16_t position_x = x * GRID_SIZE;
		SDL_RenderLine(state->renderer, position_x, 0, position_x, SIMULATION_HEIGHT);
	}
	for(size_t y = 0; SIMULATION_HEIGHT / GRID_SIZE > y; y++)
	{
		uint16_t position_y = y * GRID_SIZE;
		SDL_RenderLine(state->renderer, 0, position_y, SIMULATION_WIDTH, position_y);
	}
    
	SDL_SetRenderDrawColor(state->renderer, 0, 0, 255, 255);
	for(size_t i = 0; PARTICLE_COUNT > i; i++)
	{
		double velocity_x = getVelocity_x(state->particles[i]) * SIMULATION_SPEED;
		double velocity_y = getVelocity_y(state->particles[i]) * SIMULATION_SPEED;

		state->particles[i].position_x += velocity_x;
		state->particles[i].position_y += velocity_y;

		if(SIMULATION_WIDTH < state->particles[i].position_x)
		{
			state->particles[i].position_x = -1;
			state->particles[i].position_y = state->particles[i].original_y;
		}
		
		float speedSquare = SQ(velocity_x) + SQ(velocity_y);
		float hue = (int)((1.0f - speedSquare / MAX_VELOCITY) * 360.0f) % 360;
		float r, g, b;
		hsv2rgb(hue, &r, &g, &b);
		SDL_SetRenderDrawColor(state->renderer, r, g, b, 255);

		SDL_FPoint *star = (SDL_FPoint *)malloc(5 * sizeof(SDL_FPoint));
		if(NULL == star)
		{
			printf("Failed to allocate memory for SDL_FPoint\n");
			continue;
		}

		star[0] = (SDL_FPoint){ state->particles[i].position_x - 1, state->particles[i].position_y  };
		star[1] = (SDL_FPoint){ state->particles[i].position_x, state->particles[i].position_y - 1  };
		star[2] = (SDL_FPoint){ state->particles[i].position_x, state->particles[i].position_y      };
		star[3] = (SDL_FPoint){ state->particles[i].position_x + 1, state->particles[i].position_y  };
		star[4] = (SDL_FPoint){ state->particles[i].position_x, state->particles[i].position_y  + 1 };

		SDL_RenderPoints(state->renderer, star, 5);
		free(star);
	}

	if(!SIMULATION_DEBUG) // save computation time in debug
	{
		SDL_DestroySurface(state->surface);
		state->surface = SDL_RenderReadPixels(state->renderer, NULL);
		if(NULL == state->surface)
		{
			printf("Failed to read surface pixels: %s\n", SDL_GetError());
		}
	}
	
	SDL_RenderPresent(state->renderer);
}

int main(int argc, char const *argv[])
{
	srand(0); // seed random

	if(0 == SDL_Init(SDL_INIT_VIDEO))
	{
		printf("Failed to initialize SDL: %s\n", SDL_GetError());
		return 1;
	}

	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
	SDL_SetHint(SDL_HINT_RENDER_LINE_METHOD, "3");

	state_t state;
	if(0 == SDL_CreateWindowAndRenderer("FSim", SIMULATION_WIDTH, SIMULATION_HEIGHT, SDL_WINDOW_BORDERLESS, &state.window, &state.renderer))
	{
		printf("Failed to initialize SDL: %s\n", SDL_GetError());
		return 1;
	}

	for(int i = 0; PARTICLE_COUNT > i; i++)
	{
		double y = rand() % (SIMULATION_HEIGHT + 1);
		state.particles[i] = (particle_t){ y, (int)(-i / DISTROBUTION), y };
	}

	for(size_t i = 0; SIMULATION_PRERUN > i; i++) tick(&state);

	system("mkdir ./out/temp/");
	state.tick = 0;
	while(SIMULATION_LENGTH > state.tick++)
	{
		SDL_Event event;
		while(SDL_PollEvent(&event)){};

		tick(&state);

		if(SIMULATION_DEBUG) continue; // skip if debug

		char buffer[64]; // save frame
		sprintf(buffer, "./out/temp/%05d.bmp", state.tick);
		if(0 == SDL_SaveBMP(state.surface, buffer))
		{
			printf("failed to save frame %d: %s\n", state.tick, SDL_GetError());
		}
	}

	if(!SIMULATION_DEBUG)
	{
		system(
			"ffmpeg -y -framerate 144 -i ./out/temp/%05d.bmp -r 60 "
			"-c:v prores_ks -profile:v 4 -pix_fmt yuv444p10le "
			"-movflags +faststart out/render.mov"
		); // ffmpeg frames to mp4

		for(int i = 0; SIMULATION_LENGTH > i; i += SAVE_EVERY)
		{
			char buffer[64];
			sprintf(buffer, "magick ./out/temp/%05d.bmp ./out/%05d.png", i, i);
			system(buffer);
		}

		system("rm -r ./out/temp/"); // delete frames
	}

  SDL_DestroySurface(state.surface);
	SDL_DestroyWindow(state.window);
	SDL_DestroyRenderer(state.renderer);
	return 0;
}