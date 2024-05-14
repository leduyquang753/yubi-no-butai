#include <string>
#include <utility>

#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include "TestLine.h"

namespace {

const std::string vertexShader = R"vertex(
#version 300 es
uniform mat4 uProjection;
in vec2 inPosition;
out float fragX;

void main() {
	gl_Position = uProjection * vec4(inPosition.x, 0.0, inPosition.y, 1.0);
	fragX = inPosition.x;
}
)vertex";

static const std::string fragmentShader = R"fragment(
#version 300 es
precision highp float;
uniform float uHalfWidth;
uniform vec4 uColor;
in float fragX;
out vec4 outColor;

void main() {
	float pixelSize = abs(dFdx(fragX));
	float halfPixelSize = pixelSize / 2.;
	outColor = vec4(uColor.rgb, uColor.a * clamp(
		(min(uHalfWidth, fragX + halfPixelSize) - max(-uHalfWidth, fragX - halfPixelSize)) / pixelSize, 0., 1.
	));
}
)fragment";

} // namespace

TestLine::TestLine(): shader(std::move(Shader::loadShader(vertexShader, fragmentShader))) {
	const auto shaderProgram = shader->getProgram();
	uProjection = glGetUniformLocation(shaderProgram, "uProjection");
	uHalfWidth = glGetUniformLocation(shaderProgram, "uHalfWidth");
	uColor = glGetUniformLocation(shaderProgram, "uColor");
	inPosition = glGetAttribLocation(shaderProgram, "inPosition");
}

void TestLine::render(const glm::mat4 &matrix, const float width, const float height, const Vector4 color) const {
	const float halfWidth = width / 2;

	shader->activate();

	glUniformMatrix4fv(uProjection, 1, GL_FALSE, glm::value_ptr(matrix));
	glUniform1f(uHalfWidth, halfWidth);
	glUniform4fv(uColor, 1, color.index);

	const float
		extrudeStart = (matrix * glm::vec4(1.f, 0.f, 0.f, 0.f)).x * 540.f,
		extrudeEnd = (matrix * glm::vec4(1.f, 0.f, -height, 0.f)).x * 540.f;

	const float vertices[] = {
		-halfWidth - extrudeStart, 0.f,
		halfWidth + extrudeStart, 0.f,
		halfWidth + extrudeEnd, -height,
		-halfWidth - extrudeEnd, -height
	};
	static const Index indices[] = {0, 1, 2, 2, 3, 0};

	glEnableVertexAttribArray(inPosition);
	glVertexAttribPointer(inPosition, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), vertices);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
	glDisableVertexAttribArray(inPosition);
}