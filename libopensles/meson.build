opensles_util = static_library(
	'opensles_util',
	[
		'OpenSLESUT.c'
	],
	gnu_symbol_visibility: 'hidden',
)

opensles_helper = static_library(
	'opensles_helper',
	[
		'MPH_to.c'
	],
	c_args: [
		'-DANDROID_EXT',
		'-Wno-override-init',
		'-Wno-missing-field-initializers'
	]
)

libopensles_files = files([
	'OpenSLES_IID.c',
	'classes.c',
	'devices.c',
	'trace.c',
	'locks.c',
	'sles.c',
	'sllog.c',
	'SDL.c',
	'SndFile.c',
	'sync.c',
	'IID_to_MPH.c',
	'ThreadPool.c',
	'C3DGroup.c',
	'CAudioPlayer.c',
	'CAudioRecorder.c',
	'CEngine.c',
	'COutputMix.c',
	'IOutputMixExt.c',
	'IAndroidConfiguration.c',
	'IBassBoost.c',
	'IBufferQueue.c',
	'IEffectSend.c',
	'IEngine.c',
	'IEnvironmentalReverb.c',
	'IEqualizer.c',
	'IMuteSolo.c',
	'IObject.c',
	'IPlay.c',
	'IPlaybackRate.c',
	'IPrefetchStatus.c',
	'IPresetReverb.c',
	'IRecord.c',
	'ISeek.c',
	'IVirtualizer.c',
	'IVolume.c'
])
