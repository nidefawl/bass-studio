#ifndef CALC_KAISER_H__
#define CALC_KAISER_H__
// FIR filters by Windowing
// A.Greensted - Feb 2010
// http://www.labbookpages.co.uk

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <string.h>

enum filterType {LOW_PASS, HIGH_PASS, BAND_PASS, BAND_STOP};
enum windowType {RECTANGULAR, BARTLETT, HANNING, HAMMING, BLACKMAN};


// Prototypes
double *create1TransSinc(int windowLength, double transFreq, double sampFreq, enum filterType type);
double *create2TransSinc(int windowLength, double trans1Freq, double trans2Freq, double sampFreq, enum filterType type);

double *createWindow(double *in, double *out, int windowLength, enum windowType type);

void calculateKaiserParams(double ripple, double transWidth, double sampFreq, int *windowLength, double *beta);
double *createKaiserWindow(double *in, double *out, int windowLength, double beta);
double modZeroBessel(double x);

double* calcLPF(double sampleFreq, double transFreq, double rippleDB, double transWidth, int* kaiserWindowLength);

#endif