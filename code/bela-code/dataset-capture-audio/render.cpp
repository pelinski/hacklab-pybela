#include <Bela.h>
#include <Watcher.h>
#include <cmath>
#include <libraries/Biquad/Biquad.h>
#include <libraries/Scope/Scope.h>

// Vector of Watcher pointers
Watcher<float> gInAudio("gInAudio");

Biquad hpFilter; // Biquad high-pass frequency;

Scope scope;

bool setup(BelaContext *context, void *userData) {
  Bela_getDefaultWatcherManager()->getGui().setup(context->projectName);
  Bela_getDefaultWatcherManager()->setup(
      context->audioSampleRate); // set sample rate in watcher

  // Initialize the scope
  scope.setup(1, context->audioSampleRate);

  Biquad::Settings settings{
      .fs = context->audioSampleRate,
      .type = Biquad::highpass,
      .cutoff = 2.0,
      .q = 0.707,
      .peakGainDb = 0,
  };
  hpFilter.setup(settings);

  return true;
}

void render(BelaContext *context, void *userData) {
  for (unsigned int n = 0; n < context->audioFrames; n++) {
    uint64_t frames = context->audioFramesElapsed + n;
    Bela_getDefaultWatcherManager()->tick(frames);

    float in = audioRead(context, n, i);
    // out *= 0.4; // Scale down to avoid clipping
    in = hpFilter.process(in); // Apply high-pass filter
    gInAudio = in;             // Update the watcher with the analog input value

    float out = in;
    for (unsigned int channel = 0; channel < context->audioOutChannels;
         channel++) {
      audioWrite(context, n, channel, out);
    }
    scope.log(gAudioIn);
  }
}

void cleanup(BelaContext *context, void *userData) {}