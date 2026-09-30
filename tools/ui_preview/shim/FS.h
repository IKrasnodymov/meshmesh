#pragma once
#include "Arduino.h"
class File{public:explicit operator bool() const{return false;}};
namespace fs{class FS{};}
