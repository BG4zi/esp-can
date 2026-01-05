file(GLOB_RECURSE srcs *.c
                    src/*.c
                    )


set(include_dirs 
    . 
    include
    )

idf_component_register( SRCS ${srcs}
                        INCLUDE_DIRS ${include_dirs}
                        )

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB lz4)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
