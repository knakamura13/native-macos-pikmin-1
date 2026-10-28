// The two symbols the game's loaders (parameters.cpp, cmdStream.cpp) need from
// System that a host test cannot link: DEFINE_ERROR calls System::halt and
// DEFINE_PRINT reads sysCon. Both live in sysDolphin/system.cpp, which drags in
// the whole engine. halt records the message so a test can assert on it.
#include "Colour.h"
#include "system.h"

#include <string>

Stream* sysCon = nullptr;

std::string g_haltMessage;

void System::halt(immut char* file, int line, immut char* message)
{
	g_haltMessage = std::string(file) + ":" + std::to_string(line) + ": " + message;
}

// Parm<Colour>::write in parameters.cpp references this (sysCommon/graphics.cpp);
// the readers under test never call it.
void Colour::write(Stream&) { }
