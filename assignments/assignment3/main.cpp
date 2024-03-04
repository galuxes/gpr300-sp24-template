#include <stdio.h>
#include <math.h>

#include <ew/external/glad.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <ew/shader.h>
#include <ew/model.h>
#include <ew/camera.h>
#include <ew/transform.h>
#include <ew/cameraController.h>
#include <ew/texture.h>

#include <ew/procGen.h>


void framebufferSizeCallback(GLFWwindow* window, int width, int height);
GLFWwindow* initWindow(const char* title, int width, int height);
void drawUI();

//Global state
int screenWidth = 1080;
int screenHeight = 720;
float prevFrameTime;
float deltaTime;

ew::Camera camera;
ew::Transform monkeyTransform, planeTransform;
ew::CameraController cameraController;

struct Material {
	float Ka = 1.0;
	float Kd = 0.5;
	float Ks = 0.5;
	float Shininess = 128;
}material;

struct Framebuffer {
	unsigned int fbo;
	unsigned int colorBuffer[8];
	unsigned int depthBuffer;
	unsigned int width;
	unsigned int height;
}framebuffer;

Framebuffer gBuffer;

struct ShadowMap {
	unsigned int fbo;
	unsigned int depthBuffer;
	unsigned int width;
	unsigned int height;
}shadowMap;

struct Light {
	glm::vec3 direction = glm::vec3(0,-1,0);
	glm::vec3 color;
}light;

struct Shadow {
	float minBias = .06f;
	float maxBias = .2f;
}shadow;

struct ShadowCamera {

	glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);

	float nearPlane = 0.01f;
	float farPlane = 25.0f;
	float orthoHeight = 5.0f;
	float aspectRatio = 1;

	inline glm::vec3 position()const {
		return target - light.direction * 5.f;
	}

	inline glm::mat4 lightView()const {
		glm::vec3 toTarget = glm::normalize(target - position());
		glm::vec3 up = glm::vec3(0, 1, 0);
		//If camera is aligned with up vector, choose a new one
		if (glm::abs(glm::dot(toTarget, up)) >= 1.0f - glm::epsilon<float>()) {
			up = glm::vec3(0, 0, 1);
		}
		return glm::lookAt(position(), target, up);
	}
	inline glm::mat4 lightProj()const 
	{
		float width = orthoHeight * aspectRatio;
		float r = width / 2;
		float l = -r;
		float t = orthoHeight / 2;
		float b = -t;
		return glm::ortho(l, r, b, t, nearPlane, farPlane);
	}

}shadowCamera;

Framebuffer createFrameBuffer(unsigned int width, unsigned int height, int colorFormat)
{
	framebuffer.width = width;
	framebuffer.height = height;

	//create Framebuffer Object
	glCreateFramebuffers(1, &framebuffer.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);
	//Create Color Buffer
	glGenTextures(1, &framebuffer.colorBuffer[0]);
	glBindTexture(GL_TEXTURE_2D, framebuffer.colorBuffer[0]);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, framebuffer.width, framebuffer.height);
	//Attach color buffer to framebuffer
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, framebuffer.colorBuffer[0], 0);

	glGenTextures(1, &framebuffer.depthBuffer);
	glBindTexture(GL_TEXTURE_2D, framebuffer.depthBuffer);
	//Create depth buffer
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, framebuffer.width, framebuffer.height);
	//Attach to framebuffer
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, framebuffer.depthBuffer, 0);

	GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
		printf("Framebuffer incomplete: %d", fboStatus);
	}

	return framebuffer;
}

ShadowMap createShadowMap(unsigned int width, unsigned int height , unsigned int* dummyVAO)
{
	shadowMap.width = width;
	shadowMap.height = height;

	glCreateFramebuffers(1, &shadowMap.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.fbo);
	glGenTextures(1, &shadowMap.depthBuffer);
	glBindTexture(GL_TEXTURE_2D, shadowMap.depthBuffer);
	//16 bit depth values, 2k resolution 
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, 2048, 2048);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//Pixels outside of frustum should have max distance (white)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap.depthBuffer, 0);

	glCreateVertexArrays(1, dummyVAO);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	return shadowMap;
}

Framebuffer createGBuffer(unsigned int width, unsigned int height) 
{
	Framebuffer framebuffer;

	framebuffer.width = width;
	framebuffer.height = height;

	glCreateFramebuffers( 1, &framebuffer.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);

	int formats[3] = {
		GL_RGB32F, //world Pos
		GL_RGB16F, //world norm
		GL_RGB16F  //albedo
	};

	//create 3 color textures
	for (size_t i = 0; i < 3; i++) 
	{
		glGenTextures( 1, &framebuffer.colorBuffer[i]);
		glBindTexture(GL_TEXTURE_2D, framebuffer.colorBuffer[i]);
		glad_glTextureStorage2D(GL_TEXTURE_2D, 1, formats[i], width, height);

		//prevent wrapping by clamping
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, framebuffer.colorBuffer[i], 0);
	}
	//tell gl what color attatchments we'll draw to
	const GLenum drawBuffers[3] = {
		GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2
	};
	glDrawBuffers(3, drawBuffers);

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return framebuffer;
}


int main() {
	GLFWwindow* window = initWindow("Assignment 2", screenWidth, screenHeight);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	ew::Shader litShader = ew::Shader("assets/lit.vert", "assets/lit.frag");
	ew::Shader blurShader = ew::Shader("assets/blur.vert", "assets/blur.frag");
	ew::Shader depthOnlyShader = ew::Shader("assets/depthOnly.vert", "assets/depthOnly.frag");
	ew::Shader geoShader = ew::Shader("assets/geo.vert", "assets/geo.frag");
	ew::Model monkeyModel = ew::Model("assets/suzanne.obj");
	GLuint rockTexture = ew::loadTexture("assets/Rock037_2K-PNG/Rock037_2K-PNG_Color.png");
	ew::Mesh planeMesh = ew::Mesh(ew::createPlane(10, 10, 5));

	planeTransform.position.y += -1;

	camera.position = glm::vec3(0.0f, 0.0f, 5.0f);
	camera.target = glm::vec3(0.0f, 0.0f, 0.0f); //Look at the center of the scene
	camera.aspectRatio = (float)screenWidth / screenHeight;
	camera.fov = 60.0f; //Vertical field of view, in degrees

	unsigned int dummyVAO;

	
	gBuffer = createGBuffer(screenWidth, screenHeight);
	createFrameBuffer(screenWidth, screenHeight, 0);//idk what color format is yet
	createShadowMap(2048, 2048, &dummyVAO);

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST); //Depth testing

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		//rotate monkey
		//monkeyTransform.rotation = glm::rotate(monkeyTransform.rotation, deltaTime, glm::vec3(0.0, 1.0, 0.0));
		//camera controls
		cameraController.move(window, &camera, deltaTime);

		glm::mat4 lightViewProjection = shadowCamera.lightProj() * shadowCamera.lightView(); //Based on light type, direction

		//geo pass
		glBindFramebuffer(GL_FRAMEBUFFER, gBuffer.fbo);
		glViewport( 0, 0, gBuffer.width, gBuffer.height);
		glClearColor( 0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		geoShader.setMat4("_Model", monkeyTransform.modelMatrix());
		geoShader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());
		geoShader.setMat4("_LightViewProj", lightViewProjection);
		monkeyModel.draw(); //Draws monkey model using current shader
		geoShader.setMat4("_Model", planeTransform.modelMatrix());
		planeMesh.draw();


		//glCullFace(GL_FRONT);//Looks like shit don't dock me points

		glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.fbo);
		glViewport(0, 0, shadowMap.width, shadowMap.height);
		glClear(GL_DEPTH_BUFFER_BIT);

		depthOnlyShader.use();
		//Render scene from light’s point of view
		depthOnlyShader.setMat4("_Model", monkeyTransform.modelMatrix());
		depthOnlyShader.setMat4("_ViewProjection", lightViewProjection);
		monkeyModel.draw(); //Draws monkey model using current shader
		depthOnlyShader.setMat4("_Model", planeTransform.modelMatrix());
		planeMesh.draw();


		glCullFace(GL_BACK);
		
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);
		glViewport(0, 0, screenWidth, screenHeight);

		//RENDER
		glClearColor(0.6f,0.8f,0.92f,1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Bind rock texture to texture unit 0 
		glBindTextureUnit(1, rockTexture);
		glBindTextureUnit(0, shadowMap.depthBuffer);

		litShader.use();

		
		litShader.setFloat("_Material.Ka", material.Ka);
		litShader.setFloat("_Material.Kd", material.Kd);
		litShader.setFloat("_Material.Ks", material.Ks);
		litShader.setFloat("_Material.Shininess", material.Shininess);
		litShader.setVec3("_EyePos", camera.position);
		litShader.setVec3("_LightDirection", light.direction);
		litShader.setInt("_MainTex", 1);
		litShader.setFloat("_minBias", shadow.minBias);
		litShader.setFloat("_maxBias", shadow.maxBias);
		litShader.setInt("_ShadowMap", 0);
		litShader.setMat4("_Model", monkeyTransform.modelMatrix());
		litShader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());
		litShader.setMat4("_LightViewProj", lightViewProjection);
		monkeyModel.draw(); //Draws monkey model using current shader
		litShader.setMat4("_Model", planeTransform.modelMatrix());
		planeMesh.draw();


		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		blurShader.use();

		glBindTextureUnit(0, framebuffer.colorBuffer[0]);
		glBindVertexArray(dummyVAO);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		blurShader.setInt("_MainTex", 0);

		drawUI();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	glDeleteFramebuffers(1, &framebuffer.fbo);

	printf("Shutting down...");
}

void resetCamera(ew::Camera* camera, ew::CameraController* controller) {
	camera->position = glm::vec3(0, 0, 5.0f);
	camera->target = glm::vec3(0);
	controller->yaw = controller->pitch = 0;
}

void drawUI() {
	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Settings");
	if (ImGui::Button("Reset Camera")) {
		resetCamera(&camera, &cameraController);
	}
	if (ImGui::CollapsingHeader("Material")) {
		ImGui::SliderFloat("AmbientK", &material.Ka, 0.0f, 1.0f);
		ImGui::SliderFloat("DiffuseK", &material.Kd, 0.0f, 1.0f);
		ImGui::SliderFloat("SpecularK", &material.Ks, 0.0f, 1.0f);
		ImGui::SliderFloat("Shininess", &material.Shininess, 2.0f, 1024.0f);
	}
	if (ImGui::CollapsingHeader("Light")) {
		ImGui::SliderFloat3("Direction", (float*)&light.direction, -1, 1);
		ImGui::SliderFloat("Min Bias", &shadow.minBias, 0, 0.1);
		ImGui::SliderFloat("Max Bias", &shadow.maxBias, 0, 1);
	}
	//ImGui::Text("Add Controls Here!");
	ImGui::End();

	ImGui::Begin("Shadow Map");
	//Using a Child allow to fill all the space of the window.
	ImGui::BeginChild("Shadow Map");
	//Stretch image to be window size
	ImVec2 windowSize = ImGui::GetWindowSize();
	//Invert 0-1 V to flip vertically for ImGui display
	//shadowMap is the texture2D handle
	ImGui::Image((ImTextureID)shadowMap.depthBuffer, windowSize, ImVec2(0, 1), ImVec2(1, 0));
	ImGui::EndChild();
	ImGui::End();

	ImGui::Begin("GBuffers");
	ImVec2 texSize = ImVec2(gBuffer.width / 4, gBuffer.height / 4);
	for (size_t i = 0; i < 3; i++)
	{
		ImGui::Image((ImTextureID)gBuffer.colorBuffer[i], texSize, ImVec2(0, 1), ImVec2(1, 0));
	}
	ImGui::End();


	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	screenWidth = width;
	screenHeight = height;
	camera.aspectRatio = (float)screenWidth / screenHeight;
	framebuffer.width = width;
	framebuffer.height = height;

}

/// <summary>
/// Initializes GLFW, GLAD, and IMGUI
/// </summary>
/// <param name="title">Window title</param>
/// <param name="width">Window width</param>
/// <param name="height">Window height</param>
/// <returns>Returns window handle on success or null on fail</returns>
GLFWwindow* initWindow(const char* title, int width, int height) {
	printf("Initializing...");
	if (!glfwInit()) {
		printf("GLFW failed to init!");
		return nullptr;
	}

	GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
	if (window == NULL) {
		printf("GLFW failed to create window");
		return nullptr;
	}
	glfwMakeContextCurrent(window);

	if (!gladLoadGL(glfwGetProcAddress)) {
		printf("GLAD Failed to load GL headers");
		return nullptr;
	}

	//Initialize ImGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	return window;
}

