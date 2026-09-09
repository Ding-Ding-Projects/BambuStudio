# BundleDocs.cmake - build-time documentation bundle for the in-app browser.
#
# Usage (script mode, no Node or Python required):
#   cmake -DDOCS_SOURCE=<repo>/docs -DDOCS_OUTPUT=<repo>/resources/docs -P BundleDocs.cmake
#
# Reads every docs/features/**/*.md (category README.md indexes included) and
# writes:
#   <DOCS_OUTPUT>/bundle.json   {"version":1, "articles":[{path,category,index,title,body}...]}
#   <DOCS_OUTPUT>/assets/...    every image an article references, mirrored at
#                               its docs/-relative path so relative links keep
#                               resolving after bundling.
#
# The output is deterministic (sorted paths, fixed escaping) so the generated
# snapshot is committed and diffable; tests/docs_bundle re-derives the same
# view straight from the tree and fails when the committed bundle is stale.

cmake_minimum_required(VERSION 3.13)

if (NOT DEFINED DOCS_SOURCE OR NOT DEFINED DOCS_OUTPUT)
    message(FATAL_ERROR "BundleDocs.cmake needs -DDOCS_SOURCE=<repo>/docs and -DDOCS_OUTPUT=<out dir>")
endif()

get_filename_component(DOCS_SOURCE "${DOCS_SOURCE}" ABSOLUTE)
get_filename_component(DOCS_OUTPUT "${DOCS_OUTPUT}" ABSOLUTE)
set(FEATURES_DIR "${DOCS_SOURCE}/features")
if (NOT IS_DIRECTORY "${FEATURES_DIR}")
    message(FATAL_ERROR "BundleDocs: ${FEATURES_DIR} is not a directory")
endif()

# JSON string escaping for the subset that Markdown files can contain.
function(docs_json_escape input output_var)
    string(REPLACE "\\" "\\\\" input "${input}")
    string(REPLACE "\"" "\\\"" input "${input}")
    string(REPLACE "\r" "" input "${input}")
    string(REPLACE "\n" "\\n" input "${input}")
    string(REPLACE "\t" "\\t" input "${input}")
    set(${output_var} "${input}" PARENT_SCOPE)
endfunction()

# First "# " heading of a Markdown body (the article title), or "".
function(docs_first_heading body output_var)
    string(REGEX MATCH "(^|\n)#[ \t]+([^\n]*)" _m "${body}")
    set(title "${CMAKE_MATCH_2}")
    string(REGEX REPLACE "[ \t]+#+[ \t]*$" "" title "${title}")
    string(STRIP "${title}" title)
    set(${output_var} "${title}" PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE MD_FILES LIST_DIRECTORIES false RELATIVE "${DOCS_SOURCE}" "${FEATURES_DIR}/*.md")
list(SORT MD_FILES)

set(json "{\n  \"version\": 1,\n  \"source\": \"docs/features\",\n  \"articles\": [\n")
set(first TRUE)
set(copied_assets "")

foreach (rel IN LISTS MD_FILES)
    file(READ "${DOCS_SOURCE}/${rel}" body)
    string(REPLACE "\r\n" "\n" body "${body}")

    docs_first_heading("${body}" title)
    get_filename_component(dir "${rel}" DIRECTORY)        # features/<category>
    get_filename_component(category "${dir}" NAME)
    get_filename_component(name "${rel}" NAME)
    if (name STREQUAL "README.md")
        set(index "true")
    else()
        set(index "false")
    endif()

    # Mirror referenced images: ![alt](relative/path.png "title").
    string(REGEX MATCHALL "!\\[[^]]*\\]\\([^) \t]+" refs "${body}")
    foreach (ref IN LISTS refs)
        string(REGEX REPLACE "^!\\[[^]]*\\]\\(" "" ref "${ref}")
        if (ref MATCHES "^[A-Za-z][A-Za-z0-9+.-]*:")
            continue() # absolute URL: not a bundled asset
        endif()
        get_filename_component(abs "${DOCS_SOURCE}/${dir}/${ref}" ABSOLUTE)
        string(FIND "${abs}" "${DOCS_SOURCE}/" prefix_pos)
        if (NOT prefix_pos EQUAL 0)
            message(WARNING "BundleDocs: ${rel} references ${ref} outside docs/; not bundled")
            continue()
        endif()
        if (NOT EXISTS "${abs}")
            message(WARNING "BundleDocs: ${rel} references missing image ${ref}")
            continue()
        endif()
        file(RELATIVE_PATH asset_rel "${DOCS_SOURCE}" "${abs}")
        list(APPEND copied_assets "${asset_rel}")
    endforeach()

    docs_json_escape("${rel}" j_path)
    docs_json_escape("${title}" j_title)
    docs_json_escape("${body}" j_body)
    if (NOT first)
        string(APPEND json ",\n")
    endif()
    set(first FALSE)
    string(APPEND json "    {\n      \"path\": \"docs/${j_path}\",\n      \"category\": \"${category}\",\n      \"index\": ${index},\n      \"title\": \"${j_title}\",\n      \"body\": \"${j_body}\"\n    }")
endforeach()

string(APPEND json "\n  ]\n}\n")

file(MAKE_DIRECTORY "${DOCS_OUTPUT}")
# Only rewrite when the content changed so an unchanged tree stays clean.
set(bundle_path "${DOCS_OUTPUT}/bundle.json")
set(existing "")
if (EXISTS "${bundle_path}")
    file(READ "${bundle_path}" existing)
endif()
if (NOT existing STREQUAL json)
    file(WRITE "${bundle_path}" "${json}")
    message(STATUS "BundleDocs: wrote ${bundle_path}")
endif()

list(REMOVE_DUPLICATES copied_assets)
list(SORT copied_assets)
foreach (asset_rel IN LISTS copied_assets)
    get_filename_component(asset_dir "${DOCS_OUTPUT}/assets/${asset_rel}" DIRECTORY)
    file(MAKE_DIRECTORY "${asset_dir}")
    configure_file("${DOCS_SOURCE}/${asset_rel}" "${DOCS_OUTPUT}/assets/${asset_rel}" COPYONLY)
endforeach()

list(LENGTH MD_FILES article_count)
list(LENGTH copied_assets asset_count)
message(STATUS "BundleDocs: ${article_count} articles, ${asset_count} assets -> ${DOCS_OUTPUT}")
