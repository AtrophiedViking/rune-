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
			.DEFAULT_TEXTURE_PATH = "./res/textures/default_texture.ktx2",
			.KOBOLD_MODEL_PATH = "./res/models/CompareAlphaCoverage.glb",
			.HOVER_BIKE_MODEL_PATH = "./res/models/DragonAttenuation.glb",
			.MODEL_PATH = "./res/models/MosquitoInAmber.glb",
};

int main() {
	State state;
	state.config = new Config;
	state.config = &config;


	init(&state);
	mainloop(&state);
	cleanup(&state);
};