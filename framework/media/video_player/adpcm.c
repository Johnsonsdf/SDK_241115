/****************************************
*****************************************/	
#include "adpcm.h"

// define the resampling struct information
#define FRAC_BITS 16
#define FRAC (1 << FRAC_BITS)

/* Intel ADPCM step variation table */
static const int indexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
};

static const int stepsizeTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};          

extern unsigned char wav_first_flag;

void adpcm_decoder(char indata[], short outdata[], int len, adpcm_state *state, int avi_flag)
	
{
	signed char *inp;		/* Input buffer pointer */
	short *outp;		/* output buffer pointer */
	int sign;			/* Current adpcm sign bit */
	int delta;			/* Current adpcm output value */
	int step;			/* Stepsize */
	int valpred;		/* Predicted value */
	int vpdiff;			/* Current change to valpred */
	int index;			/* Current step change index */
	int inputbuffer = 0;		/* place to keep next 4-bit value */
	int bufferstep;		/* toggle between inputbuffer/input */
	//int *outLR;
	//int length = len;
	
	outp = outdata;
	//outLR = (int*)outdata;
	inp = (signed char *)indata;

	valpred = state->valprev;
	index = state->index;
	step = stepsizeTable[index];

	bufferstep = 0;
	
	for( ; len > 0 ; len-- ) {
		/* Step 1 - get the delta value */
		if(avi_flag)
		{
			if ( !bufferstep ) 
			{
               inputbuffer = *inp++;
               delta = inputbuffer & 0xf;
            } 
            else
            {
               delta = (inputbuffer >> 4) & 0xf;
            }
		}
		else
		{
            if ( bufferstep ) 
            {
                delta = inputbuffer & 0xf;
            } 
            else 
            {
                inputbuffer = *inp++;
                delta = (inputbuffer >> 4) & 0xf;
            }
		}
		bufferstep = !bufferstep;
		
		/* Step 2 - Find new index value (for later) */
		index += indexTable[delta];
		if ( index < 0 )
			index = 0;
		if ( index > 88 )
			index = 88;

		/* Step 3 - Separate sign and magnitude */
		sign = delta & 8;
		delta = delta & 7;
		
		/* Step 4 - Compute difference and new predicted value */
		/*
		** Computes 'vpdiff = (delta+0.5)*step/4', but see comment
		** in adpcm_coder. */
		vpdiff = step >> 3;
		if ( delta & 4 )
			vpdiff += step;
		if ( delta & 2 )
			vpdiff += step>>1;
		if ( delta & 1 )
			vpdiff += step>>2;
		
		if ( sign )
			valpred -= vpdiff;
		else
			valpred += vpdiff;

		/* Step 5 - clamp output value */
		if ( valpred > 32767 )
			valpred = 32767;
		else if ( valpred < -32768 )
			valpred = -32768;
		
		/* Step 6 - Update step value */
		step = stepsizeTable[index];
		
		/* Step 7 - Output value */
#if 0
		*outLR++ = (valpred&0xffff)|(valpred<<16);
#else
		*outp++ = valpred;
		//*outp++ = valpred;//
#endif		
	}
	state->valprev = valpred;
	state->index = index;
}

