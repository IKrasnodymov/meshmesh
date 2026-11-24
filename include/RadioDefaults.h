#pragma once

// Factory / NVS fallback radio profile. EU868 is upstream default; MM_RADIO_DEFAULT_US selects
// the MeshCore USA/Canada preset (902–928 MHz band).
#if defined(MM_RADIO_DEFAULT_US)
#define MM_RADIO_DEFAULT_FREQ 910.525f
#define MM_RADIO_DEFAULT_BW 62.5f
#define MM_RADIO_DEFAULT_SF 7
#define MM_RADIO_DEFAULT_CR 5
#else
#define MM_RADIO_DEFAULT_FREQ 868.731f
#define MM_RADIO_DEFAULT_BW 62.5f
#define MM_RADIO_DEFAULT_SF 8
#define MM_RADIO_DEFAULT_CR 6
#endif