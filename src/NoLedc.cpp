// Classic ESP32 boards without a buzzer or a dimmed backlight (T-Beam, T-LoRa; MM_NO_LEDC): their IRAM is
// full, and the deep sleep of "Turn off" (Power.cpp) needs about 200 bytes more. Only RadioLib's tone()
// for AFSK, which MeshMesh does not use, reaches the LEDC driver, whose functions sit in IRAM: the linker
// (--wrap in platformio.ini) sends its calls here instead.
#if defined(MM_NO_LEDC)
#include <stdint.h>
extern "C" {
void __wrap_ledcAttachPin(uint8_t,uint8_t){}
void __wrap_ledcDetachPin(uint8_t){}
uint32_t __wrap_ledcWriteTone(uint8_t,uint32_t){return 0;}
void __wrap_ledcWrite(uint8_t,uint32_t){}
}
#endif
