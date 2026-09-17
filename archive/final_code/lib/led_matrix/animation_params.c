#include "animation_params.h"

// --- Firework Definitions ---
const firework_params_t FIREWORK_DEFAULT_BLUE = {
    .color_mode = 0,
    .intensity = 255,
    .num_frames = 80,
    .growth_speed = 0.25f,
    .spikes = 9,
    .fade_factor = 220,
    .thinness = 0.12f,
    .core_radius = 1.5f
};

const firework_params_t FIREWORK_DEFAULT_RED = {
    .color_mode = 1,
    .intensity = 255,
    .num_frames = 80,
    .growth_speed = 0.25f,
    .spikes = 9,
    .fade_factor = 220,
    .thinness = 0.12f,
    .core_radius = 1.5f
};

const firework_params_t FIREWORK_DEFAULT_MAGENTA = {
    .color_mode = 2,
    .intensity = 255,
    .num_frames = 80,
    .growth_speed = 0.25f,
    .spikes = 9,
    .fade_factor = 220,
    .thinness = 0.12f,
    .core_radius = 1.5f
};

// --- Spiral Definition ---
const spiral_params_t SPIRAL_DEFAULT_RED = {
    .color_mode = 1,
    .intensity = 120,
    .num_frames = 80,
    .speed = 0.25f,
    .spiral_gap = 0.6f
};

// --- Circle Explosion Definition ---
const circle_explosion_params_t EXPLOSION_DEFAULT_CYAN = {
    .color_mode = 2,
    .intensity = 255,
    .num_frames = 120,
    .growth_speed = 0.20f,
    .ring_thickness = 0.55f
};