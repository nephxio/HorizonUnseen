// Test entry point.
//
// Catch2 supplies a main() of its own, but the systems under test log
// enthusiastically -- EnergyCellSystem alone narrates every charge, break and
// repair. Left at the default level that buries the test output, so the level
// is raised to Error before any test runs. Log::init() is deliberately not
// called: without it nothing touches the filesystem, so running the suite does
// not scatter logs/ directories through the build tree.

#include "Core/Log.h"

#include <catch2/catch_session.hpp>

int main(int argc, char* argv[]) {
    hu::Log::setLevel(hu::LogLevel::Error);
    return Catch::Session().run(argc, argv);
}
