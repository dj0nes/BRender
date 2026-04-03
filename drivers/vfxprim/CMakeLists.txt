project(vfxprim C)

add_library(vfxprim
    # Driver boilerplate
    driver.c
    device.c
    object.c
    plib.c
    pstate.c
    match.c
    sbuffer.c
    work.c

    # BRender-to-VFX adapter
    tri_vfx.c

    # VFX polygon engine (vendored from Miles VFX port)
    vfx_port_core.c
    vfx_poly_flat.c
    vfx_poly_gouraud.c
    vfx_poly_map.c
    vfx_poly_xlat.c
    vfx_poly_illum.c
)

target_include_directories(vfxprim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(vfxprim PRIVATE
    BASED_FLOAT=1
)

target_link_libraries(vfxprim PRIVATE BRender::DDI)

set_target_properties(vfxprim PROPERTIES
    EXPORT_NAME "Drivers::VFXPrim"
    FOLDER "BRender/Drivers"
)

add_library(BRender::Drivers::VFXPrim ALIAS vfxprim)
