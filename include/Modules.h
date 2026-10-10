#pragma once
// Optional modules of the nRF52 images (1 MB flash): the owner chooses them before installing,
// tools/nrf52.py package ENV --without chess,pet,dice,wardrive builds with MM_NO_CHESS, MM_NO_PET, MM_NO_DICE,
// MM_NO_WARDRIVE.
// A module left out keeps a small stand-in (its USB command answers "ERR ... is not in this build",
// the radio passes it nothing); its screens and menu items are not built. status lists the modules
// ("modules"), so the web page and the app hide what is missing.
#if defined(MM_NO_CHESS)
#define MM_CHESS 0
#else
#define MM_CHESS 1
#endif
#if defined(MM_NO_PET)
#define MM_PET 0
#else
#define MM_PET 1
#endif
#if defined(MM_NO_DICE)
#define MM_DICE 0
#else
#define MM_DICE 1
#endif
#if defined(MM_NO_WARDRIVE)
#define MM_WARDRIVE 0
#else
#define MM_WARDRIVE 1
#endif
#if (!MM_CHESS||!MM_PET||!MM_DICE||!MM_WARDRIVE)&&!defined(MM_NRF52)&&!defined(MM_UI_PREVIEW)
#error "Optional modules are built for the nRF52 boards only (tools/nrf52.py package ENV --without ...)"
#endif
