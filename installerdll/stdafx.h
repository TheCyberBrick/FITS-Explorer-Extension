#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <shlobj.h>
#include <strsafe.h>
#include <msiquery.h>

// WiX Header Files:
#include <wcautil.h>
#include <strutil.h>

// Extension
#include <sstream>
#include <string>
#include <tuple>
#include <set>
#include "constants.h"