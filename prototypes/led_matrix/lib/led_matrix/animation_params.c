#include "animation_params.h"

// --- Firework Definitions ---
const firework_params_t FIREWORK_DEFAULT_BLUE = {
    .color_mode = 0,
    .intensity = 50,
    .num_frames = 80,
    .growth_speed = 0.25f,
    .spikes = 9,
    .fade_factor = 220,
    .thinness = 0.12f,
    .core_radius = 1.5f
};

const firework_params_t FIREWORK_DEFAULT_RED = {
    .color_mode = 1,
    .intensity = 50,
    .num_frames = 80,
    .growth_speed = 0.25f,
    .spikes = 9,
    .fade_factor = 220,
    .thinness = 0.12f,
    .core_radius = 1.5f
};

const firework_params_t FIREWORK_DEFAULT_MAGENTA = {
    .color_mode = 2,
    .intensity = 50,
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
    .intensity = 50,
    .num_frames = 120,
    .growth_speed = 0.20f,
    .ring_thickness = 0.55f
};

// --- Blue Shield Definition (NEW) ---
const blue_shield_params_t SHIELD_DEFAULT_CONFIG = {
    .num_frames = 400,   // Duration (~4-5 seconds)
    .brightness = 50    // Max brightness
};

// --- Heal/Heart Definition (NEW) ---
const heal_params_t HEAL_DEFAULT_CONFIG = {
    .num_frames = 250,   // Duration (~3 seconds)
    .brightness = 50    // Max brightness
};