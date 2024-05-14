#include <memory>
#include <string>

#include "AndroidOut.h"
#include "Model.h"
#include "Utility.h"

#include "Shader.h"

std::unique_ptr<Shader> Shader::loadShader(const std::string &vertexSource, const std::string &fragmentSource) {
	Shader *shader = nullptr;

	GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexSource);
	if (!vertexShader) return nullptr;

	GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentSource);
	if (!fragmentShader) {
		glDeleteShader(vertexShader);
		return nullptr;
	}

	GLuint program = glCreateProgram();
	if (program) {
		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);

		glLinkProgram(program);
		GLint linkStatus = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
		if (linkStatus != GL_TRUE) {
			GLint logLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

			// If we fail to link the shader program, log the result for debugging
			if (logLength) {
				GLchar *log = new GLchar[logLength];
				glGetProgramInfoLog(program, logLength, nullptr, log);
				aout << "Failed to link program with:\n" << log << std::endl;
				delete[] log;
			}

			glDeleteProgram(program);
		} else {
			shader = new Shader(program);
		}
	}

	// The shaders are no longer needed once the program is linked. Release their memory.
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return std::unique_ptr<Shader>(shader);
}

GLuint Shader::loadShader(const GLenum shaderType, const std::string &shaderSource) {
	Utility::assertGlError();
	GLuint shader = glCreateShader(shaderType);
	if (shader) {
		auto *const shaderRawString = reinterpret_cast<const GLchar*>(shaderSource.data());
		GLint shaderLength = shaderSource.length();
		glShaderSource(shader, 1, &shaderRawString, &shaderLength);
		glCompileShader(shader);

		GLint shaderCompiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &shaderCompiled);

		// If the shader doesn't compile, log the result to the terminal for debugging
		if (!shaderCompiled) {
			GLint infoLength = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLength);

			if (infoLength) {
				std::basic_string<GLchar> infoLog(infoLength, 0);
				glGetShaderInfoLog(shader, infoLength, nullptr, infoLog.data());
				aout << "Failed to compile with:\n" << infoLog.data() << std::endl;
			}

			glDeleteShader(shader);
			shader = 0;
		}
	}
	return shader;
}

GLuint Shader::getProgram() const {
	return program;
}

void Shader::activate() const {
	glUseProgram(program);
}

void Shader::deactivate() const {
	glUseProgram(0);
}