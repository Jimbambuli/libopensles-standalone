### libOpenSLES

this repository contains a lightly patched version of google's libOpenSLES
implementation, from an old commit which specifically mentions fixing build
for non-android platforms (somewhere between then and the current master this
was broken again, and it looks like a PITA to fix the assumptions made)

##### changes by us:

- use integer types from `<stdint.h>` by default, instead of assuming win32
`long` behavior (not sure how google overrides these when building for android)

- start work on decoupling enabling of android extensions from building
for android (`SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE` was decoupled and
confirmed to work with an actual app)
any apps using the OpenSLES API are expected to assume they're running
on android, since there isn't any other (perhaps compliant) OpenSLES
implementation, afaict anyway

- to partially work around the fact that the SDL backend doesn't include any
mechanism for sample rate conversion, the `SLES_SDL_FREQ` env is introduced
so that a user can manually specify the correct sample rate for cases where
only one sample rate is ever used by an app, but it's not the default
of 44100kHz

- meson.build files added which facilitate building for non-android platforms
(tested on Linux) - google doesn't specify how to compile for non-android
platforms, they just mention that it's a thing

##### motivation:

The main user for this is [our android translation layer](https://gitlab.com/Mis012/android_translation_layer_PoC/),
since native parts of android applications are allowed to expect `libOpenSLES.so` to exist
and provide a so-so OpenSLES implementation with android extensions.

However it is definitely possible to use this (and possibly some code from the translation layer)
to lower the amount of code changes needed for adding Linux support to applications written
for android but having near zero dependency on anything Java.

##### TODO:

- add support for sample rate conversion to the SDL backend

- decouple other android extensions whenever there is an opportunity to test them with a real world
application

- figure out whether something that google did since the commit which we use is relevant and needs backporting  
alternatively, convince google to fix up HEAD and try to upstream this (not particularly likely to succeed)
