project(
  'gltf-viewer', 'c', 'cpp',
  version: '0.0.1-dev',
  license: 'MIT',
  meson_version: '>= 0.58.0',
  default_options: [ 'warning_level=1', 'werror=false', 'optimization=2', 'c_std=gnu11', 'cpp_std=gnu++17' ],
)

public_inc = include_directories('include')

dep_glfw3 = dependency('glfw3')
dep_glew = dependency('glew')

viewer_src = [
  'main.cc',
  'include/tiny_gltf.cc',
]

viewer_dep = [
  dep_glfw3,
  dep_glew,
]

executable(
  'gltf-viewer',
  viewer_src,
  install: false,
  dependencies: viewer_dep,
  include_directories: public_inc,
)
