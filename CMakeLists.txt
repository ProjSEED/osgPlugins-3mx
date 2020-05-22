# liblzma
file (GLOB LIBLZMA_SRC ./OpenCTM/liblzma/*.c)
file (GLOB LIBLZMA_H ./OpenCTM/liblzma/*.h)
source_group("liblzma" FILES ${LIBLZMA_SRC} ${LIBLZMA_H} )
include_directories("./OpenCTM/liblzma/")

# openctm
file (GLOB OPENCTM_SRC ./OpenCTM/*.c)
file (GLOB OPENCTM_H ./OpenCTM/*.h)
source_group("openCTM" FILES ${OPENCTM_SRC} ${OPENCTM_H} )
include_directories("./openCTM/")
add_definitions(-DOPENCTM_STATIC)

# cJsonObject
SET(CJSONOBJECT_SRC 
	./cJsonObject/CJsonObject.cpp
	./cJsonObject/cJSON.c
)

SET(CJSONOBJECT_H
	./cJsonObject/CJsonObject.hpp
    ./cJsonObject/cJSON.h
)
source_group("cJsonObject" FILES ${CJSONOBJECT_SRC} ${CJSONOBJECT_H} )
include_directories("./cJsonObject/")

# 3mx
SET(TARGET_SRC 
	ReaderWriter3MXB.cpp 
	${CJSONOBJECT_SRC}
	${LIBLZMA_SRC}
	${OPENCTM_SRC}
)

SET(TARGET_H
	${CJSONOBJECT_H}
	${LIBLZMA_H}
	${OPENCTM_H}
)

#### end var setup  ###
SETUP_PLUGIN(3mxb)



