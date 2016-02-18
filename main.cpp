#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <raytrace.hpp>
#include <tiny_obj_loader.h>
#include <string>
#include <iostream>
#include <functional>
#include <string>

struct window_context_t {
    int texture_width = 0;
    int texture_height = 0;
    bool texture_created = false;
    GLuint texture_id = 0;
    GLuint program_id = 0;
};

GLFWwindow* create_window(int x, int y) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    return glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
}

GLuint create_shader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);

    const GLchar* c_source = source.c_str();
    glShaderSource(shader, 1, &c_source, 0);
    glCompileShader(shader);

    GLint is_compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &is_compiled);

    if(is_compiled == GL_FALSE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

        std::vector<GLchar> info(length);
        glGetShaderInfoLog(shader, length, &length, info.data());
        
        glDeleteShader(shader);

        std::cerr << "Failed to compile shader. Info log:\n"
            << info.data() << std::endl;

        return 0;
    }

    return shader;
}

GLuint create_program() {
    const char* fragment_shader_source = R""(
#version 330

out vec4 color;

void main()
{
    color = vec4(1.0, 0.0, 1.0, 1.0);
}
    )"";

    const char* vertex_shader_source = R""(
#version 330

layout(location = 0)in vec3 position;

void main()
{
    gl_Position = vec4(position, 1.f);
}
    )"";

    GLuint fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    GLuint vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);

    GLuint program = glCreateProgram();
    glAttachShader(program, fragment_shader);
    glAttachShader(program, vertex_shader);

    glBindAttribLocation(program, 0, "position");

    glLinkProgram(program);

    GLint is_linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, (int *)&is_linked);

    if(is_linked == GL_FALSE) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

        std::vector<GLchar> info(length);
        glGetProgramInfoLog(program, length, &length, info.data());
        
        glDeleteProgram(program);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);

        std::cerr << "Failed to link program. Info log:\n"
            << info.data() << std::endl;
        
        return 0;
    }

    glDetachShader(program, fragment_shader);
    glDetachShader(program, vertex_shader);
    glDeleteShader(fragment_shader);
    glDeleteShader(vertex_shader);

    return program;
}

GLuint create_fullscreen_quad() {
    float data[6][3] = {
       { -1.0, +1.0, 0.0  },
       { -1.0, -1.0, 0.0  },
       { +1.0, -1.0, 0.0  },
       { -1.0, +1.0, 0.0  },
       { +1.0, -1.0, 0.0  },
       { +1.0, +1.0, 0.0  },
   };

    GLuint result = 0;

    glGenBuffers(1, &result);
    glBindBuffer(GL_ARRAY_BUFFER, result);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    return result;
}

void draw_fullscreen_quad(GLuint quad) {
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, quad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
}

void window_resize(GLFWwindow* window, int width, int height) {
    window_context_t* context = (window_context_t*)glfwGetWindowUserPointer(window);

    if (!context->texture_created) {
        glGenTextures(1, &context->texture_id);
        context->texture_created = true;
    }

    if (context->texture_width != width || context->texture_height != height) {
        glBindTexture(GL_TEXTURE_2D, context->texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
        context->texture_width = width;
        context->texture_height = height;
    }

    std::cerr << "Window resized to (" << width << ", " << height << ")." << std::endl;
}

int run(int width, int height, const std::function<void(GLFWwindow* window)>& func) {
    if (!glfwInit()) {
        std::cerr << "Cannot initialize glfw." << std::endl;
        return -1;
    }

    auto window = create_window(width, height);

    if (!window) {
        std::cerr << "Cannot create window." << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    
    if (!gladLoadGL()) {
        std::cerr << "Cannot load glad extensions." << std::endl;
        glfwTerminate();
        return -1;
    }

    std::cerr << "Loaded OpenGL " 
        << GLVersion.major << "." 
        << GLVersion.minor << " profile." 
        << std::endl;

    window_context_t* context = new window_context_t();
    glfwSetWindowUserPointer(window, context);
    glfwSetWindowSizeCallback(window, window_resize);
    window_resize(window, width, height);
    context->program_id = create_program();

    GLuint global_vao = 0;

    glGenVertexArrays(1, &global_vao);
    glBindVertexArray(global_vao);

    func(window);

    glDeleteVertexArrays(1, &global_vao);

    if (context->texture_created) {
        glDeleteTextures(1, &context->texture_id);
    }

    delete context;

    glfwTerminate();

    return 0;
}

int main(void) {
    return run(640, 480, [](GLFWwindow* window) {
        window_context_t* context = (window_context_t*)glfwGetWindowUserPointer(window);

        GLuint triangleVBO = create_fullscreen_quad();

        glUseProgram(context->program_id);

        while (!glfwWindowShouldClose(window)) {
            glViewport(0, 0, context->texture_width, context->texture_height);
            glClearColor(0.f, 1.f, 0.f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);
            
            draw_fullscreen_quad(triangleVBO);
            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        
    });
}
