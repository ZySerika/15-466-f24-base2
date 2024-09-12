#include "WheelMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>


// from base code
GLuint wheel_mesh_for_lit_color_texture_program = 0;
Load< MeshBuffer > wheel_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("wheel.pnct"));
	wheel_mesh_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load < Scene > wheel_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("wheel.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = wheel_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = wheel_mesh_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
	});
});

void WheelMode::spawn_hidden_point() {
	// Generate a random point in the world. Code from ChatGPT query on random generator.
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(-max_spawn_distance, max_spawn_distance);
	hidden_point = glm::vec3(dis(gen), dis(gen), 0.0f);
	distance_to_point = glm::distance(hidden_point, wheel->position);
}


WheelMode::WheelMode() : scene(*wheel_scene) {
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Cylinder") wheel = &transform;
	}
	if (wheel == nullptr) throw std::runtime_error("Wheel not found.");

	wheel_base_rotation = wheel->rotation;
	wheel_base_forward = wheel->rotation * glm::vec3(0.0f, 1.0f, 0.0f);
	wheel_base_backward = wheel->rotation * glm::vec3(0.0f, -1.0f, 0.0f);

	timer = 30;
	timer_acc = 0;

	spawn_hidden_point();

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
}

WheelMode::~WheelMode() {
}

bool WheelMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_a) {
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
	} 

	return false;
}

void WheelMode::fixed_update() {
	// constant update per second
	if(is_game_over) return;
	timer -= 1;
	if(timer == 0) {
		is_game_over = true;
	}
}

void WheelMode::update(float elapsed) {

	// https://15466.courses.cs.cmu.edu/lesson/timing
	// constant update
	timer_acc += elapsed;
	while (timer_acc > Tick) {
		fixed_update();
		timer_acc -= Tick;
	}



	constexpr float WheelSpeed = 10.0f; // translation speed
	constexpr float WheelRotationSpeed = 90.0f; // rotation speed, degrees per second
	constexpr float WheelForwardRotSpeed = 150.0f;
	distance_to_point = glm::distance(wheel->position, hidden_point);

	// Direction control
	glm::vec3 move = glm::vec3(0.0f);
	float rotation_angle = 0.0f;
	glm::vec3 camera_move = glm::vec3(0.0f);

	if(!is_game_over) {


	// Rotate left (A key)
	if (left.pressed) {
		rotation_angle += WheelRotationSpeed * elapsed;
	}
	// Rotate right (D key)
	if (right.pressed) {
		rotation_angle -= WheelRotationSpeed * elapsed;
	}

	// Apply rotation around the **global X-axis**
    if (rotation_angle != 0.0f) {
        // Rotate around the world X-axis (glm::vec3(1.0f, 0.0f, 0.0f)) in world space
        glm::quat world_x_rotation = glm::angleAxis(glm::radians(rotation_angle), glm::vec3(0.0f, 0.0f, 1.0f));
        wheel->rotation = world_x_rotation * wheel->rotation; // Pre-multiply to rotate in world space
		wheel_base_forward = world_x_rotation * wheel_base_forward;
		wheel_base_backward = world_x_rotation * wheel_base_backward;

    }

	if (up.pressed) {
		// Move forward in the direction of the wheel's forward axis
		move += wheel_base_forward * WheelSpeed * elapsed;
		wheel->rotation = glm::rotate(wheel->rotation, glm::radians(WheelForwardRotSpeed * elapsed), glm::vec3(0.0f, 0.0f, -1.0f));
		camera_move = wheel_base_forward * WheelSpeed * elapsed;
	}
	if (down.pressed) {
		// Move backward in the opposite direction of the wheel's forward axis
		move += wheel_base_backward * WheelSpeed * elapsed;
		wheel->rotation = glm::rotate(wheel->rotation, glm::radians(-WheelForwardRotSpeed * elapsed), glm::vec3(0.0f, 0.0f, -1.0f));
		camera_move = wheel_base_backward * WheelSpeed * elapsed;
	}

	}


	// Apply movement (translation)
	wheel->position += move;
	if (camera_move != glm::vec3(0.0f)) {
        camera->transform->position += camera_move;
    }

	// increment points by checking if distance is close enough
	if(glm::distance(wheel->position, hidden_point) < 3.0f) {
		points_found++;
		spawn_hidden_point();
	}


	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void WheelMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));
		// Base code is used for UI
		constexpr float H = 0.1f;
		lines.draw_text("Find the hidden point with WSAD! Distance to target: " + std::to_string(int(distance_to_point)),
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		// Draw additional text in the top right
		lines.draw_text("Points found: " + std::to_string(points_found),
			glm::vec3(aspect - 1.f, 0.9f - 0.2f * H, 0.0f), // Position near top-right
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),  // Text size and direction
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		
		lines.draw_text("Time left: " + std::to_string(timer),
		glm::vec3(aspect - 1.f, 0.75f - 0.2f * H, 0.0f), // Position near top-right
		glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),  // Text size and direction
		glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		if(is_game_over) {
			lines.draw_text("Game Over! Score: " + std::to_string(points_found),
			glm::vec3(0.5, 0.5, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
		
	}
}
