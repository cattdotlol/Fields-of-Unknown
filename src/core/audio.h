#ifndef CORE_AUDIO_H
#define CORE_AUDIO_H

/* Rain streams from disk and follows the weather; thunder is generated at
   startup rather than shipped, so there is no second file to lose. */

void AudioLoad(void);
void AudioUnload(void);
void AudioUpdate(void);

#endif /* CORE_AUDIO_H */
