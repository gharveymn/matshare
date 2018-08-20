#include <intrin.h>
#include "mex.h"

void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{
	size_t i;
	__m256d accum_avx_vect, addend_avx_vect;
	
	size_t num_elems = mxGetNumberOfElements(prhs[0]);
	double* accum = mxGetData(prhs[0]);
	double* addend = mxGetData(prhs[1]);
	
	for(i = 0; i < num_elems/4 && (accum += 4) && (addend += 4); i++)
	{
		accum_avx_vect  = _mm256_load_pd(accum);
		addend_avx_vect = _mm256_load_pd(addend);
		_mm256_store_pd(accum, _mm256_add_pd(accum_avx_vect, addend_avx_vect));
	}
	
	/* add the up to 3 more elements */
	for(i = 0; i < num_elems%4; i++, accum++, addend++)
	{
		*accum += *addend;
	}
	
}