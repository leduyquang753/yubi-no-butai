#ifndef YUBINOBUTAI_TESTLINE_H
#define YUBINOBUTAI_TESTLINE_H

#include <memory>

#include <GLES3/gl3.h>
#include <glm/glm.hpp>

#include "BasicData.h"
#include "Shader.h"

class TestLine {
	private:
		std::unique_ptr<Shader> shader;
		GLuint uProjection, uHalfWidth, uColor, inPosition;
	public:
		TestLine();
		void render(const glm::mat4 &matrix, float width, float height, Vector4 color) const;
};

#endif // YUBINOBUTAI_TESTLINE_H