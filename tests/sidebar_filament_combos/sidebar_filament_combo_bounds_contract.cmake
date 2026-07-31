## Guards the empty-vector crash fixed in 95fd064c0.
##
## Sidebar::on_filament_count_change() dereferences the FIRST filament combo:
##
##     choices[0]->GetDropDown().Invalidate();
##
## combos_filament can legitimately be EMPTY when that line is reached, by two
## routes, neither of them exotic:
##
##   1. remove_unused_filament_combos(n) pops combos_filament down to n with no
##      floor of one. on_filaments_delete() calls it with
##      combos_filament.size() - 1, so deleting the last filament passes 0 and
##      empties the vector outright.
##   2. on_filament_count_change() itself calls it with num_physical, and
##      num_physical is 0 whenever every slot is a mixed filament, because
##      physical_indices only collects non-mixed slots.
##
## Either way the next call in with a single physical filament clears the
## `num_physical == choices.size()` early-out (0 != 1), reaches the line above
## with num_physical == 1 true, and reads [0] of an empty vector: a garbage
## pointer dereferenced immediately by ->GetDropDown(). In a Release build that
## is an access violation, and it happens on project load, which is exactly when
## filament counts change.
##
## This is asserted against the shipped C++ rather than a screenshot because an
## access violation has no visible surface to photograph, and because the crash
## needs a filament-count sequence that no static capture can express.
##
## Mutation-checked: delete the emptiness guard and this test fails.

if(NOT DEFINED BAMBU_SOURCE_DIR)
    message(FATAL_ERROR "BAMBU_SOURCE_DIR is required")
endif()

set(plater_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Plater.cpp")
file(READ "${plater_cpp}" plater_source)

# 1. Every choices[0] dereference in the sidebar must be preceded by a guard
#    that proves the container is non-empty. Two call sites exist:
#      - on_filament_count_change(): guarded by !choices.empty()
#      - on_filaments_delete():      guarded by filament_id < choices.size()
string(REGEX MATCHALL "choices\\[0\\]->GetDropDown" combo_derefs "${plater_source}")
list(LENGTH combo_derefs combo_deref_count)
if(NOT combo_deref_count EQUAL 2)
    message(FATAL_ERROR
        "Expected exactly 2 'choices[0]->GetDropDown' call sites in Plater.cpp, found "
        "${combo_deref_count}. A new one has appeared or an existing one moved: it must be "
        "proven non-empty before dereferencing, then this count updated.")
endif()

# 2. The on_filament_count_change() site specifically. num_physical == 1 is true
#    while choices is empty, so the size()==1 / num_physical==1 test alone is NOT
#    sufficient - the emptiness check has to be there too.
if(NOT plater_source MATCHES "!choices\\.empty\\(\\)[ \t]*&&[ \t]*\\(choices\\.size\\(\\) == 1 \\|\\| num_physical == 1\\)")
    message(FATAL_ERROR
        "Sidebar::on_filament_count_change() must guard choices[0] with !choices.empty(). "
        "Without it, an empty combos_filament (deleting the last filament, or a project whose "
        "slots are all mixed filaments) makes the next single-filament count change read [0] of "
        "an empty vector and dereference garbage - an access violation on project load.")
endif()

# 3. remove_unused_filament_combos() is what empties the vector; if it ever grows
#    a floor of one, the guard above stops being the only thing standing between
#    a project load and an access violation, and this contract should be revisited
#    deliberately rather than silently.
if(NOT plater_source MATCHES "while \\(p->combos_filament\\.size\\(\\) > current_extruder_count\\)")
    message(FATAL_ERROR
        "remove_unused_filament_combos() no longer pops with a plain "
        "'while (size > current_extruder_count)' loop. It is the source of the empty "
        "combos_filament this contract exists for - re-check the choices[0] guards.")
endif()

message(STATUS "Sidebar filament combo bounds contract passed.")
