set(srcs)
set(includes)


list(APPEND srcs "main.c" )

idf_component_register( SRCS ${srcs}
                        INCLUDE_DIRS ${includes}
                        PRIV_REQUIRES "freertos" "tinyusb" "driver" )

