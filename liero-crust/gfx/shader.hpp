#ifndef GFX_SHADER_HPP
#define GFX_SHADER_HPP 1

#include "gfx.hpp"
#include <tl/std.h>
#include <tl/string.hpp>
#include <tl/vector.hpp>
#include "GL/glew.h"

namespace gfx {

struct ShaderRef {
	ShaderRef() : id(0) {}

	ShaderRef(GLuint id_init) : id(id_init) {
	}

	GLuint id;
};

struct Shader : protected ShaderRef {
	enum Type : GLenum {
		Vertex = GL_VERTEX_SHADER,
		Fragment = GL_FRAGMENT_SHADER
	};

	Shader() {}

	~Shader() {
		glDeleteShader(this->id); // 0 is ignored
	}

	Shader(Type t, char const* Source);

	Shader(Shader const&) = delete;
	Shader& operator=(Shader const&) = delete;
	
	Shader(Shader&& other) : ShaderRef(other.id) {
		other.id = 0;
	}
	
	Shader& operator=(Shader&& other) {
		glDeleteShader(this->id);
		this->id = other.id;
		other.id = 0;
		return *this;
	}

	GLuint obj_id() const { return this->id; }
};

struct Uniform {
	Uniform() : id(0) {}

	Uniform(GLint id_init) : id(id_init) {}

	void set(GLint v) {
		glUniform1i(id, v);
	}

	void set(GLfloat v) {
		glUniform1f(id, v);
	}

	void set(tl::VectorF2 v) {
		glUniform2f(id, v.x, v.y);
	}

	void set(GLfloat const* mat) {
		glUniformMatrix2fv(this->id, 1, GL_TRUE, mat);
	}

	GLint id;
};

struct ProgramRef {
	ProgramRef() : id(0) {}

	ProgramRef(GLuint id_init) : id(id_init) {
	}

	void attach(Shader const& shader) {
		glAttachShader(this->id, shader.obj_id());
	}

	void detach(Shader const& shader) {
		glDetachShader(this->id, shader.obj_id());
	}

	void link() {
		glLinkProgram(this->id);
		
		GLint programOk;
		glGetProgramiv(this->id, GL_LINK_STATUS, &programOk);
	}

	void validate() {
		glValidateProgram(this->id);
	}

	void use() {
		glUseProgram(this->id);
	}

	Uniform uniform(char const* name) {
		return Uniform(glGetUniformLocation(this->id, name));
	}

	void bind_attrib_location(GLuint index, char const* name) {
		glBindAttribLocation(this->id, index, name);
	}

	GLuint id;
};

struct Program : protected ProgramRef {
	using ProgramRef::attach;
	using ProgramRef::detach;
	using ProgramRef::link;
	using ProgramRef::validate;
	using ProgramRef::use;
	using ProgramRef::uniform;
	using ProgramRef::bind_attrib_location;

	Program() {}

	~Program() {
		glDeleteProgram(this->id); // 0 is ignored
	}

	static Program create();

	Program(Program const&) = delete;
	Program& operator=(Program const&) = delete;
	
	Program(Program&& other) : ProgramRef(other.id) {
		other.id = 0;
	}
	
	Program& operator=(Program&& other) {
		glDeleteProgram(this->id);
		this->id = other.id;
		other.id = 0;
		return *this;
	}
};

}

#endif // GFX_SHADER_HPP
