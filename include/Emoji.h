#pragma once
// Emoji on the screens: 12x12 pixel art of about 300 common emoji (EmojiData.h, tools/emoji.py; Twemoji,
// CC-BY 4.0). Colour screens draw the palette colours; monochrome ones light the main colour group.
#include <stdint.h>
#include "EmojiData.h"
namespace emoji {
// Index of the code point in the set, or -1.
int find(uint32_t cp);
// The picture, row by row: 0 transparent, else palette index + 1. False if the data is damaged.
bool decode(int index,uint8_t px[Size*Size]);
// Monochrome: 1 for the lit pixels. Two groups of clearly different brightness (a face and its eyes, a
// sign and its mark): the larger one lights; one group (a heart, a black circle) lights whole.
void mono(uint8_t px[Size*Size]);
// Marks that only change the emoji before them (variation selectors, skin tones, joiner, keycap, tags).
inline bool modifier(uint32_t cp){return cp==0xfe0f||cp==0xfe0e||cp==0x200d||cp==0x20e3||(cp>=0x1f3fb&&cp<=0x1f3ff)||(cp>=0xe0020&&cp<=0xe007f);}
// A regional indicator (two of them make a flag) is drawn as its letter: "RU".
inline char flagLetter(uint32_t cp){return cp>=0x1f1e6&&cp<=0x1f1ff?char('A'+(cp-0x1f1e6)):0;}
// Emoji blocks without a picture here: a placeholder keeps them visible.
inline bool pictograph(uint32_t cp){return (cp>=0x1f000&&cp<=0x1faff)||(cp>=0x2600&&cp<=0x27bf)||(cp>=0x2b00&&cp<=0x2bff);}
// Draws the decoded picture at size s (nearest pixel), top left at x,y: plot(x,y,value) for each value != 0.
template<class Plot> void draw(const uint8_t* px,int x,int y,int s,Plot plot){
 for(int dy=0;dy<s;dy++){const uint8_t* row=px+(dy*Size/s)*Size;for(int dx=0;dx<s;dx++){uint8_t v=row[dx*Size/s];if(v)plot(x+dx,y+dy,v);}}
}
}
