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
Watcher <float> gIn("gIn");
float gInPiezos[NUM_SENSORS]; // Analog inputs for piezo sensors
float gGain;

Scope scope;


bool setup(BelaContext* context, void* userData) {
    Bela_getDefaultWatcherManager()->getGui().setup(context->projectName);
    Bela_getDefaultWatcherManager()->setup(context->audioSampleRate); // set sample rate in watcher
    gAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;
    gInvAudioFramesPerAnalogFrame = 1.0f / gAudioFramesPerAnalogFrame;
    gInvSampleRate = 1.0f / context->audioSampleRate;

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
            float in = 0.0;
            for (unsigned int i = 0; i < NUM_SENSORS; i++) {
                gInPiezos[i] = analogRead(context, n / gAudioFramesPerAnalogFrame, i);
                in+= gInPiezos[i];
                // gAmplitude[i] = gPiezos[i]->get();
            }
            gGain = analogRead(context, n / gAudioFramesPerAnalogFrame, NUM_SENSORS);


            // out *= 0.4; // Scale down to avoid clipping
            in = hpFilter.process(in); // Apply high-pass filter
            *gIn = in; // Update the watcher with the analog input value
            
            float out = in;
            for (unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
              audioWrite(context, n, channel, out);
            }
            scope.log(gInPiezos[0], gInPiezos[1], gInPiezos[2], gInPiezos[3], gInPiezos[4], gInPiezos[5], gInPiezos[6], gGain, out);
          }



    }
}

void cleanup(BelaContext* context, void* userData) {
}