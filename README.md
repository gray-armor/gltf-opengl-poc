# gltf-opengl-poc

## dependency
- tinyGLTF
- glew
- glfw3

## setup
```
$ git clone git@github.com:gray-armor/gltf-opengl-poc.git
$ cd gltf-opengl-poc
```
```
$ meson build
$ ninja -C build
```

## run
```
$ ./build/gltf-viewer <model path>.gltf <binary path>.bin
```

## examples
```
$ ./build/gltf-viewer ./assets/Cube/Cube.gltf ./assets/Cube/Cube.bin
```
```
$ ./build/gltf-viewer ./assets/Duck/Duck.gltf ./assets/Duck/Duck0.bin
```
