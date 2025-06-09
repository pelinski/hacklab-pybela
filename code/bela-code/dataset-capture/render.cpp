#include <Bela.h>
#include <Watcher.h>
#include <cmath>
#include <libraries/Scope/Scope.h>
#include <libraries/Biquad/Biquad.h>
Biquad hpFilter;	// Biquad high-pass frequency;

#define NUM_SENSORS 7 // Number of sensors

unsigned int gAudioFramesPerAnalogFrame; // Number of audio frames per analog
                                         // frame
float gInvSampleRate;                    // 1/sample rate
float gInvAudioFramesPerAnalogFrame;     // 1/audio frames per analog frame

// Vector of Watcher pointers
std::vector<Watcher<float>*> gPiezos;
float gIn[NUM_SENSORS];
float gGain;

Scope scope;


bool setup(BelaContext* context, void* userData) {
    Bela_getDefaultWatcherManager()->getGui().setup(context->projectName);
    Bela_getDefaultWatcherManager()->setup(context->audioSampleRate); // set sample rate in watcher
    gAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;
    gInvAudioFramesPerAnalogFrame = 1.0f / gAudioFramesPerAnalogFrame;
    gInvSampleRate = 1.0f / context->audioSampleRate;

    // Initialize the Watcher pointers and add them to the vector
    for (unsigned int i = 0; i < NUM_SENSORS; ++i) {
        gPiezos.push_back(new Watcher<float>("piezo_" + std::to_string(i + 1)));
    }

    // Initialize the scope
    scope.setup(9, context->analogSampleRate);

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

void render(BelaContext* context, void* userData) {
    for (unsigned int n = 0; n < context->audioFrames; n++) {
        uint64_t frames = context->audioFramesElapsed + n;
        Bela_getDefaultWatcherManager()->tick(frames);

        if (gAudioFramesPerAnalogFrame && !(n % gAudioFramesPerAnalogFrame)) {
            // read analog inputs and update frequency and amplitude
            // Depending on the sampling rate of the analog inputs, this will
            // happen every audio frame (if it is 44100)
            // or every two audio frames (if it is 22050)
            for (unsigned int i = 0; i < NUM_SENSORS; i++) {
                *gPiezos[i] =  analogRead(context, n / gAudioFramesPerAnalogFrame, i);
                // gAmplitude[i] = gPiezos[i]->get();
            }
            gGain = analogRead(context, n / gAudioFramesPerAnalogFrame, NUM_SENSORS);

            float in = audioRead(context, n, 0); // Read audio input from channel 0

            // audio output
            float out = 0.0;
            for (unsigned int i = 0; i < NUM_SENSORS; i++) {
              out += gPiezos[i].get(); // Scale the input to a reasonable output level
            }
            // out *= 0.4; // Scale down to avoid clipping
            out = hpFilter.process(out); // Apply high-pass filter
            for (unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
              audioWrite(context, n, channel, out);
            }
            scope.log(gIn[0], gIn[1], gIn[2], gIn[3], gIn[4], gIn[5], gIn[6], gGain, out);
          }



    }
}

void cleanup(BelaContext* context, void* userData) {
}