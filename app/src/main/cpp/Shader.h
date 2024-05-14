#ifndef YUBINOBUTAI_SHADER_H
#define YUBINOBUTAI_SHADER_H

#include <memory>
#include <string>

#include <GLES3/gl3.h>

class Shader {
	private:
		static GLuint loadShader(GLenum shaderType, const std::string &shaderSource);

		GLuint program;

		constexpr Shader(GLuint program): program(program) {}
	public:
		static std::unique_ptr<Shader> loadShader(const std::string &vertexSource, const std::string &fragmentSource);

		~Shader() {
			if (program != 0) {
				glDeleteProgram(program);
				program = 0;
			}
		}
		GLuint getProgram() const;
		void activate() const;
		void deactivate() const;
};

#endif // YUBINOBUTAI_SHADER_H