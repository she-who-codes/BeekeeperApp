
#pragma once

#include <string>

// ==================================================
// Paths
// --------------------------------------------------
// Locates the "resources" folder (fonts, locales) no
// matter which folder BeeKeeper was started from.
// Searched in order:
//   1. <working directory>/resources
//   2. <executable folder>/resources
//   3. <executable folder>/../resources
// ==================================================

namespace Paths
{
    // Absolute path of the resources folder, or
    // "resources" if none was found.
    const std::string& Resources();

    // Resources() + "/" + relative, e.g. "fonts/Inter.ttf".
    std::string Resource(const std::string& relative);

    // Folder containing the running executable.
    std::string ExecutableDirectory();
}
