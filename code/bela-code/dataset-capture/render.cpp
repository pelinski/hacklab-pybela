#include <Bela.h>
#include <Watcher.h>
#include <libraries/Scope/Scope.h>


Watcher<float> wIn("wIn");
Scope scope;


bool setup(BelaContext *context, void *userData) {

  Bela_getDefaultWatcherManager()->getGui().setup(context->projectName);
  Bela_getDefaultWatcherManager()->setup(
      context->audioSampleRate); // set sample rate in watcher

  scope.setup(2, context->audioSampleRate);

  return true;
}

void render(BelaContext *context, void *userData) {

  for (unsigned int n = 0; n < context->audioFrames; n++) {
    uint64_t frames = context->audioFramesElapsed + n;
    Bela_getDefaultWatcherManager()->tick(frames);

    float _in = audioRead(context, n, 0);
    wIn = _in; // update watcher with input value
    float _out = _in;

    audioWrite(context, n, 0, _out);
    audioWrite(context, n, 1, _out);

    scope.log(_in,_out); // scope doesn't like watched variables
  }
}

void cleanup(BelaContext *context, void *userData) {}
