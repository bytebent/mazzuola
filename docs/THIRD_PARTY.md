# Third-party components

| Component | License | Use |
|---|---|---|
| GLFW 3.4 | zlib | windows, input, OpenGL context |
| Dear ImGui (docking branch) with its GLFW and OpenGL 3 backends | MIT | user interface |
| stb (`stb_image_write.h`, `stb_image.h`) | MIT / public domain | writing PNG files (screenshots; later textures); reading reference images (PNG, JPG, TGA, BMP) |
| cgltf 1.15 (`cgltf.h`, `cgltf_write.h`) | MIT | writing `.glb` / `.gltf` (and reading them back in tests); vendored in `third_party/cgltf` |
| Roboto (Medium), via Dear ImGui `misc/fonts` | Apache License 2.0 | UI font, embedded in the app |

All but cgltf are fetched at build time at pinned versions (see `cmake/Deps.cmake`).
