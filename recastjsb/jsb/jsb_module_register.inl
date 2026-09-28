// Copy into the Cocos native project's jsb_module_register.cpp

// include:
#include "jsb_recast.h"

// inside the existing module registration callback, `ns` is the global object:
register_all_recastjsb(ns);
