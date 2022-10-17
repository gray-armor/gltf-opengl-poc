#include "tiny_gltf.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>

int main(int argc, char **argv) {
  if (argc < 5) {
    std::cout << "[HELP] gltf-viewer <model path>.gltf <binary path>.bin <vertex shader path>.glsl <fragment shader path>.glsl" << std::endl;
    return EXIT_FAILURE;
  }

  std::string gltf_path(argv[1]);
  std::string binary_path(argv[2]);
  std::string vertex_shader_path(argv[3]);
  std::string fragment_shader_path(argv[4]);

  tinygltf::TinyGLTF loader;
  tinygltf::Model model;

  std::string err;
  std::string warn;


  bool result = false;
  // TODO: support glb
  result = loader.LoadASCIIFromFile(&model, &err, &warn, gltf_path.c_str());
  if(!result) {
    std::cerr << "[ERROR] Failed to load gltf: " << gltf_path << std::endl;
    return EXIT_FAILURE;
  }
  if (!err.empty()) {
    std::cerr << "[ERROR] " << err << std::endl;
    return EXIT_FAILURE;
  }
  if (!warn.empty()) {
    std::cout << "[WARN] " << warn << std::endl;
  }

  int width = 500;
  int height = 500;
  std::string title = "glTF viewer";
  GLFWwindow* window = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);
  if (window == NULL) {
    std::cerr << "Failed to open GLFW window." << std::endl;
  }

  // glewInit()

  while(glfwWindowShouldClose(window) == GL_FALSE) {
    glfwPollEvents();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  return EXIT_SUCCESS;
}
