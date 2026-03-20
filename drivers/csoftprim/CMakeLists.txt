project(csoftprim C)

add_library(csoftprim
        device.c
        device.h
        dither.c
        driver.c
        drv.h
        drv_ip.h
        frcp.c
        l_piz.c
        match.c
        match.h
        object.c
        object.h
        plib.c
        plib.h
        p_piz.c
        pstate.c
        pstate.h
        rastparm.h
        rcp.c
        safediv.c
        sbuffer.c
        sbuffer.h
        template.h
        timestmp.h
        tri_render.c
        work.c
        work.h
)

target_compile_definitions(csoftprim PRIVATE "-DPARTS=0xffff")

get_target_property(target_type csoftprim TYPE)
if(NOT target_type STREQUAL SHARED_LIBRARY)
    target_compile_definitions(csoftprim PRIVATE -DDEFINE_BR_ENTRY_POINT)
endif()

target_link_libraries(csoftprim PRIVATE BRender::DDI)

set_target_properties(csoftprim PROPERTIES
		EXPORT_NAME "Drivers::SoftPrim"
		FOLDER "BRender/Drivers"
		OUTPUT_NAME "softprmf"
		)

add_library(BRender::Drivers::SoftPrim ALIAS csoftprim)
