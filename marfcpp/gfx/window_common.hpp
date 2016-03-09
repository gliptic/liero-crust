static void init_extensions() {
	static int glewInitialized = 0;
	if(!glewInitialized) {
		glewInit();
		glewInitialized = 1;
	}
}

static int common_setup_gl(common_window* self) {
	// OpenGL stuff

	init_extensions();

#if BONK_USE_GL2
	if(!GLEW_VERSION_2_0)
		return -1; // TODO: OpenGL 2.0 required
#endif

	glViewport(0, 0, self->width, self->height);
	glClearColor(0.15f, 0.15f, 0.4f, 0.0f);
	//glClearColor(0.f, 0.f, 0.f, 0.0f);
	glClearDepth(0.0f);
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

#if BONK_USE_GL2
	// TODO: OpenGL 2.0+ stuff if supported
#else
	glOrtho(0.0, self->width, self->height, 0.0, -1.0, 1.0);
#endif

	return 0;
}

int common_window::clear() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	return 0;
}

static void force_button(common_window* self, int id, int down, uint32_t character, int collect_event) {
	if(id >= kbRangeBegin && id <= numButtons)
	{
		self->buttonState[id] = down;
	}
	else if(character != 0)
	{
		id = kbUnknown;
	}
	else
	{
		// No content
		return;
	}

	if(collect_event)
	{
		event e;
		e.ev = ev_button;
		e.down = down;
		e.d.button.id = id;

		//tl_vector_pushback(self->events, gfx_event, e);
		self->events.push_back(e);

	/* TODO:
		EButton button;
		button.id = id;
		button.state = down;
		button.character = character;
		onButton(button);*/
	}
}

static void set_button(common_window* self, int id, int down, uint32_t character, int collect_event) {
	if((id < kbRangeBegin || id > numButtons)
	|| self->buttonState[id] != down)
		force_button(self, id, down, character, collect_event);
}