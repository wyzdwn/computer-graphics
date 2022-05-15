
// Hand Example
// Author: Yi Kangrui <yikangrui@pku.edu.cn>

#define _CRT_SECURE_NO_WARNINGS

//#define DIFFUSE_TEXTURE_MAPPING

#include "gl_env.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <vector>
#include <fstream>

#ifndef M_PI
#define M_PI (3.1415926535897932)
#endif

#include <iostream>


#include <glm\gtc\matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Shaders {
	const char* vertex_shader_330 =
		"#version 330 core\n"
		"layout (location = 0) in vec2 Pos;\n"
		"layout (location = 1) in vec2 texCoords;\n"
		"out vec2 TexCoords;\n"
		"out vec4 ParticleColor;\n"
		"uniform mat4  u_mvp;\n"
		"uniform vec4  u_color;\n"
		"uniform vec3  u_pos;\n"
		"uniform float u_scale;\n"
		"void main()\n"
		"{\n"
		"    TexCoords = texCoords;\n"
		"    ParticleColor = u_color;\n"
		"    gl_Position = u_mvp * vec4(u_pos, 1.0) + vec4(Pos.x*u_scale, Pos.y*u_scale, 0.0, 1.0);\n"
		"}\n\0";

	const char* fragment_shader_330 =
		"#version 330 core\n"
		"in vec2 TexCoords;\n"
		"in vec4 ParticleColor;\n"
		"out vec4 FragColor;\n"
		"uniform sampler2D u_texture;\n"
		"void main()\n"
		"{\n"
		"    FragColor = texture(u_texture, TexCoords) * ParticleColor;\n"
		"}\n\0";
}

static void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

/* 相机的属性，包括位置、朝向、上轴、视角大小 */
class myCamera
{
	public:
		glm::vec3 pos;
		glm::vec3 front;
		glm::vec3 up;
		float fov; 
}Camera_now, CameraA, CameraB;// 分别表示：当前相机、A点相机、B点相机

/*用键盘的WASD控制相机的前后左右移动，按下esc可以退出*/
float cameraSpeed = 0; // 设定相机平移的最大速度
float max_pitch_rate = 0.01f; //设定相机旋转的最大速度
float max_yaw_rate = 0.01f;
float lastX = 800.0f / 2.0, lastY = 600.0 / 2.0, pitch = 0, yaw = 0; //lastX，lastY表示鼠标所在的上一个位置的坐标，pitch和yaw是俯仰角和偏航角

float deltaTime = 0.0f; // 当前帧与上一帧的时间差
float lastFrame = 0.0f; // 上一帧的时间
bool transforming = 0; //相机是否正在从A点平滑变化到B点
float transform_start_time = 0.0f; //开始变化的时间

int cot = 0;
glm::vec3 envir_acc = glm::vec3(.0f, .0f, .0f);

/* 初始化 Camera_now, CameraA 和 B */
void init_Camera()
{
	Camera_now.pos = glm::vec3(6.0f, 5.0f, -6.0f);
	Camera_now.front = glm::vec3(-1.0f, -.6f, 1.0f);
	Camera_now.up = glm::vec3(0.0f, 1.0f, 0.0f);
	Camera_now.fov = 45.0f;

	CameraA.pos = glm::vec3(0.0f, 1.0f, 12.0f);
	CameraA.front = glm::vec3(0.0f, -0.1f, -1.0f);
	CameraA.up = glm::vec3(0.0f, 1.0f, 0.0f);
	CameraA.fov = 45.0f;

	CameraB.pos = glm::vec3(12.0f, 3.5f, -12.0f);
	CameraB.front = glm::vec3(-1.0f, -0.2f, 1.0f);
	CameraB.up = glm::vec3(0.0f, 1.0f, 0.0f);
	CameraB.fov = 60.0f;
}

/* 相机从A点连续平滑变化到B点（A，B点即为上面的CameraA和CameraB） */
class Transform_A2B
{
	private:
		glm::vec3 delta_pos;
		float delta_fov;
		glm::vec3 axis; //相机旋转轴
		float angle; //相机朝向需要旋转的角度

	public:
		float totalTime;
		Transform_A2B(myCamera a, myCamera b)
		{
			delta_pos = b.pos - a.pos;
			delta_fov = b.fov - a.fov;

			glm::vec3 start = glm::normalize(a.front);
			glm::vec3 dest = glm::normalize(b.front);

			float cosTheta = glm::dot(start, dest);
			angle = acos(cosTheta);
			totalTime = angle / (M_PI / 6); // 每秒旋转30°

			axis = glm::normalize(glm::cross(start, dest));
		}

		void transform(float current_time, float start_time) //相机变化的主要函数
		{
			float now_time = current_time - start_time;
			Camera_now.pos = CameraA.pos + delta_pos * std::min(now_time / totalTime, 1.0f);
			Camera_now.fov = CameraA.fov+ delta_fov * std::min(now_time / totalTime, 1.0f);
	
			float now_angle = angle * std::min(now_time / totalTime, 1.0f);
			glm::quat rotate = glm::angleAxis(now_angle, axis);
			Camera_now.front = glm::rotate(glm::normalize(rotate), CameraA.front);

			if (now_time >= totalTime)
			{
				transforming = 0;
				delete this;
			}
		}
};

/*	↓用鼠标控制相机旋转↓	*/
bool firstMouse = true; 
void ChangePitch(float degrees) {
	//Check bounds with the max pitch rate so that we aren't moving too fast
	if (degrees < -max_pitch_rate) {
		degrees = -max_pitch_rate;
	}
	else if (degrees > max_pitch_rate) {
		degrees = max_pitch_rate;
	}
	pitch += degrees*2.5f;
}
void ChangeYaw(float degrees) {
	//Check bounds with the max heading rate so that we aren't moving too fast
	if (degrees < -max_yaw_rate) {
		degrees = -max_yaw_rate;
	}
	else if (degrees > max_yaw_rate) {
		degrees = max_yaw_rate;
	}
	yaw += degrees*2.5f;
}
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
	if (transforming) return; //相机当前正在自动从A点变化到B点，则不可以控制相机旋转
	float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = lastX - xpos;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    ChangeYaw(xoffset);
    ChangePitch(yoffset);

	//std::cout << "pitch = "<<pitch << std::endl <<"yaw="<< yaw << std::endl;
    glm::vec3 axis = glm::cross(Camera_now.front, Camera_now.up);
    glm::quat pitch_quat = glm::angleAxis(pitch, axis);
    glm::quat yaw_quat = glm::angleAxis(yaw, Camera_now.up);
    glm::quat tmp = glm::cross(pitch_quat, yaw_quat);
    tmp = glm::normalize(tmp);

    Camera_now.front = glm::rotate(tmp, Camera_now.front);

    //yaw *= .5;
    //pitch *= .5;
	yaw = 0;
	pitch = 0;
}

/* ↓鼠标滚轮控制相机缩放↓ */
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	Camera_now.fov -= (float)yoffset;
	//std::cout << "Camera_now.fov=" << Camera_now.fov << std::endl;
	if (Camera_now.fov < 1.0f)
		Camera_now.fov = 1.0f;
	if (Camera_now.fov > 90.0f)
		Camera_now.fov = 90.0f;
}

/* 按ESC退出，以及记录A点B点 */
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	if (key == GLFW_KEY_1 && action == GLFW_PRESS) //记录当前点为A点
	{
		CameraA = Camera_now;
		printf("A\n");
	}
	if (key == GLFW_KEY_2 && action == GLFW_PRESS) //记录当前点为B点
	{
		CameraB = Camera_now;
		printf("B\n");
	}
}


/************************************************************************************************/
// particle
class Emitter
{
public:
	static glm::vec3 EmitterColor;
	glm::vec3  position;
	glm::vec4 color;
	float r;
	Emitter();
};

class Particle
{
public:
	static glm::vec3 ParticleColor;
	glm::vec3 position, velocity, acceleration;
	glm::vec4 color;
	float life;
	float r;

	Particle() : position(0.0f), velocity(0.0f), acceleration(0.0f), color(1.0f), life(-1.0f) {}
};

glm::vec3 Emitter::EmitterColor = glm::vec3(0.1f, 0.8f, 0.6f);
glm::vec3 Particle::ParticleColor = glm::vec3(0.99f, 0.26f, 0.09f);

class ParticleGenerator
{
public:
	// Constructor
	ParticleGenerator(GLuint emitterAmount, GLuint particleAmount);
	// Update all particles
	void update(GLfloat dt, GLuint newParticles);
	// Count alive particles
	GLuint countAlive();
	// State
	std::vector<Particle> particles;
	std::vector<Emitter> emitters;
	GLuint emitterAmount, particleAmount;
private:
	// Returns the first Particle index that's currently unused e.g. Life <= 0.0f or 0 if no particle is currently inactive
	GLuint firstUnusedParticle();
	// Respawns particle
	void respawnParticle(Particle& particle);
};


Emitter::Emitter()
{
	this->color = glm::vec4(Emitter::EmitterColor, 1.0f);
}

ParticleGenerator::ParticleGenerator(GLuint emitterAmount, GLuint particleAmount)
	: emitterAmount(emitterAmount), particleAmount(particleAmount)
{
	for (GLuint i = 0; i < this->emitterAmount; ++i)
		this->emitters.push_back(Emitter());
	for (GLuint i = 0; i < this->particleAmount; ++i)
		this->particles.push_back(Particle());
}

void ParticleGenerator::update(GLfloat dt, GLuint newParticles)
{
	// update all emitters
	for (GLuint i = 0; i < this->emitterAmount; ++i)
	{
		Emitter& e = this->emitters[i];
	}

	// Add new particles
	for (GLuint i = 0; i < newParticles; ++i)
	{
		int unusedParticle = this->firstUnusedParticle();
		this->respawnParticle(this->particles[unusedParticle]);
	}
	// update all particles
	cot = (cot + 1) % 100;
	if(cot==50)
		envir_acc = glm::vec3((rand() % 100) / 100.f * 6.f - 3.f, 0, (rand() % 100) / 100.f * 6.f - 3.f);
	for (GLuint i = 0; i < this->particleAmount; ++i)
	{
		Particle& p = this->particles[i];
		p.life -= dt; // reduce life
		if (p.life > 0.0f)
		{	// particle is alive, thus update
			p.position += p.velocity * dt;
			p.color.a -= dt * 0.5f;
			p.acceleration = p.velocity* (-1.f)+cot/99.f*envir_acc*log(p.position.y+1);
			float tmp = 0.f;
			if (p.position.y < 1.f) tmp = p.position.y * 1.15f;
			else tmp = p.position.y * 1.f;
			p.acceleration -= glm::vec3(p.position.x, .0f, p.position.z) * (p.position.y > 1.8f ? 1.8f : tmp);
			
			
			p.velocity += p.acceleration * dt;
		}
	}
}

GLuint ParticleGenerator::countAlive()
{
	GLuint count = 0;
	for (GLuint i = 0; i < this->particleAmount; ++i)
	{
		Particle& p = this->particles[i];
		if (p.life > 0.0f)
			count++;
	}
	return count;
}

GLuint lastUsedParticle = 0;
GLuint ParticleGenerator::firstUnusedParticle()
{
	// First search from last used particle, this will usually return almost instantly
	for (GLuint i = lastUsedParticle; i < this->particleAmount; ++i) {
		if (this->particles[i].life <= 0.0f) {
			lastUsedParticle = i;
			return i;
		}
	}
	// Otherwise, do a linear search
	for (GLuint i = 0; i < lastUsedParticle; ++i) {
		if (this->particles[i].life <= 0.0f) {
			lastUsedParticle = i;
			return i;
		}
	}
	// All particles are taken, override the first one (note that if it repeatedly hits this case, more particles should be reserved)
	lastUsedParticle = 0;
	return 0;
}

void ParticleGenerator::respawnParticle(Particle& particle)
{
	Emitter& e = this->emitters[rand() % this->emitterAmount];

	particle.position = e.position;

	
	GLfloat alpha = e.color.a ;
	particle.life =  4.0f;
	particle.color = glm::vec4(Particle::ParticleColor, alpha);
	
	//particle.velocity = glm::vec3((rand() % 100) / 100.0f * 0.0 , (rand() % 100) / 100.0f + 3.f, (rand() % 100) / 100.0f *.0 );
	//particle.velocity = glm::vec3(.0, .0, .0);
	
	particle.velocity = glm::vec3((rand() % 100) / 100.0f - .5f, (rand() % 100) / 100.0f * 4.f+1.f, (rand() % 100) / 100.0f - .5f);
	particle.velocity += particle.position * (1.f - e.r) * 5.f;
	
	particle.acceleration = particle.velocity * (-1.0f);
	
}


int main(int argc, char *argv[])
{
	GLFWwindow* window;
	GLuint vertex_shader, fragment_shader, program;

	glfwSetErrorCallback(error_callback);

	if (!glfwInit())
		exit(EXIT_FAILURE);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	window = glfwCreateWindow(800, 800, "OpenGL output", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	/* 一些设定和初始化 */
	glfwSetKeyCallback(window, key_callback); //键盘按键设定（还有一些键盘按键设定在下面）
	glfwSetCursorPosCallback(window, mouse_callback); //鼠标光标移动设定(用来控制相机朝向，不过得按住鼠标左键才有用）
	glfwSetScrollCallback(window, scroll_callback); //鼠标滚轮设定
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); //让光标在窗口中“消失”
	init_Camera(); //初始化相机，以及A点B点（可以从A点平滑变化到B点，即作业的第二个任务）

	glfwMakeContextCurrent(window);
	glfwSwapInterval(0);

	if (glewInit() != GLEW_OK)
		exit(EXIT_FAILURE);

	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &Shaders::vertex_shader_330, NULL);
	glCompileShader(vertex_shader);

	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &Shaders::fragment_shader_330, NULL);
	glCompileShader(fragment_shader);

	program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	int linkStatus;
	if (glGetProgramiv(program, GL_LINK_STATUS, &linkStatus), linkStatus == GL_FALSE)
		std::cout << "Error occured in glLinkProgram()" << std::endl;

	float passed_time;

	glEnable(GL_DEPTH_TEST);
	Transform_A2B* transform_A2B = nullptr;

	/*tmp*/
	float vertices[] = {
		-0.5f,  0.5f, 0.0f, 1.0f,
		0.5f,  -0.5f, 1.0f, 0.0f,
		-0.5f, -0.5f, 0.0f, 0.0f,

		-0.5f, 0.5f, 0.0f, 1.0f,
		0.5f,  0.5f, 1.0f, 1.0f,
		0.5f, -0.5f, 1.0f, 0.0f
	};
	unsigned int particleVBO, particleVAO;
	glGenVertexArrays(1, &particleVAO);
	glGenBuffers(1, &particleVBO);
	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(particleVAO);

	glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	// texture
	unsigned int particleTexture;
	glGenTextures(1, &particleTexture);
	glBindTexture(GL_TEXTURE_2D, particleTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	stbi_set_flip_vertically_on_load(true);
	int width, height, nrChannels;
	unsigned char* data = stbi_load("ball.png", &width, &height, &nrChannels, 0);

	if (data != NULL)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}
	stbi_image_free(data);
	/*tmp*/

	ParticleGenerator* generator = new ParticleGenerator(25, 60000);


	while (!glfwWindowShouldClose(window))
	{
		passed_time = clock() / double(CLOCKS_PER_SEC);

		// --- You may edit below ---
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		cameraSpeed = 10.f * deltaTime;
		
		generator->update(deltaTime, 500);

		/* 按下T，相机从A点变化到B点 */
		if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS && transforming == 0)
		{
			transforming = 1;
			transform_A2B = new Transform_A2B(CameraA, CameraB);
			transform_start_time = currentFrame;
		}

		if (transforming == 1) transform_A2B->transform(currentFrame,transform_start_time);

		/******** Keyboard Interaction ********/
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) //向前
			Camera_now.pos += cameraSpeed * Camera_now.front;
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) //向后
			Camera_now.pos -= cameraSpeed * Camera_now.front;
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) //向左
			Camera_now.pos -= glm::normalize(glm::cross(Camera_now.front, Camera_now.up)) * cameraSpeed;
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) //向右
			Camera_now.pos += glm::normalize(glm::cross(Camera_now.front, Camera_now.up)) * cameraSpeed;
		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) //上升
			Camera_now.pos += cameraSpeed * Camera_now.up;
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) //下降
			Camera_now.pos -= cameraSpeed * Camera_now.up;
		

		int state_mouse_left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
		int state_P = glfwGetKey(window, GLFW_KEY_P);
		int state_G = glfwGetKey(window, GLFW_KEY_G);
		int state_Y = glfwGetKey(window, GLFW_KEY_Y);
		int state_L = glfwGetKey(window, GLFW_KEY_L);
		int state_space = glfwGetKey(window, GLFW_KEY_SPACE);

		static bool P_flag = 0;
		static bool G_flag = 0;
		static bool Y_flag = 0;
		static bool L_flag = 0;

		// 0:None		1:grasp	action		2:'yes' action		3:'like' action
		static int action_flag = 0;
		
		// * period = 2.4 seconds
		float period = 2.4f;
		static float tmp_time_in_period = 0; 
		float time_in_period = fmod(passed_time, period);

		if (state_P == GLFW_PRESS) P_flag = 1;
		if (state_G == GLFW_PRESS) G_flag = 1;
		if (state_Y == GLFW_PRESS) Y_flag = 1;
		if (state_L == GLFW_PRESS) L_flag = 1;

		// --- Press and hold the space to make the hand still ---
		if (state_space == GLFW_PRESS||tmp_time_in_period!=0)
		{
			if (tmp_time_in_period == 0) tmp_time_in_period = time_in_period;
			time_in_period = tmp_time_in_period;
		}
		if (state_space!=GLFW_PRESS&&abs(fmod(passed_time, period) - tmp_time_in_period) < 0.01)
		{
			tmp_time_in_period = 0;
		}

		// --- set action ---
		if (G_flag && abs(time_in_period - 1.2) < 0.01)
		{	
			G_flag = 0;
			action_flag = 1;
		}
		if (Y_flag && abs(time_in_period - 1.2) < 0.01)
		{
			Y_flag = 0;
			action_flag = 2;
		}
		if (L_flag && abs(time_in_period - 1.2) < 0.01)
		{
			L_flag = 0;
			action_flag = 3;
		}

		// --- stop all action ---
		if (P_flag && abs(time_in_period - 1.2) < 0.01)
		{
			P_flag = 0;
			action_flag = 0;
		}

		

		// --- You may edit above ---

		float ratio;
		int width, height;

		glfwGetFramebufferSize(window, &width, &height);
		ratio = width / (float)height;

		glClearColor(0.1, 0.1, 0.1, 1.0);

		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(program);
		glm::mat4 view = glm::lookAt(Camera_now.pos, Camera_now.pos + Camera_now.front, Camera_now.up);
		glm::fmat4 mvp = glm::perspective(glm::radians(Camera_now.fov), ratio, 0.0f, 100.0f) * view;
		glUniformMatrix4fv(glGetUniformLocation(program, "u_mvp"), 1, GL_FALSE, (const GLfloat*)&mvp);

		/*tmp*/
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE); //设置值（411行恢复）

		for (int i = 0; i < generator->emitterAmount; i++) //生成发射器
		{
			Emitter& e = generator->emitters[i];
			float x, z;
			if (i < 9) x = cos(2 * (i % 9) * M_PI / 9), z = sin(2 * (i % 9) * M_PI / 9), e.r = .2f;
			else x = cos(2 * (i % 16) * M_PI / 16), z = sin(2 * (i % 16) * M_PI / 16), e.r = .4f;
			
			e.position = glm::vec3(x*e.r,0.0f,z*e.r);
			glUniform4fv(glGetUniformLocation(program, "u_color"), 1, (const GLfloat*)&e.color);
			glUniform3fv(glGetUniformLocation(program, "u_pos"), 1, (const GLfloat*)&e.position);
			glUniform1f(glGetUniformLocation(program, "u_scale"), 0.2f);
			glBindTexture(GL_TEXTURE_2D, particleTexture);
			glBindVertexArray(particleVAO);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glBindVertexArray(0);
		}

		
		for (int i = 0; i < generator->particleAmount; i++) //生成粒子
		{
			Particle& p = generator->particles[i];
			if (p.life > 0.0f)
			{
				glUniform4fv(glGetUniformLocation(program, "u_color"), 1, (const GLfloat*)&p.color);
				glUniform3fv(glGetUniformLocation(program, "u_pos"), 1, (const GLfloat*)&p.position);
				glUniform1f(glGetUniformLocation(program, "u_scale"), 0.1f);
				glBindTexture(GL_TEXTURE_2D, particleTexture);
				glBindVertexArray(particleVAO);
				glDrawArrays(GL_TRIANGLES, 0, 6);
				glBindVertexArray(0);
			}
		}

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //恢复
		std::cout << "Alive Particles: " << generator->countAlive() << "\n";
		/*tmp*/

		glfwSwapBuffers(window);
		glfwPollEvents();
	}


	glfwDestroyWindow(window);

	glfwTerminate();
	exit(EXIT_SUCCESS);
}