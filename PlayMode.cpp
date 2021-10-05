#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint hexapod_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > mesh_buf(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("lost-music.pnct"));
	hexapod_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > scene_res(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("lost-music.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = mesh_buf->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = hexapod_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > blizzard_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("blizzard.opus"));
});

Load< Sound::Sample > footstep_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("footstep.opus"));
});

Load< Sound::Sample > music_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("music1.opus"));
});

Load< Sound::Sample > pickup_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("pickup.opus"));
});

Load< Sound::Sample > shiver_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("shivering.opus"));
});

PlayMode::PlayMode() : scene(*scene_res), text_drawer(data_path("ShadowsIntoLight-Regular.ttf").c_str()) {
	// init text drawer
	text_drawer.set_font_size(36);
	text_drawer.set_font_color(0, 0, 0);
	
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "World") world = &transform;
		else if (transform.name == "Radio") radio = &transform;
	}
	if (world == nullptr) throw std::runtime_error("World tarnsform not found.");
	if (radio == nullptr) throw std::runtime_error("Radio tarnsform not found.");

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//start music loop playing:
	// (note: position will be over-ridden in update())
	// leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, get_leg_tip_position(), 10.0f);
	footstep_loop = Sound::loop_3D(*footstep_sample, 0.f, glm::vec3(0.f), 10.f);
	blizzard_loop = Sound::loop_3D(*blizzard_sample, .3f, glm::vec3(0.f), 10.f);
	music_loop = Sound::loop_3D(*music_sample, 1.f, glm::vec3(0.f, 5.f, 0.f), 10.f);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel,
				evt.motion.yrel
			) / float(window_size.y) * camera->fovy;

			// accumulate
			cam_params -= motion;
			cam_params.x = glm::mod(cam_params.x, 2 * glm::pi<float>());
			cam_params.y = glm::clamp(cam_params.y, 0.f, glm::pi<float>());
			// rotate cam
			cam_yaw = glm::angleAxis(cam_params.x, glm::vec3(0.0f, 0.0f, 1.0f));
			camera->transform->rotation = glm::normalize(
				cam_yaw * glm::angleAxis(cam_params.y, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	// move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 5.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) {
			move = glm::normalize(move) * PlayerSpeed * elapsed;
			// play footstep sfx
			if (!footstep_playing) {
				footstep_loop->set_volume(0.4f);
				footstep_playing = true;
			}
		} else {
			//stop playing footstep sfx
			if (footstep_playing) {
				footstep_loop->set_volume(0.f);
				footstep_playing = false;
			}
		}
		
		// calculate yaw for walking direction
		glm::vec3 front = cam_yaw * glm::vec3(0.f, 1.f, 0.f), right = cam_yaw * glm::vec3(1.f, 0.f, 0.f);
		world->position -= move.x * right + move.y * front;

		// update listener
		Sound::listener.set_position_right(glm::vec3(0.f), right, 1.f/60.f);
	}

	// radio logic
	{
		glm::vec3 radio_pos = get_radio_location();
		// regenerate helper function
		auto regenerate_radio = [this]() {
			// regenerate a radio 30m away from player
			float theta = (static_cast<float>(std::rand()) / RAND_MAX) * 2 * glm::pi<float>();
			world->position.x = radio_distance * glm::cos(theta);
			world->position.y = radio_distance * glm::sin(theta);
		};

		// if radio missed, regenerate
		float distance = glm::length(radio_pos);
		if (distance > radio_distance + 5.f) {
			regenerate_radio();
		} else if (distance < .5f) {
			light_intensity = std::max(0.f, light_intensity - .2f);
			fog_max_vis_distance = std::max(10.f, fog_max_vis_distance - 10.f);
			radio_distance = std::min(50.f, radio_distance + 10.f);
			Sound::play(*pickup_sample);
			if (light_intensity < .5f) Sound::play(*shiver_sample, 2.f);
			regenerate_radio();
		}
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;

	// update light
	ambient_light = ambient_light_max * light_intensity;
	sky_light = sky_light_max * light_intensity;

	// //move sound to follow radio position:
	music_loop->set_position(get_radio_location(), 1.0f / 60.0f);
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);
	text_drawer.set_window_size(static_cast<float>(drawable_size.x), static_cast<float>(drawable_size.y));

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(ambient_light));
	glUniform3fv(lit_color_texture_program->FOG_COLOR_vec3, 1, glm::value_ptr(sky_light));
	glUniform1f(lit_color_texture_program->FOG_MAX_VIS_DISTANCE_float, fog_max_vis_distance);
	glUseProgram(0);

	glClearColor(sky_light.r, sky_light.g, sky_light.b, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		text_drawer.draw("Mouse motion rotates camera; WASD moves; escape ungrabs mouse", 1.f, 10.f);
	}
	GL_ERRORS();
}

glm::vec3 PlayMode::get_radio_location() const {
	// no rotation no scaling
	return radio->position + world->position;
}
