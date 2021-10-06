#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

#include "DrawText.hpp"

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	DrawText text_drawer;

	//----- game state -----

	struct Textline {
		std::string text;
		float x, y;
		Textline(std::string text, float x, float y): text(text), x(x), y(y) {}
	};
	std::vector<Textline> text_lines;

	// game logic
	enum GameState {
		NONE, START, STOP, LEFT, RIGHT
	};
	GameState game_state = NONE;
	int radio_picked = 0;
	float still_time = 10.f;

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// scene parameters
	float light_intensity = 1.f;
	glm::vec3 ambient_light = glm::vec3(.8f);
	const glm::vec3 ambient_light_max = glm::vec3(.8f);
	glm::vec3 sky_light = glm::vec3(1.f);
	const glm::vec3 sky_light_max = glm::vec3(1.f);
	float fog_max_vis_distance = 40.f;

	float radio_distance = 10.f;

	// camera parameters: yaw, pitch
	glm::vec2 cam_params = glm::vec2(0.f, glm::pi<float>()/2);
	// cache yaw quat
	glm::quat cam_yaw = glm::quat(1.f, 0.f, 0.f, 0.f);

	// transforms
	Scene::Transform *world = nullptr;
	Scene::Transform *radio = nullptr;

	// sound handlers
	std::shared_ptr<Sound::PlayingSample> footstep_loop;
	bool footstep_playing = false;
	std::shared_ptr<Sound::PlayingSample> blizzard_loop;
	bool blizzard_supressed = false;

	// music handler
	std::shared_ptr<Sound::PlayingSample> music_loop;
	size_t music_idx = 0;

	
	//camera:
	Scene::Camera *camera = nullptr;

	// helper function get radio location
	glm::vec3 get_radio_location() const;

};
