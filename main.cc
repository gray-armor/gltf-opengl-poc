#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>

#include "tiny_gltf.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

static void
drawMesh(tinygltf::Model &model, const tinygltf::Mesh &mesh,
    std::map<int, GLuint> &bufferViewMap,
    std::map<std::string, int> &attributeLocationMap)
{
  // isCurvesLoc

  for (auto primitive : mesh.primitives) {
    if (primitive.indices < 0) return;

    for (auto [attribute, accessorIndex] : primitive.attributes) {
      assert(accessorIndex >= 0);
      const tinygltf::Accessor &accessor = model.accessors[accessorIndex];

      glBindBuffer(GL_ARRAY_BUFFER, bufferViewMap[accessor.bufferView]);

      int size = -1;
      if (accessor.type == TINYGLTF_TYPE_SCALAR) {
        size = 1;
      } else if (accessor.type == TINYGLTF_TYPE_VEC2) {
        size = 2;
      } else if (accessor.type == TINYGLTF_TYPE_VEC3) {
        size = 3;
      } else if (accessor.type == TINYGLTF_TYPE_VEC4) {
        size = 4;
      } else {
        assert(0);
      }

      if (attribute == "POSITION" || attribute == "NORMAL" ||
          attribute == "TEXCOORD_0") {
        if (attributeLocationMap[attribute] < 0) continue;

        int byteStride =
            accessor.ByteStride(model.bufferViews[accessor.bufferView]);
        assert(byteStride != -1);

        glVertexAttribPointer(attributeLocationMap[attribute], size,
            accessor.componentType, accessor.normalized ? GL_TRUE : GL_FALSE,
            byteStride, BUFFER_OFFSET(accessor.byteOffset));
        glEnableVertexAttribArray(attributeLocationMap[attribute]);
      }
    }

    const tinygltf::Accessor &accessor = model.accessors[primitive.indices];
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferViewMap[accessor.bufferView]);

    int mode = -1;
    switch (primitive.mode) {
      case TINYGLTF_MODE_TRIANGLES:
        mode = GL_TRIANGLES;
        break;
      case TINYGLTF_MODE_TRIANGLE_STRIP:
        mode = GL_TRIANGLE_STRIP;
        break;
      case TINYGLTF_MODE_TRIANGLE_FAN:
        mode = GL_TRIANGLE_FAN;
        break;
      case TINYGLTF_MODE_POINTS:
        mode = GL_POINTS;
        break;
      case TINYGLTF_MODE_LINE:
        mode = GL_LINES;
        break;
      case TINYGLTF_MODE_LINE_LOOP:
        mode = GL_LINE_LOOP;
        break;
      default:
        assert(0);
        break;
    }
    glDrawElements(mode, accessor.count, accessor.componentType,
        BUFFER_OFFSET(accessor.byteOffset));

    {
      for (auto [attribute, _] : primitive.attributes) {
        if (attribute == "POSITION" || attribute == "NORMAL" ||
            attribute == "TEXCOORD_0") {
          if (attributeLocationMap[attribute] < 0) continue;

          glDisableVertexAttribArray(attributeLocationMap[attribute]);
        }
      }
    }
  }
}

static void
drawNode(tinygltf::Model &model, const tinygltf::Node &node,
    std::map<int, GLuint> &bufferViewMap,
    std::map<std::string, int> &attributeLocationMap)
{
  // glPushMtrix();
  glPushMatrix();
  if (node.matrix.size() == 16) {
    glMultMatrixd(node.matrix.data());
  } else {
    // TODO: support matrix: whose size is not 16
  }

  if (node.mesh > -1) {
    assert(node.mesh < model.meshes.size());
    drawMesh(
        model, model.meshes[node.mesh], bufferViewMap, attributeLocationMap);
  }

  for (auto nodeIndex : node.children) {
    drawNode(
        model, model.nodes[nodeIndex], bufferViewMap, attributeLocationMap);
  }

  glPopMatrix();
}

static void
drawModel(tinygltf::Model &model, std::map<int, GLuint> &bufferViewMap,
    std::map<std::string, int> &attributeLocationMap)
{
  assert(model.scenes.size() > 0);

  int defaultSceneIndex = model.defaultScene > -1 ? model.defaultScene : 0;
  const tinygltf::Scene &scene = model.scenes[defaultSceneIndex];
  for (auto nodeIndex : scene.nodes) {
    drawNode(
        model, model.nodes[nodeIndex], bufferViewMap, attributeLocationMap);
  }
}

static bool
setupBuffers(tinygltf::Model &model, GLuint programId,
    std::map<int, GLuint> &bufferViewMap,
    std::map<std::string, int> &attributeLocationMap)
{
  for (size_t i = 0; i < model.bufferViews.size(); i++) {
    auto &bufferView = model.bufferViews[i];
    if (bufferView.target == 0) {
      std::cout << "[WARN] bufferView.target is zero" << std::endl;
      continue;
    }

    int sparse_accessor = -1;
    for (size_t a_i = 0; a_i < model.accessors.size(); ++a_i) {
      auto &accessor = model.accessors[a_i];
      if (accessor.bufferView == i) {
        std::cout << i << " is used by accessor " << a_i << std::endl;
        if (accessor.sparse.isSparse) {
          std::cout
              << "[WARN]: this bufferView has at least one sparse accessor."
              << std::endl;
          sparse_accessor = a_i;
          break;
        }
      }
    }

    const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
    glGenBuffers(1, &bufferViewMap[i]);  // bufferViewを1個つくる
    glBindBuffer(bufferView.target, bufferViewMap[i]);
    std::cout << "buffer.size= " << buffer.data.size()
              << ", byteOffset = " << bufferView.byteOffset << std::endl;

    if (sparse_accessor < 0) {
      glBufferData(bufferView.target, bufferView.byteLength,
          &buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
      glBindBuffer(bufferView.target, 0);
    } else {
      std::cerr << "[ERROR]: Sparse accessor support is TODO." << std::endl;
      return false;
      // TODO: support sparse accessor
    }
  }

  // TODO: Implement texture

  // これ何に使う？
  glUseProgram(programId);
  attributeLocationMap["POSITION"] =
      glGetAttribLocation(programId, "in_vertex");
  attributeLocationMap["NORMAL"] = glGetAttribLocation(programId, "in_normal");
  attributeLocationMap["TEXCOORD_0"] =
      glGetAttribLocation(programId, "in_texcoord");
  // GLint glGetAttribLocation

  return true;
}

static bool
loadShader(GLenum shaderType, GLuint &shader, const char *shaderFilename)
{
  GLint val = 0;
  if (shader != 0) {
    glDeleteShader(shader);
  }

  std::vector<GLchar> srcbuf;
  FILE *fp = fopen(shaderFilename, "rb");
  if (!fp) {
    std::cerr << "Failed to load shader: " << shaderFilename << std::endl;
    return false;
  }
  fseek(fp, 0, SEEK_END);
  size_t len = ftell(fp);
  rewind(fp);
  srcbuf.resize(len + 1);
  len = fread(&srcbuf.at(0), 1, len, fp);
  srcbuf[len] = 0;
  fclose(fp);

  const GLchar *srcs[1];
  srcs[0] = &srcbuf.at(0);

  shader = glCreateShader(shaderType);
  glShaderSource(shader, 1, srcs, NULL);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &val);
  if (val != GL_TRUE) {
    char log[4096];
    GLsizei message_len;
    glGetShaderInfoLog(shader, 4096, &message_len, log);
    std::cerr << log << std::endl;
    std::cerr << "[ERROR] " << shaderFilename << std::endl;
    return false;
  }

  std::cout << "Loaded shader " << shaderFilename << " OK" << std::endl;
  return true;
}

static bool
linkShader(GLuint &programId, GLuint &vertexShader, GLuint &fragmentShader)
{
  GLint val = 0;

  if (programId != 0) {
    glDeleteProgram(programId);
  }

  programId = glCreateProgram();

  glAttachShader(programId, vertexShader);
  glAttachShader(programId, fragmentShader);
  glLinkProgram(programId);

  glGetProgramiv(programId, GL_LINK_STATUS, &val);
  assert(val == GL_TRUE);

  std::cout << "Link shader OK" << std::endl;

  return true;
}

int
main(int argc, char **argv)
{
  if (argc < 5) {
    std::cout << "[HELP] gltf-viewer <model path>.gltf <binary path>.bin "
                 "<vertex shader path>.glsl <fragment shader path>.glsl"
              << std::endl;
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

  // globalになっているところ
  std::map<int, GLuint> bufferViewMap;  // bufferViewIndex : Buffer
  std::map<std::string, int>
      attributeLocationMap;  // attribute : AttribLocation

  bool result = false;
  // TODO: support glb
  result = loader.LoadASCIIFromFile(&model, &err, &warn, gltf_path.c_str());
  if (!result) {
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

  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW" << std::endl;
    return EXIT_FAILURE;
  }

  int width = 500;
  int height = 500;
  std::string title = "glTF viewer";
  GLFWwindow *window =
      glfwCreateWindow(width, height, title.c_str(), NULL, NULL);
  if (window == NULL) {
    std::cerr << "Failed to open GLFW window." << std::endl;
  }

  glfwGetWindowSize(window, &width, &height);
  glfwMakeContextCurrent(window);

  if (glewInit() != GLEW_OK) {
    std::cerr << "Failed to initialize GLEW." << std::endl;
    return EXIT_FAILURE;
  }

  GLuint vertexId = 0, fragmentId = 0, programId = 0;

  if (!loadShader(GL_VERTEX_SHADER, vertexId, vertex_shader_path.c_str()))
    return EXIT_FAILURE;

  if (!loadShader(GL_FRAGMENT_SHADER, fragmentId, fragment_shader_path.c_str()))
    return EXIT_FAILURE;

  if (!linkShader(programId, vertexId, fragmentId)) return EXIT_FAILURE;

  glUseProgram(programId);

  if (setupBuffers(model, programId, bufferViewMap, attributeLocationMap)) {
    std::cerr << "Failed to generate buffers." << std::endl;
    return EXIT_FAILURE;
  }

  while (glfwWindowShouldClose(window) == GL_FALSE) {
    glfwPollEvents();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // render
    drawModel(model, bufferViewMap, attributeLocationMap);

    glFlush();
    glfwSwapBuffers(window);
  }

  glfwTerminate();
  return EXIT_SUCCESS;
}
