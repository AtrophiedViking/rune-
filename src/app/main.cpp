#include "app/application.h"
#include "core/state.h"
#include "core/config.h"

Config config
{
			.windowTitle = "Rune++",
			.windowResizable = true,
			.windowWidth = 800,
			.windowHeight = 600,
			.swapchainBuffering = SWAPCHAIN_TRIPPLE_BUFFERING,
			.MAX_OBJECTS = 3,
			.backgroundColor = {0.04f,0.015f,0.04f},
			.msaaSamples = VK_SAMPLE_COUNT_1_BIT,
			.DEFAULT_CUBEMAP = "./res/cubemaps/default_cubemap.ktx2",
			.DEFAULT_TEXTURE_PATH = "./res/textures/default_texture.ktx2",
			.MODEL_PATH = "./res/models/DragonAttenuation.glb",
};

int main() {
	State state;
	state.config = new Config;
	state.config = &config;


	init(&state);
	mainloop(&state);
	cleanup(&state);
};