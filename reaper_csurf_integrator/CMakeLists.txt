file(GLOB_RECURSE sources CONFIGURE_DEPENDS ./*.c* ./*.h*)
target_sources(${PROJECT_NAME}
    PRIVATE
    ${sources}
    )