#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>

#define SIMULATION_LENGTH   2000
#define SIMULATION_WIDTH    750
#define SIMULATION_HEIGHT   500
#define SIMULATION_PRERUN   0

#define GRID_SIZE           50
#define PARTICLE_COUNT      50000
#define SIMULATION_SPEED    1
#define DISTROBUTION        25
#define MAX_VELOCITY        4       // honestly, I justt guessed here,
																		// though there may be a smart way to calculate it

#define U                   1
#define L                   (SIMULATION_HEIGHT / 6)

#define SQ(_n)              ((_n) * (_n))
#define TX(_p)              ((_p.position_x) - SIMULATION_WIDTH / 2)    // true x
#define TY(_p)              ((_p.position_y) - SIMULATION_HEIGHT / 2)   // true y

// referenced from https://github.com/Inseckto/HSV-to-RGB/blob/master/HSV2RGB.c
void hsv2rgb(float H, float *red, float *green, float *blue)
{
	float s = 1.0f;
	float v = 1.0f;
	float r, g, b;

	float h = H / 60.0f;
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

// getVelocity_x and getVelocity_y were implemented
// directly according to the formula derived in Section 5.3
// EXCEPT for the fact that if a particle ends up inside
// the sphere that getVelocity_x returns -0.5 to push it out
// this is due to the use of Euler's methods rather than more
// mature ODE solution methods like RK4 (Runge-Kutta 4 step method)
// this isn't needed in the mathematical formula because this could
// never physically happen.
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
	for(size_t x = 0; x < SIMULATION_WIDTH / GRID_SIZE; x++)
	{
		uint16_t position_x = x * GRID_SIZE;
		SDL_RenderLine(state->renderer, position_x, 0, position_x, SIMULATION_HEIGHT);
	}

	for(size_t y = 0; y < SIMULATION_HEIGHT / GRID_SIZE; y++)
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

		SDL_RenderPoint(state->renderer,	// draw "star" shape
										state->particles[i].position_x - 1, state->particles[i].position_y);
		SDL_RenderPoint(state->renderer,
										state->particles[i].position_x, state->particles[i].position_y - 1);
		SDL_RenderPoint(state->renderer,
										state->particles[i].position_x, state->particles[i].position_y);
		SDL_RenderPoint(state->renderer,
										state->particles[i].position_x, state->particles[i].position_y + 1);
		SDL_RenderPoint(state->renderer,
										state->particles[i].position_x + 1, state->particles[i].position_y);
	}

	SDL_DestroySurface(state->surface);
	state->surface = SDL_RenderReadPixels(state->renderer, NULL);
	if(NULL == state->surface)
	{
		printf("Failed to read surface pixels: %s\n", SDL_GetError());
	}

	SDL_RenderPresent(state->renderer);
}

int main(void)
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
	if(0 == SDL_CreateWindowAndRenderer("FSim", SIMULATION_WIDTH, SIMULATION_HEIGHT, 0, &state.window, &state.renderer))
	{
		printf("Failed to initialize SDL: %s\n", SDL_GetError());
		return 1;
	}

	for(int i = 0; i < PARTICLE_COUNT; i++)
	{
		double y = rand() % (SIMULATION_HEIGHT + 1);
		state.particles[i] = (particle_t){ y, (int)(-i / DISTROBUTION), y };
	}

	for(size_t i = 0; i < SIMULATION_PRERUN; i++) tick(&state);

	state.tick = 0;
	while(state.tick++ < SIMULATION_LENGTH)
	{
		SDL_Event event;
		while(SDL_PollEvent(&event)){};

		tick(&state);

		char buffer[64]; // save frame
		sprintf(buffer, "./out/frames/%05d.bmp", state.tick);
		if(0 == SDL_SaveBMP(state.surface, buffer))
		{
			printf("failed to save frame %d: %s\n", state.tick, SDL_GetError());
		}
	}

	system(
		"ffmpeg -y -framerate 144 -i ./out/frames/%05d.bmp -r 120 "
		"-c:v prores_ks -profile:v 4 -pix_fmt yuv444p10le "
		"-movflags +faststart out/render.mov"
	); // ffmpeg frames to .mov file
	system("rm ./out/frames/*"); // delete frames

	SDL_DestroySurface(state.surface);
	SDL_DestroyWindow(state.window);
	SDL_DestroyRenderer(state.renderer);
	return 0;
}