#include <vector>

/**
* Invoke soxr to downsample single channel audio data
* @param sampleRate
* @param samplesIn
* @param len
* @param samplesOut
* @param downSampleFactor
* @return 0 = no error
*/
int downsample(float sampleRate, float* samplesIn, int len, std::vector<float>& samplesOut, int downSampleFactor);
