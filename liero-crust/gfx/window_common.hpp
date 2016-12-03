#include "window.hpp"
#include "buttons_common.hpp"

namespace gfx {

static void init_extensions() {
	static int glewInitialized = 0;
	if(!glewInitialized) {
		glewInit();
		glewInitialized = 1;
	}
}

void DefaultProgram::init(Shader vs_init, Shader fs_init, u32 width, u32 height) {
	this->vs = std::move(vs_init);
	this->fs = std::move(fs_init);

	this->Program::operator=(Program::create());
	this->attach(this->vs);
	this->attach(this->fs);

	this->bind_attrib_location(AttribPosition, "position");
	this->bind_attrib_location(AttribColor, "color");
	this->bind_attrib_location(AttribTexCoord, "texcoord");

	this->link();

	this->transform_uni = this->uniform("transform");
	this->translation_uni = this->uniform("translation");
	this->texture = this->uniform("texture");

	GLfloat tr[4] = {
		2.f / width, 0.f,
		0.f, -2.f / height
	};

	this->use();
	this->transform_uni.set(tr);
	this->translation_uni.set(tl::VectorF2(-1.f, 1.f));
	this->texture.set(0);
}

static int common_setup_gl(CommonWindow* self) {
	// OpenGL stuff

	init_extensions();

#if BONK_USE_GL2
	if(!GL_VERSION_2_0)
		return -1; // TODO: OpenGL 2.0 required
#endif

	glViewport(0, 0, self->width, self->height);

	//glClearColor(0.15f, 0.15f, 0.4f, 0.0f);
	glClearColor(0.f, 0.f, 0.f, 0.0f);
	glClearDepth(0.0f);
	
#if !BONK_USE_GL2
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);
#endif
	glDisable(GL_DEPTH_TEST);

	//glEnable(GL_MULTISAMPLE_ARB);
	//glHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_NICEST);

#if BONK_USE_GL2
	static char const defaultVsSrc[] = {

R"=(#version 110
uniform mat2 transform;
uniform vec2 translation;
attribute vec2 position;
attribute vec2 texcoord;
attribute vec4 color;

varying vec2 fragTexcoord;
varying vec4 fragColor;

void main() {
	gl_Position = vec4((transform * position) + translation, 0.0, 1.0);
	fragTexcoord = texcoord;
	fragColor = color;
})="
	};

	static char const defaultFsSrc[] = {
R"=(#version 110

uniform sampler2D texture;
varying vec2 fragTexcoord;
varying vec4 fragColor;

void main() {
	vec4 m = texture2D(texture, fragTexcoord);

	gl_FragColor = vec4((m * fragColor).rgb, 1);
})="
	};

	// vec4 m = texture2D(texture, fragTexcoord);

	/*
	vec4 a = texture2D(texture, fragTexcoord + vec2(504.0 / 512.0 / 1024.0 / 4.0, 350.0 / 512.0 / 768.0 / 2.0));
	vec4 b = texture2D(texture, fragTexcoord + vec2(-504.0 / 512.0 / 1024.0 / 2.0, 350.0 / 512.0 / 768.0 / 4.0));
	vec4 c = texture2D(texture, fragTexcoord + vec2(-504.0 / 512.0 / 1024.0 / 4.0, -350.0 / 512.0 / 768.0 / 2.0));
	vec4 d = texture2D(texture, fragTexcoord + vec2(504.0 / 512.0 / 1024.0 / 2.0, -350.0 / 512.0 / 768.0 / 4.0));
	vec4 m = mix(mix(a, b, 0.5), mix(c, d, 0.5), 0.5);
	*/

	check_gl();
	
	glActiveTexture(GL_TEXTURE0);

	self->textured.init(
		Shader(Shader::Vertex, defaultVsSrc),
		Shader(Shader::Fragment, defaultFsSrc),
		self->width,
		self->height
	);

	self->white_texture = Texture(1, 1);
	u8 white[] = {0xff, 0xff, 0xff, 0xff};
	self->white_texture.upload_subimage(tl::ImageSlice(white, 1, 1, 4, 4));

#else
	glOrtho(0.0, self->width, self->height, 0.0, -1.0, 1.0);
#endif

	return 0;
}

int CommonWindow::clear() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	return 0;
}

static void force_button(CommonWindow* self, int id, char down, uint32_t character, bool collect_event) {
	assert(down == 0 || down == 1);

	if (id >= kbRangeBegin && id <= numButtons) {
		self->button_state[id] = down;
	} else if (character != 0) {
		id = kbUnknown;
	} else {
		// No content
		return;
	}

	if (collect_event) {
		Event e;
		e.ev = ev_button;
		e.down = down;
		e.d.button.id = id;

		self->events.push_back(e);
	}
}

static void set_button(CommonWindow* self, int id, char down, uint32_t character, bool collect_event) {
	if((id < kbRangeBegin || id > numButtons)
	|| self->button_state[id] != down)
		force_button(self, id, down, character, collect_event);
}

CommonWindow::CommonWindow() {
	memset(this, 0, sizeof(CommonWindow));

#if GFX_PREDICT_VSYNC
	this->min_swap_interval = 0xffffffff;
#endif
	this->refresh_rate = 60;
	this->bpp = 32;
	init_keymaps();
}

}
