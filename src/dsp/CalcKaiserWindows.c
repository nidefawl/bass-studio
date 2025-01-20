// FIR filters by Windowing
// A.Greensted - Feb 2010
// http://www.labbookpages.co.uk

#include "CalcKaiserWindow.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double* calcLPF(double sampFreq, double transFreq, double rippleDB, double transWidth, int* kaiserWindowLength) {
	double beta;

	calculateKaiserParams(rippleDB, transWidth, sampFreq, kaiserWindowLength, &beta);

	double *lpf = create1TransSinc(*kaiserWindowLength, transFreq, sampFreq, LOW_PASS);
	double *lpf_kaiser = createKaiserWindow(lpf, NULL, *kaiserWindowLength, beta);
	free(lpf);
	return lpf_kaiser;
}



// Create sinc function for filter with 1 transition - Low and High pass filters
double *create1TransSinc(int windowLength, double transFreq, double sampFreq, enum filterType type)
{
	if (type != LOW_PASS && type != HIGH_PASS) {
		fprintf(stderr, "create1TransSinc: Bad filter type, should be either LOW_PASS of HIGH_PASS\n");
		return NULL;
	}
	int n;

	// Allocate memory for the window
	double *window = (double *) malloc(windowLength * sizeof(double));
	if (window == NULL) {
		fprintf(stderr, "create1TransSinc: Could not allocate memory for window\n");
		return NULL;
	}


	// Calculate the normalised transistion frequency. As transFreq should be
	// less than or equal to sampFreq / 2, ft should be less than 0.5
	double ft = transFreq / sampFreq;

	double m_2 = 0.5 * (windowLength-1);
	int halfLength = windowLength / 2;

	// Set centre tap, if present
	// This avoids a divide by zero
	if (2*halfLength != windowLength) {
		double val = 2.0 * ft;

		// If we want a high pass filter, subtract sinc function from a dirac pulse
		if (type == HIGH_PASS) val = 1.0 - val;

		window[halfLength] = val;
	}
	else if (type == HIGH_PASS) {
		fprintf(stderr, "create1TransSinc: For high pass filter, window length must be odd\n");
		free(window);
		return NULL;
	}

	// This has the effect of inverting all weight values
	if (type == HIGH_PASS) ft = -ft;

	// Calculate taps
	// Due to symmetry, only need to calculate half the window
	for (n=0 ; n<halfLength ; n++) {
		double val = sin(2.0 * M_PI * ft * (n-m_2)) / (M_PI * (n-m_2));

		window[n] = val;
		window[windowLength-n-1] = val;
	}

	return window;
}

// Create two sinc functions for filter with 2 transitions - Band pass and band stop filters
double *create2TransSinc(int windowLength, double trans1Freq, double trans2Freq, double sampFreq, enum filterType type)
{
	if (type != BAND_PASS && type != BAND_STOP) {
		fprintf(stderr, "create2TransSinc: Bad filter type, should be either BAND_PASS or BAND_STOP\n");
		return NULL;
	}
	int n;

	// Allocate memory for the window
	double *window = (double *) malloc(windowLength * sizeof(double));
	if (window == NULL) {
		fprintf(stderr, "create2TransSinc: Could not allocate memory for window\n");
		return NULL;
	}


	// Calculate the normalised transistion frequencies.
	double ft1 = trans1Freq / sampFreq;
	double ft2 = trans2Freq / sampFreq;

	double m_2 = 0.5 * (windowLength-1);
	int halfLength = windowLength / 2;

	// Set centre tap, if present
	// This avoids a divide by zero
	if (2*halfLength != windowLength) {
		double val = 2.0 * (ft2 - ft1);

		// If we want a band stop filter, subtract sinc functions from a dirac pulse
		if (type == BAND_STOP) val = 1.0 - val;

		window[halfLength] = val;
	}
	else {
		fprintf(stderr, "create1TransSinc: For band pass and band stop filters, window length must be odd\n");
		free(window);
		return NULL;
	}

	// Swap transition points if Band Stop
	if (type == BAND_STOP) {
		double tmp = ft1;
		ft1 = ft2; ft2 = tmp;
	}

	// Calculate taps
	// Due to symmetry, only need to calculate half the window
	for (n=0 ; n<halfLength ; n++) {
		double val1 = sin(2.0 * M_PI * ft1 * (n-m_2)) / (M_PI * (n-m_2));
		double val2 = sin(2.0 * M_PI * ft2 * (n-m_2)) / (M_PI * (n-m_2));

		window[n] = val2 - val1;
		window[windowLength-n-1] = val2 - val1;
	}

	return window;
}

// Create a set of window weights
// in - If not null, each value will be multiplied with the window weight
// out - The output weight values, if NULL and new array will be allocated
// windowLength - the number of weights
// windowType - The window type
double *createWindow(double *in, double *out, int windowLength, enum windowType type)
{
	// If output buffer has not been allocated, allocate memory now
	if (out == NULL) {
		out = (double *) malloc(windowLength * sizeof(double));
		if (out == NULL) {
			fprintf(stderr, "Could not allocate memory for window\n");
			return NULL;
		}
	}

	int n;
	int m = windowLength - 1;
	int halfLength = windowLength / 2;

	// Calculate taps
	// Due to symmetry, only need to calculate half the window
	switch (type)
	{
		case RECTANGULAR:
			for (n=0 ; n<windowLength ; n++) {
				out[n] = 1.0;
			}
			break;

		case BARTLETT:
			for (n=0 ; n<=halfLength ; n++) {
				double tmp = (double) n - (double)m / 2;
				double val = 1.0 - (2.0 * fabs(tmp))/m;
				out[n] = val;
				out[windowLength-n-1] = val;
			}

			break;

		case HANNING:
			for (n=0 ; n<=halfLength ; n++) {
				double val = 0.5 - 0.5 * cos(2.0 * M_PI * n / m);
				out[n] = val;
				out[windowLength-n-1] = val;
			}

			break;

		case HAMMING:
			for (n=0 ; n<=halfLength ; n++) {
				double val = 0.54 - 0.46 * cos(2.0 * M_PI * n / m);
				out[n] = val;
				out[windowLength-n-1] = val;
			}
			break;

		case BLACKMAN:
			for (n=0 ; n<=halfLength ; n++) {
				double val = 0.42 - 0.5 * cos(2.0 * M_PI * n / m) + 0.08 * cos(4.0 * M_PI * n / m);
				out[n] = val;
				out[windowLength-n-1] = val;
			}
			break;
	}

	// If input has been given, multiply with out
	if (in != NULL) {
		for (n=0 ; n<windowLength ; n++) {
			out[n] *= in[n];
		}
	}

	return out;
}

// Transition Width (transWidth) is given in Hz
// Sampling Frequency (sampFreq) is given in Hz
// Window Length (windowLength) will be set
void calculateKaiserParams(double ripple, double transWidth, double sampFreq, int *windowLength, double *beta)
{
	// Calculate delta w
	double dw = 2 * M_PI * transWidth / sampFreq;

	// Calculate ripple dB
	double a = -20.0 * log10(ripple);

	// Calculate filter order
	int m;
	if (a>21) m = (int) ceil((a-7.95) / (2.285*dw));
	else m = (int) ceil(5.79/dw);

	*windowLength = m + 1;

	if (a<=21) *beta = 0.0;
	else if (a<=50) *beta = 0.5842 * pow(a-21, 0.4) + 0.07886 * (a-21);
	else *beta = 0.1102 * (a-8.7);
}

double *createKaiserWindow(double *in, double *out, int windowLength, double beta)
{
	double m_2 = (double)(windowLength-1) / 2.0;
	double denom = modZeroBessel(beta);					// Denominator of Kaiser function

	// If output buffer has not been allocated, allocate memory now
	if (out == NULL) {
		out = (double *) malloc(windowLength * sizeof(double));
		if (out == NULL) {
			fprintf(stderr, "Could not allocate memory for window\n");
			return NULL;
		}
	}

	int n;
	for (n=0 ; n<windowLength ; n++)
	{
		double val = ((n) - m_2) / m_2;
		val = 1 - (val * val);
		out[n] = modZeroBessel(beta * sqrt(val)) / denom;
	}

	// If input has been given, multiply with out
	if (in != NULL) {
		for (n=0 ; n<windowLength ; n++) {
			out[n] *= in[n];
		}
	}

	return out;
}

double modZeroBessel(double x)
{
	int i;

	double x_2 = x/2;
	double num = 1;
	double fact = 1;
	double result = 1;

	for (i=1 ; i<20 ; i++) {
		num *= x_2 * x_2;
		fact *= i;
		result += num / (fact * fact);
	}

	return result;
}
