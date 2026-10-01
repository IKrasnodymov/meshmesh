#pragma once
#include <stdint.h>
// Only the build hash: SHA-256 of the application image as written to flash (the .bin of the package).
struct esp_app_desc_t {uint8_t app_elf_sha256[32];};
const esp_app_desc_t* esp_ota_get_app_description();
