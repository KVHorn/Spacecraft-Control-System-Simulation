// This file exists solely to compile tinygltf's implementation in one
// translation unit. Do not add anything else here.
//
// tinygltf will internally define STB_IMAGE_IMPLEMENTATION, so to avoid
// multiple-definition errors with our existing stb_image_impl.cpp, we tell it
// to skip including stb_image altogether and reuse ours (TINYGLTF_NO_INCLUDE_STB_IMAGE).

#include <stb_image.h>                // declarations only; implementation in stb_image_impl.cpp

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE   // we don't write images
#include <tiny_gltf.h>
