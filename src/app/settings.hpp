#pragma once
// Where per-user files (window layout, later: recent files, palettes) live.
// Always on the user's machine, never inside the repo.
#include <string>

namespace mazz {

// Returns the settings directory, creating it if needed.
//   macOS: ~/Library/Application Support/Mazzuola
//   other: $XDG_CONFIG_HOME/mazzuola or ~/.config/mazzuola
// Returns an empty string if no home directory is available.
std::string settingsDir();

} // namespace mazz
