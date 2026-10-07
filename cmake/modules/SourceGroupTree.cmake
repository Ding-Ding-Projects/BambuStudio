# Groups a target's sources for IDE generators by their folder under ROOT.
#
# source_group(TREE <root>) stops the configure step on every generator,
# Ninja included, when any listed file lies outside <root>. Target source
# lists may still name shared sources from a sibling directory, such as the
# ../libslic3r service sources compiled into libslic3r_gui. Files outside
# ROOT are grouped by their folder under OUTSIDE_ROOT, below OUTSIDE_PREFIX.
# Files outside both roots go into a flat OUTSIDE_PREFIX group.
#
#   bambu_source_group_tree(ROOT <dir> OUTSIDE_ROOT <dir> OUTSIDE_PREFIX <name>
#                           FILES <file>...)
#
# Relative files resolve against CMAKE_CURRENT_SOURCE_DIR, as for
# add_library().
function(bambu_source_group_tree)
    cmake_parse_arguments(PARSE_ARGV 0 _bambu_sg "" "ROOT;OUTSIDE_ROOT;OUTSIDE_PREFIX" "FILES")
    if(NOT _bambu_sg_ROOT OR NOT _bambu_sg_OUTSIDE_ROOT OR NOT _bambu_sg_OUTSIDE_PREFIX)
        message(FATAL_ERROR "bambu_source_group_tree needs ROOT, OUTSIDE_ROOT and OUTSIDE_PREFIX")
    endif()
    get_filename_component(_root "${_bambu_sg_ROOT}" ABSOLUTE)
    get_filename_component(_outside_root "${_bambu_sg_OUTSIDE_ROOT}" ABSOLUTE)
    set(_inside)
    set(_outside)
    set(_elsewhere)
    foreach(_file IN LISTS _bambu_sg_FILES)
        get_filename_component(_absolute "${_file}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        string(FIND "${_absolute}" "${_root}/" _root_at)
        string(FIND "${_absolute}" "${_outside_root}/" _outside_at)
        if(_root_at EQUAL 0)
            list(APPEND _inside "${_absolute}")
        elseif(_outside_at EQUAL 0)
            list(APPEND _outside "${_absolute}")
        else()
            list(APPEND _elsewhere "${_absolute}")
        endif()
    endforeach()
    if(_inside)
        source_group(TREE "${_root}" FILES ${_inside})
    endif()
    if(_outside)
        source_group(TREE "${_outside_root}" PREFIX "${_bambu_sg_OUTSIDE_PREFIX}" FILES ${_outside})
    endif()
    if(_elsewhere)
        source_group("${_bambu_sg_OUTSIDE_PREFIX}" FILES ${_elsewhere})
    endif()
endfunction()
