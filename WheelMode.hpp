#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct WheelMode : Mode {
	WheelMode();
	virtual ~WheelMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	bool is_game_over = false;

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// https://15466.courses.cs.cmu.edu/lesson/timing
	void fixed_update();
	int timer = 0;
	static constexpr float Tick = 1.f;
	float timer_acc = 0.f;

	Scene::Transform *wheel = nullptr;
	glm::quat wheel_base_rotation;
	glm::vec3 wheel_base_forward;
	glm::vec3 wheel_base_backward;
	
	//camera:
	Scene::Camera *camera = nullptr;

	//hidden points
	int points_found = 0; // Number of points found
	glm::vec3 hidden_point; // Position of the hidden point
    float distance_to_point = 0.0f; // Distance between player and hidden point
    constexpr static float max_spawn_distance = 20.0f; // Max distance from the player to spawn the point
    void spawn_hidden_point(); 

};
