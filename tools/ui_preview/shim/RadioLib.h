#pragma once
#include "SPI.h"
// Radio type placeholders: the preview never touches a transceiver.
class Module{public:Module(int,int,int,int,SPIClass&){}};
class LR1110{public:explicit LR1110(Module*){}};
class SX1262{public:explicit SX1262(Module*){}};
