#pragma once

#include <stdbool.h>
#include "audio.h"

bool audio_vorbis_load(audio_StaticSource *source, char const * filename);
void audio_vorbis_decoder_init();
