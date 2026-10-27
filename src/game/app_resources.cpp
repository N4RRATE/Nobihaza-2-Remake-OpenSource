#include "app_resources.h"
#include <stdexcept>

std::filesystem::path bundledContentPath(){
    throw std::runtime_error("Default application resources are not supported on this platform. Usage: nobihaza <scene.json|combat.json> [--entry name] [--frames N] [--save-dir path]");
}
