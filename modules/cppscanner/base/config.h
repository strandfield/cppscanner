// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#pragma once

namespace cppscanner
{

// This is the file extension for the snapshots produced by the plugin.
constexpr const char* PLUGIN_SNAPSHOT_EXTENSION = ".aba";

// This is the name of the directory that is created inside the plugin's output
// directory and which will contain all the snapshots created by the plugin.
constexpr const char* PLUGIN_OUTPUT_FOLDER_NAME = ".cppscanner";

// This environment variable is used by the plugin to enable 
// the indexing of local symbols.
// If the variable does not exist, or is zero ("0"), local symbols are 
// not indexed (which is the default behavior).
constexpr const char* ENV_INDEX_LOCAL_SYMBOLS = "CPPSCANNER_INDEX_LOCAL_SYMBOLS";

// This environment variable is used by the plugin and the scanner's 'merge' command
// to initialize the snapshot's home directory, if it isn't otherwise specified.
// Every snapshot must provide a home directory as it is used to decide whether a file
// belongs to the project or not.
constexpr const char* ENV_HOME_DIR = "CPPSCANNER_HOME_DIR";

// This environment variable is used by the plugin to initialize the output
// directory if none is provided as a plugin argument.
constexpr const char* ENV_PLUGIN_OUTPUT_DIR = "CPPSCANNER_OUTPUT_DIR";

} // namespace cppscanner
