#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>

#include "io/SceneParser.h"
#include "render/Renderer.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <scene.xml> [output.ppm|output.png]\n";
        return 1;
    }

    const std::string scenePath = argv[1];
    const std::string outputPath = (argc >= 3) ? argv[2] : "output.ppm";

    rt::Scene scene;
    rt::SceneParser parser;
    std::string error;

    if (parser.parse(scenePath, scene, error) == false) {
        std::cerr << "Scene parse error: " << error << "\n";
        return 1;
    }

    rt::Renderer renderer(scene);
    rt::Image img = renderer.render();

    std::string lowerPath = outputPath;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    bool ok = false;
    if (lowerPath.size() >= 4 && lowerPath.substr(lowerPath.size() - 4) == ".png") {
        ok = img.savePNG(outputPath);
    } else {
        ok = img.savePPM(outputPath);
    }

    if (ok == false) {
        std::cerr << "Failed to save output image: " << outputPath << "\n";
        return 1;
    }

    std::cout << "Render complete: " << outputPath << "\n";
    return 0;
}
