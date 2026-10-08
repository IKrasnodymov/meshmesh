#pragma once
#include <Arduino.h>
// The MeshCore companion protocol (examples/companion_radio of MeshCore, protocol version 10): the
// stock phone apps talk to this node over Bluetooth (Nordic UART service, one frame per write and per
// notification) or over USB ('<' + 16-bit length + frame in, '>' + length + frame out). The protocol
// runs on the chat node itself (src/Companion.inc in MeshRadio.cpp): contacts, channels and the
// messages are the ones of the screen.
namespace companion {
constexpr size_t MaxFrame=176;
enum Link:uint8_t {LinkNone,LinkBle,LinkUsb};
void command(const uint8_t* frame,size_t length,Link from); // one frame from the app
size_t next(uint8_t out[MaxFrame],Link to);                   // the next frame for that link; 0: none
void disconnected(Link link);
bool connected();                                            // an app spoke to the node and is still linked
}
