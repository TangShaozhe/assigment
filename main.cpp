#include <iostream>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl2.h>

using namespace glm;

GLFWwindow* window = nullptr;
int err_no;

GLuint load_texture(const char* tex_file)
{
	int img_width, img_height, img_channel;
	auto img_data = stbi_load(tex_file, &img_width, &img_height, &img_channel, 0);
	if (!img_data)
	{
		printf("cannot load texture\n");
		return false;
	}
	GLuint ret = 0;
	glGenTextures(1, &ret);
	glBindTexture(GL_TEXTURE_2D, ret);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_width, img_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glBindTexture(GL_TEXTURE_2D, 0);
	return ret;
}

struct Model
{
	std::vector<vec3> vertices;
	std::vector<vec3> normals;
	std::vector<vec2> uvs;
	std::vector<uint> indices;
	GLuint texture;

	bool load(const char* obj_file, const char* tex_file)
	{
		Assimp::Importer importer;
		auto load_flags =
			aiProcess_RemoveRedundantMaterials |
			aiProcess_FlipUVs;
		auto scene = importer.ReadFile(obj_file, load_flags);
		if (!scene)
		{
			printf("cannot open model: %s\n", importer.GetErrorString());
			return false;
		}

		for (auto i = 0; i < scene->mNumMeshes; i++)
		{
			auto src = scene->mMeshes[i];
			for (auto j = 0; j < src->mNumVertices; j++)
			{
				vertices.push_back(*(vec3*)&src->mVertices[j]);
				normals.push_back(src->mNormals ? *(vec3*)&src->mNormals[j] : vec3(0.f));
				uvs.push_back(src->mTextureCoords[0] ? *(vec2*)&src->mTextureCoords[0][j] : vec2(0.f));
			}
			for (auto j = 0; j < src->mNumFaces; j++)
			{
				indices.push_back(src->mFaces[j].mIndices[0]);
				indices.push_back(src->mFaces[j].mIndices[1]);
				indices.push_back(src->mFaces[j].mIndices[2]);
			}
		}

		texture = load_texture(tex_file);
		if (!texture)
		{
			printf("cannot load texture\n");
			return false;
		}

		return true;
	}

	void draw(const mat4& MV)
	{
		glBindTexture(GL_TEXTURE_2D, texture);
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(&MV[0][0]);
		glBegin(GL_TRIANGLES);
		for (auto i : indices)
		{
			auto& uv = uvs[i];
			glTexCoord2f(uv.x, uv.y);
			auto& nor = normals[i];
			glNormal3f(nor.x, nor.y, nor.z);
			auto& v = vertices[i];
			glVertex3f(v.x, v.y, v.z);
		}
		glEnd();
	}
}cube;

struct Camera
{
	vec3 coord = vec3(0.f, 1.f, 0.f);
	float x_angle = 0.f;
	float y_angle = 0.f;
	vec3 forward_dir;
	vec3 side_dir;
	bool forward = false;
	bool backward = false;
	bool left = false;
	bool right = false;

	mat4 update()
	{
		mat4 rot;
		rot = rotate(mat4(1.f), radians(x_angle), vec3(0.f, 1.f, 0.f));
		rot = rotate(rot,		radians(y_angle), vec3(1.f, 0.f, 0.f));
		forward_dir = -rot[2];
		side_dir = rot[0];

		if (forward)
			coord += forward_dir * 0.1f;
		if (backward)
			coord -= forward_dir * 0.1f;
		if (left)
			coord -= side_dir * 0.1f;
		if (right)
			coord += side_dir * 0.1f;

		return inverse(translate(mat4(1.f), coord) * rot);
	}
}camera;

template<class T, size_t N>
constexpr size_t size(T(&)[N]) { return N; }

static GLFWkeyfun prev_keyfun = nullptr;
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (prev_keyfun)
		prev_keyfun(window, key, scancode, action, mods);
	if (ImGui::GetIO().WantCaptureKeyboard)
		return;
	if (key == GLFW_KEY_W)
	{
		if (action == GLFW_PRESS)
			camera.forward = true;
		else if (action == GLFW_RELEASE)
			camera.forward = false;
	}
	else if (key == GLFW_KEY_S)
	{
		if (action == GLFW_PRESS)
			camera.backward = true;
		else if (action == GLFW_RELEASE)
			camera.backward = false;
	}
	else if (key == GLFW_KEY_A)
	{
		if (action == GLFW_PRESS)
			camera.left = true;
		else if (action == GLFW_RELEASE)
			camera.left = false;
	}
	else if (key == GLFW_KEY_D)
	{
		if (action == GLFW_PRESS)
			camera.right = true;
		else if (action == GLFW_RELEASE)
			camera.right = false;
	}
}

bool dragging = false;
double last_x = 0;
double last_y = 0;

static GLFWmousebuttonfun prev_mousebuttonfun = nullptr;
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (prev_mousebuttonfun)
		prev_mousebuttonfun(window, button, action, mods);
	if (ImGui::GetIO().WantCaptureMouse)
		return;
	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (action == GLFW_PRESS)
		{
			glfwGetCursorPos(window, &last_x, &last_y);
			dragging = true;
		}
		else if (action == GLFW_RELEASE)
			dragging = false;
	}
}

static GLFWcursorposfun prev_cursorposfun = nullptr;
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (prev_cursorposfun)
		prev_cursorposfun(window, xpos, ypos);
	if (ImGui::GetIO().WantCaptureMouse)
		return;
	if (dragging)
	{
		camera.x_angle -= xpos - last_x;
		camera.y_angle -= ypos - last_y;
		last_x = xpos;
		last_y = ypos;
	}
}

const auto GRIDX = 20U;
const auto GRIDY = 40U;
const auto GRIDS = 0.2f;

int main()
{
	if (!glfwInit())
		return 0;

	window = glfwCreateWindow(800, 600, "", nullptr, nullptr);
	if (!window)
		return 0;

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	//if (!cube.load("models/1.obj", "models/1.png"))
	//	return 0;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL2_Init();

	prev_keyfun = glfwSetKeyCallback(window, key_callback);
	prev_mousebuttonfun = glfwSetMouseButtonCallback(window, mouse_button_callback);
	prev_cursorposfun = glfwSetCursorPosCallback(window, cursor_position_callback);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		//ImGui_ImplOpenGL2_NewFrame();
		//ImGui_ImplGlfw_NewFrame();
		//ImGui::NewFrame();

		int win_width;
		int win_height;
		glfwGetWindowSize(window, &win_width, &win_height);
		glViewport(0, 0, win_width, win_height);

		glClearColor(0.7f, 0.7f, 0.7f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glEnable(GL_TEXTURE_2D);
		glEnable(GL_DEPTH);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glEnable(GL_LIGHTING);
		glEnable(GL_NORMALIZE);

		auto proj = perspective(radians(45.f), (float)win_width / (float)win_height, 1.f, 1000.f);
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(&proj[0][0]);
		glMatrixMode(GL_MODELVIEW);
		auto view = camera.update();
		glLoadMatrixf(&view[0][0]);

		glBegin(GL_LINES);
		glColor3f(0.f, 0.f, 0.f);
		auto offset = vec2(GRIDX, GRIDY) * GRIDS * -0.5f;
		for (auto y = 0; y < GRIDY + 1; y++)
		{
			glVertex3f(0.f + offset.x, 0.f, y * GRIDS + offset.y);
			glVertex3f(GRIDX * GRIDS + offset.x, 0.f, y * GRIDS + offset.y);
		}
		for (auto x = 0; x < GRIDX + 1; x++)
		{
			glVertex3f(x * GRIDS + offset.x, 0.f, 0.f + offset.y);
			glVertex3f(x * GRIDS + offset.x, 0.f, GRIDY * GRIDS + offset.y);
		}
		glEnd();

		//ImGui::Render();
		//ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}
