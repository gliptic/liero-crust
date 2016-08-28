#include "shader.hpp"
#include "window.hpp"

namespace gfx {

void show_info_log(GLuint obj_id, PFNGLGETSHADERIVPROC glGet_iv, PFNGLGETSHADERINFOLOGPROC glGet_InfoLog) {
	GLint log_len;
	glGet_iv(obj_id, GL_INFO_LOG_LENGTH, &log_len);

	tl::Vec<char> buf;
	buf.reserve(usize(log_len) + 1);
	glGet_InfoLog(obj_id, log_len, NULL, buf.begin());
	buf.begin()[log_len] = 0;
	printf("%s\n", buf.begin());
}

Shader::Shader(Shader::Type t, char const* source) {

	this->id = glCreateShader(t);

	char const* source_ptr = source;
	GLint length = GLint(strlen(source));
	glShaderSource(this->id, 1, reinterpret_cast<GLchar const**>(&source_ptr), &length);
	glCompileShader(this->id);

	GLint compile_ok = 0;
	glGetShaderiv(this->id, GL_COMPILE_STATUS, &compile_ok);

	if (!compile_ok) {
		show_info_log(this->id, glGetShaderiv, glGetShaderInfoLog);
	}
}

Program Program::create() {
	Program p;
	p.id = glCreateProgram();
	return std::move(p);
}

}
