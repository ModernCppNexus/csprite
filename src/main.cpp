#include <cstdio>
#include <iostream>
#include <string>

#include "../include/imgui/imgui.h"
#include "../include/imgui/imgui_impl_glfw.h"
#include "../include/imgui/imgui_impl_opengl3.h"
#include "../lib/ImGuiFileDialog.h"

#include "../include/glad/glad.h"
#include "GLFW/glfw3.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../lib/stb/stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../lib/stb/stb_image.h"

#include "shader.h"
#include "math_linear.h"
#include "main.h"

std::string FILE_NAME = "test.png"; // Default Output Filename
int WINDOW_DIMS[2] = {700, 500}; // Default Window Dimensions
int DIMS[2] = {20, 16}; // Width, Height Default Canvas Size

unsigned char *canvas_data; // Canvas Data Containg Pixel Values.

unsigned char last_palette_index = 1;
unsigned char palette_index = 1;
unsigned char palette_count = 16;
unsigned char palette[128][4] = {
	{ 0,   0,   0,   0   }, // Black Transparent/None
	// Pico 8 Color Palette - https://lospec.com/palette-list/pico-8
	{ 0,   0,   0,   255 }, // Black Color
	{ 29,  43,  83,  255 }, // Dark Violet
	{ 126, 37,  83,  255 }, // Dark Pink
	{ 0,   135, 81,  255 }, // Dark Green
	{ 171, 82,  54,  255 }, // Dark Orange
	{ 95,  87,  79,  255 }, // Dark Brown
	{ 194, 195, 199, 255 }, // Grey
	{ 255, 241, 232, 255 }, // Seashell
	{ 255, 0,   77,  255 }, // Redish Pink
	{ 255, 163, 0,   255 }, // Orange
	{ 255, 236, 39,  255 }, // Yellow
	{ 0,   228, 54,  255 }, // Green
	{ 41,  173, 255, 255 }, // Blue
	{ 131, 118, 156, 255 }, // Light Purple
	{ 255, 119, 168, 255 }, // Pink
	{ 255, 204, 170, 255 }  // Pale Orange
};

// NO_MODE defines that there shouldn't be anything drawn
enum mode { SQUARE_BRUSH, CIRCLE_BRUSH, PAN, FILL };

unsigned char zoom_level = 4; // Default Zoom Level
unsigned char zoom[8] = {1, 2, 4, 8, 16, 32, 64, 128}; // Zoom Levels
unsigned char brush_size = 1; // Default Brush Size

// Holds if a ctrl/shift is pressed or not
unsigned char ctrl = 0;
unsigned char shift = 0;

enum mode mode = SQUARE_BRUSH;
enum mode last_mode = SQUARE_BRUSH;
bool CANVAS_FREEZE = false;
unsigned char *draw_colour; // Holds Pointer To Currently Selected Color
unsigned char erase[4] = {0, 0, 0, 0}; // Erase Color, Transparent Black.
unsigned char should_save = 0;

GLfloat viewport[4];
GLfloat vertices[] = {
	 1.0f,  1.0f,  0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,
	 1.0f, -1.0f,  0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
	-1.0f, -1.0f,  0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f,
	-1.0f,  1.0f,  0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f
};

unsigned int indices[] = {0, 1, 3, 1, 2, 3};

double cursor_pos[2];
double cursor_pos_last[2];
double cursor_pos_relative[2];

int main(int argc, char **argv) {
	for (unsigned char i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-f") == 0) {
			FILE_NAME = argv[i+1];
			load_image_to_canvas();
			i++;
		}

		if (strcmp(argv[i], "-d") == 0) {
			int w, h;
			string_to_int(&w, argv[i + 1]);
			string_to_int(&h, argv[i + 2]);
			DIMS[0] = w;
			DIMS[1] = h;
			i += 2;
		}

		if (strcmp(argv[i], "-o") == 0) {
			FILE_NAME = argv[i + 1];
			i++;
		}

		if (strcmp(argv[i], "-w") == 0) {
			int w, h;
			string_to_int(&w, argv[i + 1]);
			string_to_int(&h, argv[i + 2]);
			WINDOW_DIMS[0] = w;
			WINDOW_DIMS[1] = h;
			i += 2;
		}

		if (strcmp(argv[i], "-p") == 0) {
			palette_count = 0;
			i++;

			while (i < argc && (strlen(argv[i]) == 6 || strlen(argv[i]) == 8)) {
				long number = (long)strtol(argv[i], NULL, 16);
				int start;
				unsigned char r, g, b, a;

				if (strlen(argv[i]) == 6) {
					start = 16;
					a = 255;
				} else if (strlen(argv[i]) == 8) {
					start = 24;
					a = number >> (start - 24) & 0xff;
				} else {
					puts("Invalid colour in palette, "
						 "check the length is 6 or 8."
						 " Make sure to convert to LF"
						 " line endings if this file "
						 "came from the web or a "
						 "Windows PC");
					break;
				}

				r = number >> start & 0xff;
				g = number >> (start - 8) & 0xff;
				b = number >> (start - 16) & 0xff;

				palette[palette_count + 1][0] = r;
				palette[palette_count + 1][1] = g;
				palette[palette_count + 1][2] = b;
				palette[palette_count + 1][3] = a;

				printf("Adding colour: #%s - rgb(%d, %d, %d)\n", argv[i], r, g, b);

				palette_count++;
				i++;
			}
		}
	}

	if (canvas_data == NULL)
		canvas_data = (unsigned char *)malloc(DIMS[0] * DIMS[1] * 4 * sizeof(unsigned char));

	GLFWwindow *window;
	GLFWcursor *cursor = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);

	draw_colour = palette[palette_index];

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_TRUE);

	window = glfwCreateWindow(WINDOW_DIMS[0], WINDOW_DIMS[1], "CSprite", NULL, NULL);

	if (!window)
		puts("Failed to create GLFW window");

	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		puts("Failed to init GLAD");

	glfwSetCursor(window, cursor);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	viewport[0] = (int)(WINDOW_DIMS[0] / 2 - DIMS[0] * zoom[zoom_level] / 2);
	viewport[1] = (int)(WINDOW_DIMS[1] / 2 - DIMS[1] * zoom[zoom_level] / 2);
	viewport[2] = DIMS[0] * zoom[zoom_level];
	viewport[3] = DIMS[1] * zoom[zoom_level];

	viewport_set();
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetKeyCallback(window, key_callback);

	unsigned int shader_program = create_shader_program(NULL, NULL, NULL);

	unsigned int vbo, vao, ebo;
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);
	glBindVertexArray(vao);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	unsigned int texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, DIMS[0], DIMS[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, canvas_data);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = NULL; // Disable Generation of .ini file

	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	ImGuiWindowFlags window_flags = 0;
	window_flags |= ImGuiWindowFlags_NoBackground;
	window_flags |= ImGuiWindowFlags_NoTitleBar;
	window_flags |= ImGuiWindowFlags_NoResize;
	window_flags |= ImGuiWindowFlags_NoMove;
	ImVec2 windowPos = {
		0, 0
	};

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		process_input(window);

		glClearColor(0.075, 0.075, 0.1, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(shader_program);
		glBindVertexArray(vao);

		glBindTexture(GL_TEXTURE_2D, 0);

		unsigned int alpha_loc = glGetUniformLocation(shader_program, "alpha");
		glUniform1f(alpha_loc, 0.2f);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		glUniform1f(alpha_loc, 1.0f);
		glBindTexture(GL_TEXTURE_2D, texture);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, DIMS[0], DIMS[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, canvas_data);

		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		if (should_save > 0) {
			// Using Malloc Because Can't Use "new";
			unsigned char *data = (unsigned char *) malloc(DIMS[0] * DIMS[1] * 4 * sizeof(unsigned char));

			// glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			stbi_write_png(FILE_NAME.c_str(), DIMS[0], DIMS[1], 4, data, 0);

			free(data);
			should_save = 0;
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("SelectedToolWindow", NULL, window_flags);
		ImGui::SetWindowPos(windowPos);

		if (ImGui::Button("Open File")) {
			CANVAS_FREEZE = true;
			ImGui::SetNextWindowSize({580,380});
			ImGuiFileDialog::Instance()->OpenDialog("OpenFileDialogKey0", "Choose File", ".png,.PNG", ".");
		}

		if (ImGuiFileDialog::Instance()->Display("OpenFileDialogKey0")) {
			CANVAS_FREEZE = true;
			if (ImGuiFileDialog::Instance()->IsOk()) {
				FILE_NAME = ImGuiFileDialog::Instance()->GetFilePathName();
				load_image_to_canvas();
			}

			CANVAS_FREEZE = false;
			ImGuiFileDialog::Instance()->Close();
		}

		if (mode == SQUARE_BRUSH)
			if (palette_index == 0) {
				ImGui::Text("Square Eraser - (Size: %d)", brush_size);
			} else {
				ImGui::Text("Square Brush - (Size: %d)", brush_size);
			}
		else if (mode == CIRCLE_BRUSH)
			if (palette_index == 0) {
				ImGui::Text("Circle Eraser - (Size: %d)", brush_size);
			} else {
				ImGui::Text("Circle Brush - (Size: %d)", brush_size);
			}
		else if (mode == FILL)
			ImGui::Text("Fill");
		else if (mode == PAN)
			ImGui::Text("Panning");

		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

unsigned char * get_char_data(unsigned char *data, int x, int y) {
	return data + ((y * DIMS[0] + x) * 4);
}

void framebuffer_size_callback(GLFWwindow *window, int w, int h) {
	glViewport(0, 0, w, h);
}

void process_input(GLFWwindow *window) {
	if (CANVAS_FREEZE == true)
		return;

	int x, y;
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS) {
		x = (int)(cursor_pos_relative[0] / zoom[zoom_level]);
		y = (int)(cursor_pos_relative[1] / zoom[zoom_level]);

		if (x >= 0 && x < DIMS[0] && y >= 0 && y < DIMS[1]) {
			switch (mode) {
				case SQUARE_BRUSH: case CIRCLE_BRUSH:
					draw(x, y);
					break;
				case PAN:
					break;
				case FILL: {
					unsigned char *ptr = get_pixel(y, y);
					// Color Clicked On.
					unsigned char colour[4] = {
						*(ptr + 0),
						*(ptr + 1),
						*(ptr + 2),
						*(ptr + 3)
					};
					fill(x, y, colour);
					break;
				}
			}
		}
	}
}

void mouse_callback(GLFWwindow *window, double x, double y) {
	/* infitesimally small chance aside from startup */
	if (cursor_pos_last[0] != 0 && cursor_pos_last[1] != 0) {
		if (mode == PAN) {
			float xmov = (cursor_pos_last[0] - cursor_pos[0]);
			float ymov = (cursor_pos_last[1] - cursor_pos[1]);
			viewport[0] -= xmov;
			viewport[1] += ymov;
			viewport_set();
		}
	}

	cursor_pos_last[0] = cursor_pos[0];
	cursor_pos_last[1] = cursor_pos[1];
	cursor_pos[0] = x;
	cursor_pos[1] = y;
	cursor_pos_relative[0] = x - viewport[0];
	cursor_pos_relative[1] = (y + viewport[1]) - (WINDOW_DIMS[1] - viewport[3]);
}

void mouse_button_callback(GLFWwindow *window, int button, int down, int c) {
	// Will Use For SOMETHING in future.
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
	if (yoffset > 0)
		adjust_zoom(1);
	else
		adjust_zoom(0);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
	if (action == GLFW_PRESS) {
		if (mods == GLFW_MOD_CONTROL) {
			ctrl = 1;

			// if ctrl key is pressed and + or - is pressed, adjust the zoom size
			if (key == GLFW_KEY_EQUAL) {
				adjust_zoom(1);
			} else if (key == GLFW_KEY_MINUS) {
				adjust_zoom(0);
			}
		}

		if (mods == GLFW_MOD_SHIFT) {
			shift = 1;
		}
	} else if (action == GLFW_RELEASE) {
		if (mods == GLFW_MOD_CONTROL)
			ctrl = 0;
		
		if (mods == GLFW_MOD_SHIFT)
			shift = 0;

		if (key == GLFW_KEY_I) {
			if (brush_size < 255)
				brush_size++;
		} else if (key == GLFW_KEY_O) {
			if (brush_size != 1)
				brush_size--;
		}
	}

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);

	// Color Changing
	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_H) {
			if (palette_index > 1)
				palette_index--;
		} else if (key == GLFW_KEY_J) {
			if (palette_index > palette_count + 8)
				palette_index += 8;
		} else if (key == GLFW_KEY_K) {
			if (palette_index < palette_count - 8)
				palette_index -= 8;
		} else if (key == GLFW_KEY_L)
			if (palette_index < palette_count)
				palette_index++;

		if (key == GLFW_KEY_1) {
			if (palette_count >= 1) {
				palette_index = shift ? 9 : 1;
			}
		} else if (key == GLFW_KEY_2) {
			if (palette_count >= 2) {
				palette_index = shift ? 10 : 2;
			}
		} else if (key == GLFW_KEY_3) {
			if (palette_count >= 3) {
				palette_index = shift ? 11 : 2;
			}
		} else if (key == GLFW_KEY_4) {
			if (palette_count >= 4) {
				palette_index = shift ? 12 : 4;
			}
		} else if (key == GLFW_KEY_5) {
			if (palette_count >= 5) {
				palette_index = shift ? 13 : 5;
			}
		} else if (key == GLFW_KEY_6) {
			if (palette_count >= 6) {
				palette_index = shift ? 14 : 6;
			}
		} else if (key == GLFW_KEY_7) {
			if (palette_count >= 7) {
				palette_index = shift ? 15 : 7;
			}
		} else if (key == GLFW_KEY_8) {
			if (palette_count >= 8) {
				if (shift)
					palette_index = 16;
				else
					palette_index = 8;
			}
		}

		if (key == GLFW_KEY_F) {
			mode = FILL;
		} else if (key == GLFW_KEY_B) {
			if (shift)
				mode = CIRCLE_BRUSH;
			else
				mode = SQUARE_BRUSH;

			palette_index = last_palette_index;
		} else if (key == GLFW_KEY_E) {
			if (shift)
				mode = CIRCLE_BRUSH;
			else
				mode = SQUARE_BRUSH;

			last_palette_index = palette_index;
			palette_index = 0;
		}
	}

	if (key == GLFW_KEY_SPACE) {
		if (action == GLFW_PRESS) {
			last_mode = mode;
			mode = PAN;
		} else if (action == GLFW_RELEASE) {
			mode = last_mode;
		}
	}

	draw_colour = palette[palette_index];

	if (key == GLFW_KEY_S && action == GLFW_PRESS) {
		should_save = 1;
	}
}

void viewport_set() {
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void adjust_zoom(int increase) {
	if (increase > 0) {
		if (zoom_level < 5)
			zoom_level++;
	} else {
		if (zoom_level > 0)
			zoom_level--;
	}

	viewport[0] = (int)(WINDOW_DIMS[0] / 2 - DIMS[0] * zoom[zoom_level] / 2);
	viewport[1] = (int)(WINDOW_DIMS[1] / 2 - DIMS[1] * zoom[zoom_level] / 2);
	viewport[2] = DIMS[0] * zoom[zoom_level];
	viewport[3] = DIMS[1] * zoom[zoom_level];

	viewport_set();
}

int string_to_int(int *out, char *s) {
	char *end;
	if (s[0] == '\0')
		return -1;
	long l = strtol(s, &end, 10);
	if (l > INT_MAX)
		return -2;
	if (l < INT_MIN)
		return -3;
	if (*end != '\0')
		return -1;
	*out = l;
	return 0;
}

int color_equal(unsigned char *a, unsigned char *b) {
	if (*(a + 0) == *(b + 0) && *(a + 1) == *(b + 1) && *(a + 2) == *(b + 2) &&
		*(a + 3) == *(b + 3)) {
		return 1;
	}
	return 0;
}

unsigned char * get_pixel(int x, int y) {
	return canvas_data + ((y * DIMS[0] + x) * 4);
}

void draw(int x, int y) {
	for (int yr = -brush_size/2; yr < brush_size/2+1; yr++) {
		for (int xr = -brush_size/2; xr < brush_size/2+1; xr++) {
			if (x+xr < 0 || x+xr >= DIMS[0] || y+yr < 0 || y+yr > DIMS[1])
				continue;

			if (mode == CIRCLE_BRUSH && xr*xr + yr*yr > brush_size / 2 * brush_size / 2)
				continue;

			unsigned char *ptr = get_pixel(x+xr, y+yr);

			// Set Pixel Color
			*ptr = draw_colour[0]; // Red
			*(ptr + 1) = draw_colour[1]; // Green
			*(ptr + 2) = draw_colour[2]; // Blue
			*(ptr + 3) = draw_colour[3]; // Alpha
		}
	}
}

// Fill Tool, Fills The Whole Canvas Using Recursion
void fill(int x, int y, unsigned char *old_colour) {
	unsigned char *ptr = get_pixel(x, y);
	if (color_equal(ptr, old_colour)) {
		*ptr = draw_colour[0];
		*(ptr + 1) = draw_colour[1];
		*(ptr + 2) = draw_colour[2];
		*(ptr + 3) = draw_colour[3];

		if (x != 0 && !color_equal(get_pixel(x - 1, y), draw_colour))
			fill(x - 1, y, old_colour);
		if (x != DIMS[0] - 1 && !color_equal(get_pixel(x + 1, y), draw_colour))
			fill(x + 1, y, old_colour);
		if (y != DIMS[1] - 1 && !color_equal(get_pixel(x, y + 1), draw_colour))
			fill(x, y + 1, old_colour);
		if (y != 0 && !color_equal(get_pixel(x, y - 1), draw_colour))
			fill(x, y - 1, old_colour);
	}
}

void load_image_to_canvas() {
	CANVAS_FREEZE = true;
	int x, y, c;
	unsigned char *image_data = stbi_load(FILE_NAME.c_str(), &x, &y, &c, 0);
	if (image_data == NULL) {
		printf("Unable to load image %s\n", FILE_NAME.c_str());
	} else {
		DIMS[0] = x;
		DIMS[1] = y;
		canvas_data = (unsigned char *)malloc(DIMS[0] * DIMS[1] * 4 * sizeof(unsigned char));
		int j, k;
		unsigned char *ptr;
		unsigned char *iptr;
		for (j = 0; j < y; j++) {
			for (k = 0; k < x; k++) {
				ptr = get_pixel(k, j);
				iptr = get_char_data(image_data, k, j);
				*(ptr+0) = *(iptr+0);
				*(ptr+1) = *(iptr+1);
				*(ptr+2) = *(iptr+2);
				*(ptr+3) = *(iptr+3);
			}
		}
		stbi_image_free(image_data);
	}
	CANVAS_FREEZE = false;
}