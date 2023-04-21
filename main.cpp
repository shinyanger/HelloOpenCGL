#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

#include <glad/glad.h>
#ifdef _WIN64
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <CL/cl.h>
#include <CL/cl_gl.h>

constexpr auto RENDER_WIDTH = 32;
constexpr auto RENDER_HEIGHT = 20;
constexpr auto LOCAL_ITEM_SIZE = 4;
static_assert(RENDER_WIDTH % LOCAL_ITEM_SIZE == 0, "Alignment error");
static_assert(RENDER_HEIGHT % LOCAL_ITEM_SIZE == 0, "Alignment error");

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
"  frag_color = vec4(0.6, 0.6, 0.6, 1.0);"
"}";

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
    glfwWindowHint(GLFW_VISIBLE, FALSE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Hello OpenCGL", NULL, NULL);
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

std::vector<float> PrepareData()
{
    std::vector<float> points{
        0.0f, 0.8f, 0.0f,
        0.8f, -0.8f, 0.0f,
        -0.8f, -0.8f, 0.0f};

    return points;
}

std::optional<GLuint> RenderToGLBuffer(const std::vector<float>& points)
{
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(float), points.data(), GL_STATIC_DRAW);

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, points.size() / 3, GL_FLOAT, GL_FALSE, 0, NULL);

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

    glViewport(0, 0, RENDER_WIDTH, RENDER_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, points.size() / 3);

    glFinish();

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

void OutputPixels(const std::vector<cl_uchar>& pixels)
{
    constexpr auto pixels_size = RENDER_WIDTH * RENDER_HEIGHT;

    for (int i = 0; i < pixels_size; i++)
    {
        std::cout << std::setw(3) << +pixels[i];

        if ((i + 1) % RENDER_WIDTH == 0)
        {
            std::cout << std::endl;
        }
    }
}

void RunCLWithGLBuffer(GLFWwindow* window, GLuint rbo)
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
    size_t local_item_size[] = {LOCAL_ITEM_SIZE, LOCAL_ITEM_SIZE};
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

    OutputPixels(pixels);
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

    auto points = PrepareData();

    auto optRbo = RenderToGLBuffer(points);
    if (!optRbo)
    {
        CleanupGL(window);
        return 1;
    }

    RunCLWithGLBuffer(window, *optRbo);

    CleanupGL(window);

    return 0;
}
