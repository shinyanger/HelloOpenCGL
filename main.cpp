#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

#include <glad/glad.h>
#ifdef _WIN64
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <CL/cl.h>
#include <CL/cl_gl.h>

// #define RENDER_TO_SCREEN

constexpr auto GRID_SIZE = 32;
constexpr auto ROW_NUM = 60;
constexpr auto COL_NUM = 30;
constexpr auto RENDER_WIDTH = GRID_SIZE * ROW_NUM;
constexpr auto RENDER_HEIGHT = GRID_SIZE * COL_NUM;
constexpr auto WINDOW_WIDTH = 1920;
constexpr auto WINDOW_HEIGHT = 1080;

constexpr char VERTEX_SHADER[] =
"#version 400\n"
"in vec3 vp;"
"void main() {"
"  gl_Position = vec4(vp, 1.0);"
"}";

constexpr char FRAGMENT_SHADER[] =
"#version 400\n"
"out vec4 frag_color;"
"void main() {"
"  frag_color = vec4(1.0, 0.0, 0.0, 1.0);"
"}";

using vertices_t = std::vector<GLfloat>;
using shape_t = std::vector<vertices_t>;

static void ValidateConstants()
{
    static_assert(GRID_SIZE % 4 == 0, "Alignment error");
}

void ErrorCallback(int error, const char* description)
{
    std::cerr << "Error: " << description << std::endl;
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

bool CheckFrameBufferStatus()
{
    GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    switch (status)
    {
    case GL_FRAMEBUFFER_COMPLETE:
        break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
        break;
    default:
        std::cerr << "Framebuffer Error" << std::endl;
        return false;
    }

    return true;
}

GLFWwindow* InitGL()
{
    glfwSetErrorCallback(ErrorCallback);

    if (!glfwInit())
    {
        return nullptr;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#ifndef RENDER_TO_SCREEN
    glfwWindowHint(GLFW_VISIBLE, FALSE);
#endif

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Hello OpenCGL", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return nullptr;
    }

    glfwSetKeyCallback(window, KeyCallback);

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSwapInterval(1);

    return window;
}

std::vector<shape_t> PrepareShapes()
{
    std::vector<shape_t> shapes;

    shape_t shape1{
        {
            -0.8f, 0.8f, 0.0f,
            0.8f, 0.8f, 0.0f,
            0.8f, -0.8f, 0.0f,
            -0.8f, -0.8f, 0.0f
        },
        {
            -0.4f, 0.4f, 0.0f,
            0.4f, 0.4f, 0.0f,
            0.4f, -0.4f, 0.0f,
            -0.4f, -0.4f, 0.0f
        }
    };
    shapes.emplace_back(shape1);

    shape_t shape2{
        {
            -0.8f, 0.8f, 0.0f,
            -0.4f, 0.8f, 0.0f,
            -0.4f, 0.0f, 0.0f,
            0.4f, 0.0f, 0.0f,
            0.4f, 0.8f, 0.0f,
            0.8f, 0.8f, 0.0f,
            0.8f, -0.8f, 0.0f,
            -0.8f, -0.8f, 0.0f
        }
    };
    shapes.emplace_back(shape2);

    shape_t shape3{
        {
            -0.8f, 0.0f, 0.0f,
            -0.4f, 0.0f, 0.0f,
            -0.4f, 0.8f, 0.0f,
            0.4f, 0.8f, 0.0f,
            0.4f, 0.0f, 0.0f,
            0.8f, 0.0f, 0.0f,
            0.8f, -0.8f, 0.0f,
            -0.8f, -0.8f, 0.0f
        }
    };
    shapes.emplace_back(shape3);

    return shapes;
}

std::vector<std::vector<GLuint>> GetGLVertexArray(const std::vector<shape_t>& shapes)
{
    assert(shapes.size() <= ROW_NUM * COL_NUM);

    std::vector<std::vector<GLuint>> vboss(shapes.size());
    std::vector<std::vector<GLuint>> vaoss(shapes.size());
    for (size_t i = 0; i < shapes.size(); i++)
    {
        auto row = i / COL_NUM;
        auto col = i % COL_NUM;

        auto& shape = shapes[i];
        std::vector<GLuint> vbos(shape.size());
        std::vector<GLuint> vaos(shape.size());
        glGenBuffers(vbos.size(), vbos.data());
        glGenVertexArrays(vaos.size(), vaos.data());

        for (size_t j = 0; j < shape.size(); j++)
        {
            vertices_t vertices = shape[j];
            for (size_t k = 0; k < vertices.size(); k += 3)
            {
                auto& x = vertices[k];
                auto& y = vertices[k + 1];
                auto localX = (x + 1) / 2 * GRID_SIZE;
                auto localY = (y + 1) / 2 * GRID_SIZE;
                auto absX = localX + col * GRID_SIZE;
                auto absY = localY + row * GRID_SIZE;
                x = absX * 2 / RENDER_WIDTH - 1;
                y = absY * 2 / RENDER_HEIGHT - 1;
            }

            glBindVertexArray(vaos[j]);
            glBindBuffer(GL_ARRAY_BUFFER, vbos[j]);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
            glEnableVertexAttribArray(0);
        }

        vboss[i] = std::move(vbos);
        vaoss[i] = std::move(vaos);
    }

    return vaoss;
}

GLuint GetGLProgram()
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    const GLchar* vertex_shader = VERTEX_SHADER;
    glShaderSource(vs, 1, &vertex_shader, NULL);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar* fragment_shader = FRAGMENT_SHADER;
    glShaderSource(fs, 1, &fragment_shader, NULL);
    glCompileShader(fs);

    GLuint program = glCreateProgram();
    glAttachShader(program, fs);
    glAttachShader(program, vs);
    glLinkProgram(program);

    return program;
}

void GLDrawArrays(GLuint program, const std::vector<std::vector<GLuint>>& vaoss, const std::vector<shape_t>& shapes)
{
    glViewport(0, 0, RENDER_WIDTH, RENDER_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);

    for (size_t i = 0; i < vaoss.size(); i++)
    {
        auto& vaos = vaoss[i];
        auto& shape = shapes[i];
        for (size_t j = 0; j < vaos.size(); j++)
        {
            glBindVertexArray(vaos[j]);
            glDrawArrays(GL_LINE_LOOP, 0, shape[j].size() / 3);
        }
    }

    glFinish();
}

std::optional<GLuint> RenderToGLBuffer(GLuint program, const std::vector<std::vector<GLuint>>& vaoss, const std::vector<shape_t>& shapes)
{
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, RENDER_WIDTH, RENDER_HEIGHT);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    if (!CheckFrameBufferStatus())
    {
        return {};
    }

    GLDrawArrays(program, vaoss, shapes);

    return rbo;
}

std::string ReadCLKernel(const std::string& path)
{
    auto size = std::filesystem::file_size(path);
    std::string result(size, 0);
    std::ifstream fs(path, std::ios::in | std::ios::binary);
    fs.read(result.data(), size);
    fs.close();

    return result;
}

std::vector<cl_uchar> GetNthPixels(const std::vector<cl_uchar>& pixels, size_t index)
{
    std::vector<cl_uchar> gridPixels(GRID_SIZE * GRID_SIZE);

    auto row = index / COL_NUM;
    auto col = index % COL_NUM;
    for (size_t i = 0; i < GRID_SIZE; i++)
    {
        auto startIt = std::next(pixels.begin(), (row * GRID_SIZE + i) * RENDER_WIDTH + col * GRID_SIZE);
        auto endIt = std::next(startIt, GRID_SIZE);
        auto dstIt = std::next(gridPixels.begin(), i * GRID_SIZE);
        std::copy(startIt, endIt, dstIt);
    }

    return gridPixels;
}

void OutputPixels(const std::vector<cl_uchar>& pixels, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        auto gridPixels = GetNthPixels(pixels, i);
        for (size_t j = 0; j < gridPixels.size(); j++)
        {
            auto str = gridPixels[j] > 0 ? "11" : "00";
            std::cout << str;

            if ((j + 1) % GRID_SIZE == 0)
            {
                std::cout << std::endl;
            }
        }
        std::cout << std::endl;
    }
}

void RunCLWithGLBuffer(GLFWwindow* window, GLuint rbo, size_t count)
{
    cl_platform_id platform_id = NULL;
    cl_device_id device_id = NULL;
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
    cl_int ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
    ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, &ret_num_devices);

    cl_context_properties properties[] = {
#ifdef _WIN64
        CL_GL_CONTEXT_KHR,
        (cl_context_properties)glfwGetWGLContext(window),
        CL_WGL_HDC_KHR,
        (cl_context_properties)GetDC(glfwGetWin32Window(window)),
#else
#error share GL context in macOS
#endif
        0};

    cl_context context = clCreateContext(properties, 1, &device_id, NULL, NULL, &ret);

    cl_command_queue command_queue = clCreateCommandQueueWithProperties(context, device_id, 0, &ret);

    cl_mem mem_obj = clCreateFromGLRenderbuffer(context, CL_MEM_READ_ONLY, rbo, &ret);
    cl_mem mem_obj_pixels = clCreateBuffer(context, CL_MEM_WRITE_ONLY, RENDER_WIDTH * RENDER_HEIGHT, NULL, &ret);

    ret = clEnqueueAcquireGLObjects(command_queue, 1, &mem_obj, 0, NULL, NULL);

    auto kernel_file = ReadCLKernel("kernel.cl");
    auto kernel_str = kernel_file.data();
    auto kernel_size = kernel_file.size();

    cl_program program = clCreateProgramWithSource(context, 1, (const char**)&kernel_str, &kernel_size, &ret);
    ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
    cl_kernel kernel = clCreateKernel(program, "hello_opencgl", &ret);

    ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&mem_obj);
    ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&mem_obj_pixels);

    size_t global_item_size[] = {RENDER_WIDTH, RENDER_HEIGHT};
    size_t local_item_size[] = {GRID_SIZE, GRID_SIZE};
    ret = clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, global_item_size, local_item_size, 0, NULL, NULL);

    ret = clEnqueueReleaseGLObjects(command_queue, 1, &mem_obj, 0, NULL, NULL);

    constexpr auto pixels_size = RENDER_WIDTH * RENDER_HEIGHT;
    std::vector<cl_uchar> pixels(pixels_size);
    ret = clEnqueueReadBuffer(command_queue, mem_obj_pixels, CL_TRUE, 0, pixels_size, pixels.data(), 0, NULL, NULL);

    ret = clFlush(command_queue);
    ret = clFinish(command_queue);

    ret = clReleaseKernel(kernel);
    ret = clReleaseProgram(program);
    ret = clReleaseMemObject(mem_obj);
    ret = clReleaseMemObject(mem_obj_pixels);
    ret = clReleaseCommandQueue(command_queue);
    ret = clReleaseContext(context);

    OutputPixels(pixels, count);
}

void CleanupGL(GLFWwindow* window)
{
    glfwDestroyWindow(window);

    glfwTerminate();
}

int main(void)
{
    auto window = InitGL();
    if (!window)
    {
        return 1;
    }

    auto shapes = PrepareShapes();

    auto vaoss = GetGLVertexArray(shapes);

    auto program = GetGLProgram();

#ifdef RENDER_TO_SCREEN
    while (!glfwWindowShouldClose(window))
    {
        GLDrawArrays(program, vaoss, shapes);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
#else
    auto optRbo = RenderToGLBuffer(program, vaoss, shapes);
    if (!optRbo)
    {
        CleanupGL(window);
        return 1;
    }

    RunCLWithGLBuffer(window, *optRbo, shapes.size());
#endif

    CleanupGL(window);

    return 0;
}
