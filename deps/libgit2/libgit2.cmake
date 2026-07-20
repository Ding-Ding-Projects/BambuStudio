# libgit2 is used only for Bambu Studio's local project-history repositories.
# Pin the release archive by both version and digest so dependency updates are
# explicit and reproducible. v1.9.3 resolves to commit
# 1affb8b19346c4f90e163a9a0364959ff1410f64.
bambustudio_add_cmake_project(libgit2
    URL "https://github.com/libgit2/libgit2/archive/refs/tags/v1.9.3.tar.gz"
    URL_HASH SHA256=d532172d7ab24d2a25944e2434212d63ee85f3650e97b5f7579e7f201a78ad64
    CMAKE_ARGS
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
        -DBUILD_TESTS:BOOL=OFF
        -DBUILD_CLI:BOOL=OFF
        -DBUILD_EXAMPLES:BOOL=OFF
        -DBUILD_FUZZERS:BOOL=OFF
        -DUSE_SSH:STRING=OFF
        -DUSE_HTTPS:STRING=OFF
        -DUSE_GSSAPI:BOOL=OFF
        -DUSE_NTLMCLIENT:BOOL=OFF
        -DUSE_HTTP_PARSER:STRING=builtin
        -DREGEX_BACKEND:STRING=builtin
        -DUSE_SHA1:STRING=CollisionDetection
        -DUSE_SHA256:STRING=Builtin
        -DUSE_BUNDLED_ZLIB:BOOL=ON
        -DENABLE_REPRODUCIBLE_BUILDS:BOOL=ON
        -DSTATIC_CRT:BOOL=OFF
        -DSONAME:BOOL=OFF
)

if (MSVC)
    add_debug_dep(dep_libgit2)
endif ()
