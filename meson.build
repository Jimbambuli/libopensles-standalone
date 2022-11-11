project('libopensles', ['c', 'java'])
# java somehow needed for jni

incdir_dep = declare_dependency(include_directories: 'include')
add_project_dependencies(incdir_dep, language: 'c')

subdir('libopensles')

cc = meson.get_compiler('c')
libm_dep = cc.find_library('m')

libopensles = shared_library(
	'OpenSLES',
	[
		libopensles_files
	],
	gnu_symbol_visibility: 'hidden',
	dependencies: [
		dependency('dl'), dependency('jni'), libm_dep, dependency('threads'), dependency('sdl2'), dependency('sndfile'),
	],
	link_with: [
		opensles_util, opensles_helper
	],
	# note that this implementation isn't compliant with the spec, nonetheless it's the only implementation
	# that anyone writes code against so it should work with any existing code
	# also note that not all android extensions are currently enabled, they will be enabled whenever
	# an app that needs them is identified (so that they can immediately be tested)
	version: '1.0.1',
	c_args: [
		'-DANDROID_EXT', # enable android extensions on non-android build
		'-DUSE_PROFILES=0', # OpenSLES profiles are a thing, but google says this implementation doesn't really care?
		'-DUSE_TRACE', # debug
		'-DUSE_DEBUG', # debug
		'-UNDEBUG', # debug
		'-DUSE_LOG=SLAndroidLogLevel_Verbose', # debug
		'-DSLAPIENTRY=__attribute__((visibility("default")))', # since we use `gnu_symbol_visibility: 'hidden'`, we need to explicity export the functions which constitute the API
		'-DUSE_SDL', # the backend of choice for non-android builds (probably not ideal? but seems to somewhat work)
		'-DUSE_OUTPUTMIXEXT', # used by the SDL backend
		'-DANDROID_SIMULATOR', # not sure, but sounds like something we want
		'-DUSE_SNDFILE', # used by the SDL backend
		'-DUSE_BACKPORT', # gets rid of dependencies on some stuff from AOSP

		'-Wno-multichar'
	]
)
