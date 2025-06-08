#include <Bela.h>
#include <Watcher.h>
#include <libraries/Scope/Scope.h>


Watcher<float> in("in");
Watcher<float> out("out");
Scope scope;


bool setup(BelaContext *context, void *userData) {

  Bela_getDefaultWatcherManager()->getGui().setup(context->projectName);
  Bela_getDefaultWatcherManager()->setup(
      context->audioSampleRate); // set sample rate in watcher

  gAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;
  gInverseSampleRate = 1.0 / context->audioSampleRate;

  scope.setup(2, context->audioSampleRate);

  return true;
}

void render(BelaContext *context, void *userData) {

  for (unsigned int n = 0; n < context->audioFrames; n++) {
    uint64_t frames = int((context->audioFramesElapsed + n) / 2);
    Bela_getDefaultWatcherManager()->tick(frames);

    in = audioRead(context, n, 0);
    out = in;
    
    audioWrite(context, n, 0, out);
    audioWrite(context, n, 1, out);
    scope.log(in,out);
  }
}

void cleanup(BelaContext *context, void *userData) {}
