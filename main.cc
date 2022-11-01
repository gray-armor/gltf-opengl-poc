#include <GL/glew.h>
#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "tiny_gltf.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

#define CAM_Z (3.0f)
int width = 768;
int height = 768;

double prevMouseX, prevMouseY;
bool mouseLeftPressed;
bool mouseRightPressed;
float eye[3], lookat[3], up[3];

GLFWwindow *window;

typedef struct {
  GLuint vb;
} GLBufferState;

typedef struct {
  std::map<std::string, GLint> attribs;
  std::map<std::string, GLint> uniforms;
} GLProgramState;

std::map<int, GLBufferState> glBufferState;
GLProgramState glProgramState;

void
checkErrors(std::string desc)
{
  GLenum e = glGetError();
  if (e != GL_NO_ERROR) {
    fprintf(stderr, "OpenGL error in \"%s\": %d (%d)\n", desc.c_str(), e, e);
    exit(EXIT_FAILURE);
  }
}

static std::string
getFilePathExtension(const std::string &FileName)
{
  if (FileName.find_last_of(".") != std::string::npos)
    return FileName.substr(FileName.find_last_of(".") + 1);
  return "";
}

bool
loadShader(GLenum shaderType, GLuint &shader, const char *shaderSourceFilename)
{
  GLint val = 0;

  if (shader != 0) {
    glDeleteShader(shader);
  }

  std::vector<GLchar> srcbuf;
  FILE *fp = fopen(shaderSourceFilename, "rb");
  if (!fp) {
    fprintf(stderr, "failed to load shader: %s\n", shaderSourceFilename);
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
    GLsizei msglen;
    glGetShaderInfoLog(shader, 4096, &msglen, log);
    printf("%s\n", log);
    // assert(val == GL_TRUE && "failed to compile shader");
    printf(
        "ERR: Failed to load or compile shader [ %s ]\n", shaderSourceFilename);
    return false;
  }

  printf("Load shader [ %s ] OK\n", shaderSourceFilename);
  return true;
}

bool
linkShader(GLuint &prog, GLuint &vertShader, GLuint &fragShader)
{
  GLint val = 0;

  if (prog != 0) {
    glDeleteProgram(prog);
  }

  prog = glCreateProgram();

  glAttachShader(prog, vertShader);
  glAttachShader(prog, fragShader);
  glLinkProgram(prog);

  glGetProgramiv(prog, GL_LINK_STATUS, &val);
  assert(val == GL_TRUE && "failed to link shader");

  printf("Link shader OK\n");

  return true;
}

void
pointerButtonHandler(GLFWwindow *window, int button, int action, int mods)
{
  if ((button == GLFW_MOUSE_BUTTON_LEFT)) {
    if (action == GLFW_PRESS) {
      mouseLeftPressed = true;
      mouseRightPressed = false;
    } else if (action == GLFW_RELEASE) {
      mouseLeftPressed = false;
    }
  }
  if ((button == GLFW_MOUSE_BUTTON_RIGHT)) {
    if (action == GLFW_PRESS) {
      mouseRightPressed = true;
      mouseLeftPressed = false;
    } else if (action == GLFW_RELEASE) {
      mouseRightPressed = false;
    }
  }
}

void
pointerMotionHandler(GLFWwindow *window, double mouse_x, double mouse_y)
{
  float transScale = 2.0f;

  if (mouseLeftPressed) {
    eye[0] += -transScale * (mouse_x - prevMouseX) / (float)width;
    lookat[0] += -transScale * (mouse_x - prevMouseX) / (float)width;
    eye[1] += transScale * (mouse_y - prevMouseY) / (float)height;
    lookat[1] += transScale * (mouse_y - prevMouseY) / (float)height;
  } else if (mouseRightPressed) {
    eye[2] += transScale * (mouse_y - prevMouseY) / (float)height;
    lookat[2] += transScale * (mouse_y - prevMouseY) / (float)height;
  }

  prevMouseX = mouse_x;
  prevMouseY = mouse_y;
}

static void
setupBuffer(tinygltf::Model &model, GLuint progId)
{
  {
    for (int i = 0; i < (int)model.bufferViews.size(); ++i) {
      const tinygltf::BufferView &bufferView = model.bufferViews[i];
      if (bufferView.target == 0) {
        std::cout << "TODO: bufferView.target is zero (unsupported)"
                  << std::endl;
        continue;
      }

      int sparse_accessor = -1;
      for (int a_i = 0; a_i < (int)model.accessors.size(); ++a_i) {
        const auto &accessor = model.accessors[a_i];
        if (accessor.bufferView == i) {
          std::cout << i << " is used by accessor " << a_i << std::endl;
          if (accessor.sparse.isSparse) {
            std::cout << "TODO: support sparse_accessor" << std::endl;
            sparse_accessor = a_i;
            break;
          }
        }
      }

      const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
      GLBufferState state;
      glGenBuffers(1, &state.vb);
      glBindBuffer(bufferView.target, state.vb);
      std::cout << "buffer.size= " << buffer.data.size()
                << ", byteOffset = " << bufferView.byteOffset << std::endl;

      if (sparse_accessor < 0)
        glBufferData(bufferView.target, bufferView.byteLength,
            &buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
      else {
        std::cout << "TODO: support sparse_accessor" << std::endl;
      }

      glBindBuffer(bufferView.target, 0);

      glBufferState[i] = state;
    }
  }

  glUseProgram(progId);

  glProgramState.attribs["POSITION"] = glGetAttribLocation(progId, "in_vertex");
  glProgramState.attribs["NORMAL"] = glGetAttribLocation(progId, "in_normal");
  glProgramState.attribs["TEXCOORD_0"] =
      glGetAttribLocation(progId, "in_texcoord");
};

static void
drawMesh(tinygltf::Model &model, const tinygltf::Mesh &mesh)
{
  for (size_t i = 0; i < mesh.primitives.size(); i++) {
    const tinygltf::Primitive &primitive = mesh.primitives[i];

    if (primitive.indices < 0) return;

    std::map<std::string, int>::const_iterator it(primitive.attributes.begin());
    std::map<std::string, int>::const_iterator itEnd(
        primitive.attributes.end());

    for (auto [attribute, index] : primitive.attributes) {
      assert(index >= 0);
      const tinygltf::Accessor &accessor = model.accessors[index];
      glBindBuffer(GL_ARRAY_BUFFER, glBufferState[accessor.bufferView].vb);
      checkErrors("bind buffer");
      int size = 1;
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
      if ((attribute == "POSITION") || (attribute == "NORMAL") ||
          (attribute == "TEXCOORD_0")) {
        if (glProgramState.attribs[attribute] >= 0) {
          // compute byteStride from accessor + bufferView.
          int byteStride =
              accessor.ByteStride(model.bufferViews[accessor.bufferView]);
          assert(byteStride != -1);
          glVertexAttribPointer(glProgramState.attribs[attribute], size,
              accessor.componentType, accessor.normalized ? GL_TRUE : GL_FALSE,
              byteStride, BUFFER_OFFSET(accessor.byteOffset));
          checkErrors("vertex attrib pointer");
          glEnableVertexAttribArray(glProgramState.attribs[attribute]);
          checkErrors("enable vertex attrib array");
        }
      }
    }

    const tinygltf::Accessor &indexAccessor =
        model.accessors[primitive.indices];
    glBindBuffer(
        GL_ELEMENT_ARRAY_BUFFER, glBufferState[indexAccessor.bufferView].vb);
    checkErrors("bind buffer");
    int mode = -1;
    if (primitive.mode == TINYGLTF_MODE_TRIANGLES) {
      mode = GL_TRIANGLES;
    } else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP) {
      mode = GL_TRIANGLE_STRIP;
    } else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_FAN) {
      mode = GL_TRIANGLE_FAN;
    } else if (primitive.mode == TINYGLTF_MODE_POINTS) {
      mode = GL_POINTS;
    } else if (primitive.mode == TINYGLTF_MODE_LINE) {
      mode = GL_LINES;
    } else if (primitive.mode == TINYGLTF_MODE_LINE_LOOP) {
      mode = GL_LINE_LOOP;
    } else {
      assert(0);
    }
    glDrawElements(mode, indexAccessor.count, indexAccessor.componentType,
        BUFFER_OFFSET(indexAccessor.byteOffset));
    checkErrors("draw elements");

    {
      for (auto [attribute, _] : primitive.attributes) {
        if (attribute == "POSITION" || attribute == "NORMAL" ||
            attribute == "TEXCOORD_0") {
          if (glProgramState.attribs[attribute] >= 0) {
            glDisableVertexAttribArray(glProgramState.attribs[attribute]);
          }
        }
      }
    }
  }
}

// Hierarchically draw nodes
static void
drawNode(tinygltf::Model &model, const tinygltf::Node &node)
{
  glPushMatrix();
  if (node.matrix.size() == 16) {
    // ModelView行列に相対位置をかける
    glMultMatrixd(node.matrix.data());
  } else {
    // Trans x Rotate x Scale
    if (node.translation.size() == 3) {
      std::cout << "TODO: support node translation matrix" << std::endl;
      // glTranslated(
      // node.translation[0], node.translation[1], node.translation[2]);
    }

    if (node.rotation.size() == 4) {
      std::cout << "TODO: support node rotation matrix" << std::endl;
      // glRotated();
    }

    if (node.scale.size() == 3) {
      std::cout << "TODO: support node scale matrix" << std::endl;
      // glScaled(node.scale[0], node.scale[1], node.scale[2]);
    }
  }

  if (node.mesh > -1) {
    assert(node.mesh < (int)model.meshes.size());
    drawMesh(model, model.meshes[node.mesh]);
  }

  for (size_t i = 0; i < node.children.size(); i++) {
    assert(node.children[i] < (int)model.nodes.size());
    drawNode(model, model.nodes[node.children[i]]);
  }

  glPopMatrix();
}

static void
drawModel(tinygltf::Model &model)
{
  assert(model.scenes.size() > 0);

  int scene_to_display = model.defaultScene > -1 ? model.defaultScene : 0;
  const tinygltf::Scene &scene = model.scenes[scene_to_display];
  for (size_t i = 0; i < scene.nodes.size(); i++) {
    drawNode(model, model.nodes[scene.nodes[i]]);
  }
}

int
main(int argc, char **argv)
{
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  if (argc < 2) {
    std::cout << argv[0] << " "
              << "<model path>.gltf <binary path>.bin" << std::endl;
    return EXIT_FAILURE;
  }

  std::string filename(argv[1]);
  std::string ext = getFilePathExtension(filename);

  bool ret = false;
  if (ext.compare("glb") == 0) {
    ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename.c_str());
  } else {
    ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename.c_str());
  }

  if (!warn.empty()) {
    printf("Warn: %s\n", warn.c_str());
  }

  if (!err.empty()) {
    printf("ERR: %s\n", err.c_str());
    return EXIT_FAILURE;
  }
  if (!ret) {
    printf("Failed to load .glTF : %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  {
    eye[0] = 0.0f;
    eye[1] = 0.0f;
    eye[2] = CAM_Z;

    lookat[0] = 0.0f;
    lookat[1] = 0.0f;
    lookat[2] = 0.0f;

    up[0] = 0.0f;
    up[1] = 1.0f;
    up[2] = 0.0f;
  }

  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW." << std::endl;
    return EXIT_FAILURE;
  }

  window = glfwCreateWindow(width, height, "glTF Viewer", NULL, NULL);
  if (window == NULL) {
    std::cerr << "Failed to open GLFW window. " << std::endl;
    glfwTerminate();
    return EXIT_FAILURE;
  }

  glfwGetWindowSize(window, &width, &height);

  glfwMakeContextCurrent(window);

  glfwSetMouseButtonCallback(window, pointerButtonHandler);
  glfwSetCursorPosCallback(window, pointerMotionHandler);

  glewExperimental = true;
  if (glewInit() != GLEW_OK) {
    std::cerr << "Failed to initialize GLEW." << std::endl;
    return EXIT_FAILURE;
  }

  GLuint programId = 0, vertexId = 0, fragmentId = 0;

  const char *shader_frag_filename = "shader.frag";
  const char *shader_vert_filename = "shader.vert";

  if (!loadShader(GL_VERTEX_SHADER, vertexId, shader_vert_filename))
    return EXIT_FAILURE;
  checkErrors("load vert shader");

  if (!loadShader(GL_FRAGMENT_SHADER, fragmentId, shader_frag_filename))
    return EXIT_FAILURE;
  checkErrors("load frag shader");

  if (!linkShader(programId, vertexId, fragmentId)) return EXIT_FAILURE;
  checkErrors("link");

  {
    GLint vtxLoc = glGetAttribLocation(programId, "in_vertex");
    if (vtxLoc < 0) {
      printf("in_vertex loc not found.\n");
      return EXIT_FAILURE;
    }
  }

  glUseProgram(programId);
  checkErrors("useProgram");

  setupBuffer(model, programId);
  checkErrors("setupBuffer");

  while (glfwWindowShouldClose(window) == GL_FALSE) {
    glfwPollEvents();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);

    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (float)width / (float)height, 0.1f, 1000.0f);
    gluLookAt(eye[0], eye[1], eye[2], lookat[0], lookat[1], lookat[2], up[0],
        up[1], up[2]);
    glPushMatrix();

    glMatrixMode(GL_MODELVIEW);
    drawModel(model);  // Push, Pop

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glFlush();
    glfwSwapBuffers(window);
  }

  glfwTerminate();
}
