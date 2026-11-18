# Third-party components

| Component | License | Use |
|---|---|---|
| GLFW 3.4 | zlib | windows, input, OpenGL context |
| Dear ImGui (docking branch) with its GLFW and OpenGL 3 backends | MIT | user interface |
| stb (`stb_image_write.h`) | MIT / public domain | writing PNG files (screenshots; later textures) |
| Roboto (Medium), via Dear ImGui `misc/fonts` | Apache License 2.0 | UI font, embedded in the app |

All are fetched at build time at pinned versions (see `cmake/Deps.cmake`).
