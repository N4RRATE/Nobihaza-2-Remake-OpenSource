#pragma once
#include <filesystem>

// The application owns read-only resources; SaveFiles owns user data separately.
std::filesystem::path bundledContentPath();
