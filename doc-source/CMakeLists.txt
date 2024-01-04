project(doc-source)

if(MSVC)
	set(doc-source_PLATFORM_DEPS
		w32-pthreads)
endif()

set(doc-source_SOURCES
	doc-source.c)

if(WIN32)
	set(MODULE_DESCRIPTION "OBS document module")
	#configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in doc-source.rc)
	list(APPEND doc-source_SOURCES)
endif()

add_library(doc-source MODULE
	${doc-source_SOURCES})
target_link_libraries(doc-source
	libobs
	${doc-source_PLATFORM_DEPS})
set_target_properties(doc-source PROPERTIES FOLDER "plugins")


if(WIN32)
	install_obs_plugin_with_data(doc-source data)
else()
	setup_plugin_target(doc-source)
endif(WIN32)
