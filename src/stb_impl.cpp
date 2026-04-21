// Single translation unit that provides the implementations of the stb
// public-domain single-header libraries.
//
//   stb_image.h       -> PNG/JPG/BMP/TGA/GIF/HDR/PSD/PIC/PNM decoders
//   stb_image_write.h -> PNG/BMP/TGA/JPG/HDR encoders
//
// All other translation units include the headers normally and pick up the
// declarations only.

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
