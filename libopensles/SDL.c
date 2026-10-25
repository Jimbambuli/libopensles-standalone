/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** \file SDL.c SDL platform implementation */

#include "sles_allinclusive.h"
#include <SDL.h>

/** \brief Copy PCM data into the SDL stream, applying the player's gains */

static void SDL_mix(const short *source, short *destination, SLuint32 size,
    SLuint32 channels, const float *gains)
{
    for (SLuint32 i = 0; i < size / sizeof(short); ++i) {
        SLuint32 channel = i % channels;
        float gain = channel < STEREO_CHANNELS ? gains[channel] : 1.0f;
        destination[i] = source[i] * gain;
    }
}


/** \brief Called by SDL to fill the next audio output buffer for one audio player */

static void SDLCALL SDL_callback(void *context, Uint8 *stream, int len)
{
    assert(len > 0);
    CAudioPlayer *audioPlayer = (CAudioPlayer *) context;
    IBufferQueue *bufferQueue = &audioPlayer->mBufferQueue;
    Uint8 *destination = stream;
    SLuint32 remaining = (SLuint32) len;
    SLuint32 channels = audioPlayer->mNumChannels;
    SLuint32 frameSize = channels * sizeof(short);
    if (remaining >= frameSize) {
        remaining -= remaining % frameSize;
    } else {
        remaining = 0;
    }

    object_lock_exclusive(&audioPlayer->mObject);

    for (;;) {
        if (bufferQueue->mClearRequested) {
            bufferQueue->mFront = &bufferQueue->mArray[0];
            bufferQueue->mRear = &bufferQueue->mArray[0];
            bufferQueue->mState.count = 0;
            bufferQueue->mState.playIndex = 0;
            audioPlayer->mReader = NULL;
            audioPlayer->mAvail = 0;
            bufferQueue->mClearRequested = SL_BOOLEAN_FALSE;
            object_cond_broadcast(&audioPlayer->mObject);
        }

        if (SL_PLAYSTATE_PLAYING != audioPlayer->mPlay.mState) {
            if (SL_PLAYSTATE_STOPPED == audioPlayer->mPlay.mState) {
                audioPlayer->mReader = NULL;
                audioPlayer->mAvail = 0;
            }
            break;
        }

        if (0 == remaining) {
            break;
        }

        if (0 == audioPlayer->mAvail) {
            if (bufferQueue->mFront == bufferQueue->mRear) {
                break;
            }
            const BufferHeader *front = bufferQueue->mFront;
            audioPlayer->mReader = front->mBuffer;
            audioPlayer->mAvail = front->mSize;
        }

        SLuint32 actual = remaining;
        if (actual > audioPlayer->mAvail) {
            actual = audioPlayer->mAvail;
        }
        actual -= actual % frameSize;

        if (0 < actual) {
            SDL_mix((const short *) audioPlayer->mReader, (short *) destination,
                actual, channels, audioPlayer->mGains);
            destination += actual;
            remaining -= actual;
            audioPlayer->mReader = (const char *) audioPlayer->mReader + actual;
            audioPlayer->mAvail -= actual;
            SLuint32 frames = actual / frameSize;
            audioPlayer->mPlay.mFramesSinceLastSeek += frames;
            audioPlayer->mPlay.mFramesSincePositionUpdate += frames;
        } else {
            // discard a remainder that is smaller than one frame
            audioPlayer->mReader = (const char *) audioPlayer->mReader + audioPlayer->mAvail;
            audioPlayer->mAvail = 0;
        }

        if (0 == audioPlayer->mAvail) {
            const BufferHeader *oldFront = bufferQueue->mFront;
            const BufferHeader *rear = bufferQueue->mRear;
            if (oldFront != rear) {
                BufferHeader *newFront = (BufferHeader *) oldFront;
                if (++newFront == &bufferQueue->mArray[bufferQueue->mNumBuffers + 1]) {
                    newFront = bufferQueue->mArray;
                }
                bufferQueue->mFront = newFront;
                assert(0 < bufferQueue->mState.count);
                --bufferQueue->mState.count;
                ++bufferQueue->mState.playIndex;
                // The callback may enqueue more data and must be called with the lock released
                slBufferQueueCallback callback = bufferQueue->mCallback;
                void *callbackContext = bufferQueue->mContext;
                object_unlock_exclusive(&audioPlayer->mObject);
                if (NULL != callback) {
                    (*callback)(&bufferQueue->mItf, callbackContext);
                }
                object_lock_exclusive(&audioPlayer->mObject);
            }
        }
    }

    if (destination < stream + len) {
        memset(destination, 0, (size_t) (stream + len - destination));
    }

    object_unlock_exclusive(&audioPlayer->mObject);
}


/** \brief Open a dedicated SDL audio device for an audio player */

SLresult SDL_open(CAudioPlayer *audioPlayer)
{
    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        SL_LOGE("Unable to initialize SDL audio: %s", SDL_GetError());
        return SL_RESULT_RESOURCE_ERROR;
    }

    SDL_AudioSpec fmt;
    SDL_zero(fmt);
    fmt.freq = audioPlayer->mSampleRateMilliHz / 1000;
    fmt.format = AUDIO_S16;
    fmt.channels = audioPlayer->mNumChannels;
#ifdef _WIN32 // FIXME Either a bug or a serious misunderstanding
    fmt.samples = SndFile_BUFSIZE;
#else
    fmt.samples = SndFile_BUFSIZE / sizeof(short);
#endif
    fmt.callback = SDL_callback;
    fmt.userdata = (void *) audioPlayer;

    SDL_AudioDeviceID device = SDL_OpenAudioDevice(NULL, 0, &fmt, NULL, 0);
    if (0 == device) {
        SL_LOGE("Unable to open audio: %s", SDL_GetError());
        return SL_RESULT_RESOURCE_ERROR;
    }

    audioPlayer->mAudioDevice = device;
    audioPlayer->mReader = NULL;
    audioPlayer->mAvail = 0;
    audioPlayer->mGains[0] = 1.0f;
    audioPlayer->mGains[1] = 1.0f;

    // The callback runs even while the player is stopped so that buffer queue clear
    // and player destroy requests can always be acknowledged.
    SDL_PauseAudioDevice(device, 0);
    return SL_RESULT_SUCCESS;
}


/** \brief Close the SDL audio device dedicated to an audio player */

void SDL_close(CAudioPlayer *audioPlayer)
{
    if (0 != audioPlayer->mAudioDevice) {
        SDL_PauseAudioDevice(audioPlayer->mAudioDevice, 1);
        SDL_CloseAudioDevice(audioPlayer->mAudioDevice);
        audioPlayer->mAudioDevice = 0;
    }
}


/** \brief Check whether the supplied audio player source and sink are supported */

SLresult SDL_checkAudioPlayerSourceSink(CAudioPlayer *this)
{
    switch (this->mDataSource.mLocator.mLocatorType) {
    case SL_DATALOCATOR_BUFFERQUEUE:
#ifdef ANDROID_EXT
    case SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE:
#endif
        if (SL_DATAFORMAT_PCM != this->mDataSource.mFormat.mFormatType) {
            return SL_RESULT_CONTENT_UNSUPPORTED;
        }
        break;
    case SL_DATALOCATOR_URI:
        break;
    default:
        return SL_RESULT_CONTENT_UNSUPPORTED;
    }

    if (SL_DATALOCATOR_OUTPUTMIX != this->mDataSink.mLocator.mLocatorType) {
        return SL_RESULT_CONTENT_UNSUPPORTED;
    }

    return SL_RESULT_SUCCESS;
}


/** \brief Called when a gain-related field (mute, solo, volume, stereo position, etc.) updated */

void audioPlayerGainUpdate(CAudioPlayer *audioPlayer)
{
    SLboolean mute = audioPlayer->mVolume.mMute;
    SLuint8 muteMask = audioPlayer->mMuteMask;
    SLuint8 soloMask = audioPlayer->mSoloMask;
    SLmillibel level = audioPlayer->mVolume.mLevel;
    SLboolean enableStereoPosition = audioPlayer->mVolume.mEnableStereoPosition;
    SLpermille stereoPosition = audioPlayer->mVolume.mStereoPosition;

    if (soloMask) {
        muteMask |= ~soloMask;
    }
    if (mute || !(~muteMask & 3)) {
        audioPlayer->mGains[0] = 0.0f;
        audioPlayer->mGains[1] = 0.0f;
    } else {
        float playerGain = powf(10.0f, level / 2000.0f);
        unsigned channel;
        for (channel = 0; channel < STEREO_CHANNELS; ++channel) {
            float gain;
            if (muteMask & (1 << channel)) {
                gain = 0.0f;
            } else {
                gain = playerGain;
                if (enableStereoPosition) {
                    switch (channel) {
                    case 0:
                        if (stereoPosition > 0) {
                            gain *= (1000 - stereoPosition) / 1000.0f;
                        }
                        break;
                    case 1:
                        if (stereoPosition < 0) {
                            gain *= (1000 + stereoPosition) / 1000.0f;
                        }
                        break;
                    default:
                        assert(SL_BOOLEAN_FALSE);
                        break;
                    }
                }
            }
            audioPlayer->mGains[channel] = gain;
        }
    }
}
