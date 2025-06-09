#include <Bela.h>
#include <RtThread.h>
#include <libraries/Biquad/Biquad.h>
#include <libraries/Scope/Scope.h>
#include <torch/script.h>
#include <vector>

#define N_VARS 1
#define NUM_SENSORS 7

float gGain;
float gMean = -1.860415344092044e-08;
float gStd = 0.08207735935086384;
Biquad hpFilter; // Biquad high-pass frequency;
Scope scope;

// torch buffers
const int gWindowSize = 512;
int gNumTargetWindows = 1;
const int gInputBufferSize = 50 * gWindowSize;
const int gOutputBufferSize = 50 * gNumTargetWindows * gInputBufferSize;
int gOutputBufferWritePointer = 3 * gWindowSize;
int gDebugPrevBufferWritePointer = gOutputBufferWritePointer;
int gDebugFrameCounter = 0;
int gOutputBufferReadPointer = 0;
int gInputBufferPointer = 0;
int gWindowFramesCounter = 0;

std::vector<std::vector<float>>
    gInputBuffer(N_VARS, std::vector<float>(gInputBufferSize));
std::vector<std::vector<float>>
    gOutputBuffer(N_VARS, std::vector<float>(gOutputBufferSize));
torch::jit::script::Module model;
std::vector<std::vector<float>> unwrappedBuffer(gWindowSize,
                                                std::vector<float>(N_VARS));

AuxiliaryTask gInferenceTask;
int gCachedInputBufferPointer = 0;

void inference_task_background(void *);

std::string gModelPath = "model.jit";

int start = 1;

unsigned int gAudioFramesPerAnalogFrame; // Number of audio frames per analog
                                         // frame
float gInvSampleRate;                    // 1/sample rate
float gInvAudioFramesPerAnalogFrame;     // 1/audio frames per analog frame

bool setup(BelaContext *context, void *userData) {

  gAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;
  gInvAudioFramesPerAnalogFrame = 1.0f / gAudioFramesPerAnalogFrame;
  gInvSampleRate = 1.0f / context->audioSampleRate;

  // Set up the thread for the inference
  Biquad::Settings settings{
      .fs = context->audioSampleRate,
      .type = Biquad::highpass,
      .cutoff = 2.0,
      .q = 0.707,
      .peakGainDb = 0,
  };
  hpFilter.setup(settings);

  scope.setup(3, context->analogSampleRate);

  gInferenceTask =
      Bela_createAuxiliaryTask(inference_task_background, 99, "bela-inference");

  if (userData != 0)
    gModelPath = *(std::string *)userData;

  try {
    model = torch::jit::load(gModelPath);
  } catch (const c10::Error &e) {
    std::cerr << "Error loading the model: " << e.msg() << std::endl;
    return false;
  }
  // Warm up inference
  try {
    // Create a dummy input tensor
    torch::Tensor dummy_input =
        torch::rand({1, gWindowSize, N_VARS}, torch::kFloat);

    for (int i = 0; i < 10; ++i) { // warmup
      printf("Warming up inference %d\n", i);
      auto output = model.forward({dummy_input}).toTensor();
      output = model.forward({dummy_input}).toTensor();
    }
    auto output = model.forward({dummy_input}).toTensor();
    output = model.forward({dummy_input}).toTensor();
    // Print the result
    std::cout << "Input tensor dimensions: " << dummy_input.sizes()
              << std::endl;
    std::cout << "Output tensor dimensions: " << output.sizes() << std::endl;
  } catch (const c10::Error &e) {
    std::cerr << "Error during dummy inference: " << e.msg() << std::endl;
    return false;
  }

  return true;
}
void inference_task(const std::vector<std::vector<float>> &inBuffer,
                    unsigned int inPointer,
                    std::vector<std::vector<float>> &outBuffer,
                    unsigned int outPointer) {
  RtThread::setThisThreadPriority(1);

  // Precompute circular buffer indices
  std::vector<int> circularIndices(gWindowSize);
  for (int n = 0; n < gWindowSize; ++n) {
    circularIndices[n] =
        (inPointer + n - gWindowSize + gInputBufferSize) % gInputBufferSize;
  }

  // Fill unwrappedBuffer using precomputed indices
  for (int n = 0; n < gWindowSize; ++n) {
    for (int i = 0; i < N_VARS; ++i) {
      unwrappedBuffer[n][i] = inBuffer[i][circularIndices[n]];
    }
  }

  // Convert unwrappedBuffer to a Torch tensor without additional copying
  torch::Tensor inputTensor =
      torch::from_blob(unwrappedBuffer.data(), {1, gWindowSize, N_VARS},
                       torch::kFloat)
          .clone();

  // Perform inference
  torch::Tensor outputTensor = model.forward({inputTensor}).toTensor();
  outputTensor = outputTensor.squeeze(
      0); // Shape: [gNumTargetWindows * gWindowSize, N_VARS]

  // Prepare a pointer to the output tensor's data
  float *outputData = outputTensor.data_ptr<float>();

  // Precompute output circular buffer indices
  std::vector<int> outCircularIndices(gNumTargetWindows * gWindowSize);
  for (int n = 0; n < gNumTargetWindows * gWindowSize; ++n) {
    outCircularIndices[n] = (outPointer + n) % gOutputBufferSize;
  }

  // Fill outBuffer using precomputed indices and outputData
  for (int n = 0; n < gNumTargetWindows * gWindowSize; ++n) {
    int circularBufferIndex = outCircularIndices[n];
    for (int i = 0; i < N_VARS; ++i) {
      outBuffer[i][circularBufferIndex] = outputData[n * N_VARS + i];
    }
  }
}

void inference_task_background(void *) {

  RtThread::setThisThreadPriority(1);

  inference_task(gInputBuffer, gInputBufferPointer, gOutputBuffer,
                 gOutputBufferWritePointer);
  gOutputBufferWritePointer =
      (gOutputBufferWritePointer + gNumTargetWindows * gWindowSize) %
      gOutputBufferSize;
}

void render(BelaContext *context, void *userData) {

  for (unsigned int n = 0; n < context->audioFrames; n++) {
    if (gAudioFramesPerAnalogFrame && !(n % gAudioFramesPerAnalogFrame)) {
      float in = 0.0;
      for (unsigned int i = 0; i < NUM_SENSORS; i++) {
        in += analogRead(context, n / gAudioFramesPerAnalogFrame, i);
      }
      gGain = analogRead(context, n / gAudioFramesPerAnalogFrame, NUM_SENSORS);

      // out *= 0.4; // Scale down to avoid clipping
      in = hpFilter.process(in);
      in = (in - gMean) / gStd; // norm
      gInputBuffer[0][gInputBufferPointer] =
          in; // Update the watcher with the analog input value

      // -- pytorch buffer

      if (++gInputBufferPointer >= gInputBufferSize) {
        // Wrap the circular buffer
        // Notice: this is not the condition for starting a new inference
        gInputBufferPointer = 0;
      }

      if (++gWindowFramesCounter >= gWindowSize) {
        gWindowFramesCounter = 0;
        gCachedInputBufferPointer = gInputBufferPointer;
        Bela_scheduleAuxiliaryTask(gInferenceTask);
      }

      // debugging
      gDebugFrameCounter++;
      if (gOutputBufferWritePointer != gDebugPrevBufferWritePointer) {
        rt_printf("aux task took: %d, write pointer - read pointer: %d \n",
                  gDebugFrameCounter,
                  gOutputBufferWritePointer - gOutputBufferReadPointer);
        gDebugPrevBufferWritePointer = gOutputBufferWritePointer;
        gDebugFrameCounter = 0;
      }

      // Get the output sample from the output buffer
      float out = gOutputBuffer[0][gOutputBufferReadPointer];
      out = out * gStd + gMean;
      // Write the audio input to left channel, output to
      // the right channel
      audioWrite(context, n, 0, out);
      audioWrite(context, n, 1, out);
      // Increment the read pointer in the output circular buffer
      if ((gOutputBufferReadPointer + 1) % gOutputBufferSize ==
          gOutputBufferWritePointer) {

        rt_printf("read pointer: %d, write pointer %d \n",
                  gOutputBufferReadPointer, gOutputBufferWritePointer);
        rt_printf("Warning: output buffer overrun\n");
      } else {
        gOutputBufferReadPointer++;
      }
      if (gOutputBufferReadPointer >= gOutputBufferSize)
        gOutputBufferReadPointer = 0;

      scope.log(in, out, gGain);
      // --
    }

    //   if (n % 64) { // debug every 64 frames
    //     rt_printf("audioIn: %.2f, pot2: %.2f\n", audioIn, pot2);
    //     rt_printf("audioOut: %.2f, outpot2: %.2f\n", audioOut, outpot2);
    //   }
  }
}

void cleanup(BelaContext *context, void *userData) {
  // Clean up resources
}
