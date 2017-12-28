#include <windows.h>
#include <iostream>
#include <vector>
#include <GL\glew.h>
#include <gl\freeglut.h>
#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <GL\glaux.h>

#define FOURCC_DXT1 0x31545844 // Equivalent to "DXT1" in ASCII
#define FOURCC_DXT3 0x33545844 // Equivalent to "DXT3" in ASCII
#define FOURCC_DXT5 0x35545844 // Equivalent to "DXT5" in ASCII

using namespace glm;

GLuint Program;

GLuint MatrixID;
GLuint ViewMatrixID;
GLuint ModelMatrixID;
GLuint TextureID;
GLuint LightID;

GLuint vertexbuffer;
GLuint uvbuffer;
GLuint normalbuffer;

GLuint Texture;

std::vector<glm::vec3> vertices;
std::vector<glm::vec2> uvs;
std::vector<glm::vec3> normals;

glm::mat4 MVP;
glm::mat4 ModelMatrix;
glm::mat4 ViewMatrix;
glm::mat4 ProjectionMatrix;

AUX_RGBImageRec *image;
AUX_RGBImageRec *image_win;
unsigned int image_tex;
unsigned int image_win_tex;


//! Вершина
struct vertex
{
	GLfloat x;
	GLfloat y;
	GLfloat z;
};

bool loadOBJ(const char * path, std::vector<glm::vec3> & out_vertices, 
	std::vector<glm::vec2> & out_uvs, std::vector<glm::vec3> & out_normals) {
	std::cout << "Loading OBJ file..." << std::endl;

	std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
	std::vector<glm::vec3> temp_vertices;
	std::vector<glm::vec2> temp_uvs;
	std::vector<glm::vec3> temp_normals;


	FILE * file = fopen(path, "r");
	if (file == NULL) {
		std::cout << "Impossible to open the file." << std::endl;
		getchar();
		return false;
	}

	while (true) {
		char lineHeader[128];
		// read the first word of the line
		int res = fscanf(file, "%s", lineHeader);
		if (res == EOF)
			break;

		if (strcmp(lineHeader, "v") == 0) {
			glm::vec3 vertex;
			fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
			temp_vertices.push_back(vertex);
		}
		else if (strcmp(lineHeader, "vt") == 0) {
			glm::vec2 uv;
			fscanf(file, "%f %f\n", &uv.x, &uv.y);
			temp_uvs.push_back(uv);
		}
		else if (strcmp(lineHeader, "vn") == 0) {
			glm::vec3 normal;
			fscanf(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
			temp_normals.push_back(normal);
		}
		else if (strcmp(lineHeader, "f") == 0) {
			std::string vertex1, vertex2, vertex3;
			unsigned int vertexIndex[3], uvIndex[3], normalIndex[3];
			int matches = fscanf(file, "%d/%d/%d %d/%d/%d %d/%d/%d\n", &vertexIndex[0], &uvIndex[0], &normalIndex[0], &vertexIndex[1], &uvIndex[1], &normalIndex[1], &vertexIndex[2], &uvIndex[2], &normalIndex[2]);
			vertexIndices.push_back(vertexIndex[0]);
			vertexIndices.push_back(vertexIndex[1]);
			vertexIndices.push_back(vertexIndex[2]);
			uvIndices.push_back(uvIndex[0]);
			uvIndices.push_back(uvIndex[1]);
			uvIndices.push_back(uvIndex[2]);
			normalIndices.push_back(normalIndex[0]);
			normalIndices.push_back(normalIndex[1]);
			normalIndices.push_back(normalIndex[2]);
		}
		else {
			char stupidBuffer[1000];
			fgets(stupidBuffer, 1000, file);
		}

	}

	// For each vertex of each triangle
	for (unsigned int i = 0; i<vertexIndices.size(); i++) {

		// Get the indices of its attributes
		unsigned int vertexIndex = vertexIndices[i];
		unsigned int uvIndex = uvIndices[i];
		unsigned int normalIndex = normalIndices[i];

		// Get the attributes thanks to the index
		glm::vec3 vertex = temp_vertices[vertexIndex - 1];
		glm::vec2 uv = temp_uvs[uvIndex - 1];
		glm::vec3 normal = temp_normals[normalIndex - 1];

		// Put the attributes in buffers
		out_vertices.push_back(vertex);
		out_uvs.push_back(uv);
		out_normals.push_back(normal);

	}
	fclose(file);
	return true;
}

void initGL() {
	glClearColor(0.1, 0.1, 0.1, 0);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
}

//! Функция печати лога шейдера
void shaderLog(unsigned int shader)
{
	int   infologLen = 0;
	int   charsWritten = 0;
	char *infoLog;

	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infologLen);

	if (infologLen > 1)
	{
		infoLog = new char[infologLen];
		if (infoLog == NULL)
		{
			std::cout << "ERROR: Could not allocate InfoLog buffer\n";
			exit(1);
		}
		glGetShaderInfoLog(shader, infologLen, &charsWritten, infoLog);
		std::cout << "InfoLog: " << infoLog << "\n\n\n";
		delete[] infoLog;
	}
}

//! Проверка ошибок OpenGL, если есть то вывод в консоль тип ошибки
void checkOpenGLerror()
{
	setlocale(LC_ALL, "Russian");
	GLenum errCode;
	if ((errCode = glGetError()) != GL_NO_ERROR)
		std::cout << "OpenGl error! - " << gluErrorString(errCode);
}

//! Инициализация шейдеров
void initShader()
{
	//! Исходный код шейдеров
	const GLchar* vsSource =
		"layout(location = 0) in vec3 position;\n"
		"layout(location = 1) in vec2 texcoord;\n"
		"layout(location = 2) in vec3 normal;\n"

		"out vec2 UV;\n" //Текстурные координаты
		"out float distance;\n" //Расстояние от точечного источника освещения до вершины
		"out vec3 Normal_cameraspace;\n" //Нормаль к поверхности объекта в вершине
		"out vec3 EyeDirection_cameraspace;\n" //Направление взгляда, от вершины к наблюдателю
		"out vec3 LightDirection_cameraspace;\n" //Направление падающего света, от вершины к источнику освещения

		"uniform mat4 MVP;\n"
		"uniform mat4 V;\n"
		"uniform mat4 M;\n"
		"uniform vec3 LightPosition_worldspace;\n"

		"void main() {\n"
		" gl_Position = MVP * vec4(position, 1);\n"
		
		" vec3 Position_worldspace = (M * vec4(position, 1)).xyz;\n"
		" vec3 vertexPosition_cameraspace = (V * M * vec4(position, 1)).xyz;\n"
		
		" EyeDirection_cameraspace = vec3(0, 0, 0) - vertexPosition_cameraspace;\n"
		" vec3 LightPosition_cameraspace = (V * vec4(LightPosition_worldspace, 1)).xyz;\n"
		
		" distance = length(LightPosition_worldspace - Position_worldspace);\n"

		" LightDirection_cameraspace = LightPosition_cameraspace + EyeDirection_cameraspace;\n"
		" Normal_cameraspace = (V * M * vec4(normal, 0)).xyz;\n"
		
		" UV = texcoord;\n"
		"}\n";

	const char* fsSource =
		"in vec2 UV;\n"
		"in float distance;\n"
		"in vec3 Normal_cameraspace;\n"
		"in vec3 EyeDirection_cameraspace;\n"
		"in vec3 LightDirection_cameraspace;\n"

		"out vec3 color;\n"

		"uniform sampler2D myTextureSampler;\n"
		"uniform mat4 MV;\n"
		"uniform vec3 LightPosition_worldspace;\n"

		"void main() {\n"
		"vec3 LightColor = vec3(1.0, 1.0, 1.0);\n"
		"float LightPower = 40.0f;\n"

		"vec3 MaterialDiffuseColor = texture(myTextureSampler, UV).rgb;\n"
		"vec3 MaterialAmbientColor = vec3(0.3, 0.3, 0.3) * MaterialDiffuseColor;\n"
		"vec3 MaterialSpecularColor = vec3(0.1, 0.1, 0.1);\n"

		"vec3 n = normalize(Normal_cameraspace);\n"
		"vec3 l = normalize(LightDirection_cameraspace);\n"
		"float cosTheta = clamp(dot(n, l), 0, 1);\n"

		"vec3 E = normalize(EyeDirection_cameraspace);\n"
		"vec3 R = reflect(-l, n);\n"
		"float cosAlpha = clamp(dot(E, R), 0, 1);\n"

		"color = \n"
		"MaterialAmbientColor +\n"
		"MaterialDiffuseColor * LightColor * LightPower * cosTheta / (distance*distance) +\n"
		"MaterialSpecularColor * LightColor * LightPower * pow(cosAlpha, 2) / (distance*distance);\n"
		
		"}\n";

	//! Переменные для хранения идентификаторов шейдеров
	GLuint vShader, fShader;

	//! Создаем вершинный шейдер
	vShader = glCreateShader(GL_VERTEX_SHADER);
	//! Передаем исходный код
	glShaderSource(vShader, 1, &vsSource, NULL);
	//! Компилируем шейдер
	glCompileShader(vShader);

	std::cout << "vertex shader \n";
	shaderLog(vShader);

	//! Создаем фрагментный шейдер
	fShader = glCreateShader(GL_FRAGMENT_SHADER);
	//! Передаем исходный код
	glShaderSource(fShader, 1, &fsSource, NULL);
	//! Компилируем шейдер
	glCompileShader(fShader);

	std::cout << "fragment shader \n";
	shaderLog(fShader);

	//! Создаем программу и прикрепляем шейдеры к ней
	Program = glCreateProgram();
	glAttachShader(Program, vShader);
	glAttachShader(Program, fShader);

	//! Линкуем шейдерную программу
	glLinkProgram(Program);

	//! Проверяем статус сборки
	int link_ok;
	glGetProgramiv(Program, GL_LINK_STATUS, &link_ok);
	if (!link_ok)
	{
		std::cout << "error attach shaders \n";
		return;
	}

	MatrixID = glGetUniformLocation(Program, "MVP");
	ViewMatrixID = glGetUniformLocation(Program, "V");
	ModelMatrixID = glGetUniformLocation(Program, "M");

	TextureID = glGetUniformLocation(Program, "myTextureSampler");

	LightID = glGetUniformLocation(Program, "LightPosition_worldspace");

	checkOpenGLerror();
}

//! Инициализация VBO
void initVBO()
{
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);

	glGenBuffers(1, &uvbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], GL_STATIC_DRAW);

	glGenBuffers(1, &normalbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals[0], GL_STATIC_DRAW);

	checkOpenGLerror();
}

//! Освобождение шейдеров
void freeShader()
{
	//! Передавая ноль, мы отключаем шейдрную программу
	glUseProgram(0);
	//! Удаляем шейдерную программу
	glDeleteProgram(Program);
}

//! Освобождение буфера
void freeVBO()
{
	glDeleteBuffers(1, &vertexbuffer);
	glDeleteBuffers(1, &uvbuffer);
	glDeleteBuffers(1, &normalbuffer);
}

void resizeWindow(int width, int height)
{
	glViewport(0, 0, width, height);
	height = height > 0 ? height : 1;
	const GLfloat aspectRatio = (GLfloat)width / (GLfloat)height;

	glm::vec3 position = glm::vec3(0, 0, 3);
	glm::vec3 direction(cos(0.0f) * sin(3.14f), 0, -1);
	glm::vec3 right = glm::vec3(0, 0, 1);
	glm::vec3 up = glm::cross(right, direction);

	ProjectionMatrix = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 200.0f);
	ViewMatrix = glm::lookAt(position, position + direction, up);
	ModelMatrix = glm::mat4(1.0);
	MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
}

//! Отрисовка
void render()
{
	// Clear the screen
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Use our shader
	glUseProgram(Program);

	// Send our transformation to the currently bound shader, 
	// in the "MVP" uniform
	glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
	glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
	glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);

	glm::vec3 lightPos = glm::vec3(-2, 4, 5);
	glUniform3f(LightID, lightPos.x, lightPos.y, lightPos.z);

	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &image_tex);
	glBindTexture(GL_TEXTURE_2D, image_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image->sizeX, image->sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, image->data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glUniform1i(TextureID, 0);

	// 1rst attribute buffer : vertices
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

	// 2nd attribute buffer : UVs
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

	// 3rd attribute buffer : normals
	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

	// Draw the triangle !
	//glDrawElements(GL_TRIANGLES, vertices.size(), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, vertices.size());

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);

	checkOpenGLerror();
	glutSwapBuffers();
}

void specialKeys(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_UP:
		MVP = glm::rotate(MVP, 0.1f, glm::vec3(1.0f, 0.0f, 0.0f));
		break;
	case GLUT_KEY_DOWN:
		MVP = glm::rotate(MVP, -0.1f, glm::vec3(1.0f, 0.0f, 0.0f));
		break;
	case GLUT_KEY_RIGHT:
		MVP = glm::rotate(MVP, 0.1f, glm::vec3(0.0f, 1.0f, 0.0f));
		break;
	case GLUT_KEY_LEFT:
		MVP = glm::rotate(MVP, -0.1f, glm::vec3(0.0f, 1.0f, 0.0f));
		break;
	case GLUT_KEY_PAGE_UP:
		MVP = glm::rotate(MVP, 0.1f, glm::vec3(0.0f, 0.0f, 1.0f));
		break;
	case GLUT_KEY_PAGE_DOWN:
		MVP = glm::rotate(MVP, -0.1f, glm::vec3(0.0f, 0.0f, 1.0f));
		break;
	}

	glutPostRedisplay();
}

int main(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_ALPHA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(600, 600);
	glutCreateWindow("Simple shaders");

	//! Обязательно перед инициализации шейдеров
	GLenum glew_status = glewInit();
	if (GLEW_OK != glew_status)
	{
		//! GLEW не проинициализировалась
		std::cout << "Error: " << glewGetErrorString(glew_status) << "\n";
		return 1;
	}

	//! Проверяем доступность OpenGL 2.0
	if (!GLEW_VERSION_2_0)
	{
		//! OpenGl 2.0 оказалась не доступна
		std::cout << "No support for OpenGL 2.0 found\n";
		return 1;
	}

	//! Инициализация
	initGL();
	image = auxDIBImageLoad("african_head_SSS.bmp");
	bool res = loadOBJ("african_head.obj", vertices, uvs, normals);
	initVBO();
	initShader();

	glutReshapeFunc(resizeWindow);
	glutDisplayFunc(render);
	glutSpecialFunc(specialKeys);
	glutMainLoop();

	freeVBO();
}