#ifndef slic3r_GUI_Init_hpp_
#define slic3r_GUI_Init_hpp_

#include <string>
#include <vector>

#include <libslic3r/Preset.hpp>
#include <libslic3r/PrintConfig.hpp>

namespace Slic3r {

namespace GUI {

struct GUI_InitParams
{
	int		                    argc;
	char	                  **argv;

	// Substitutions of unknown configuration values done during loading of user presets.
	PresetsConfigSubstitutions  preset_substitutions;

    std::vector<std::string>    load_configs;
    DynamicPrintConfig          extra_config;
    std::vector<std::string>    input_files;

    //BBS: remove start_as_gcodeviewer logic
	//bool	                    start_as_gcodeviewer;
	bool                        input_gcode { false };

    // A warning found during the command-line setup, before any window exists
    // (for example a blacklisted library injected into the process). The main
    // window shows it with the Material message dialog once it is up; empty
    // when there is nothing to report.
    std::wstring                startup_warning;
};

int GUI_Run(GUI_InitParams &params);

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_GUI_Init_hpp_
