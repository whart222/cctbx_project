#include "hip/hip_runtime.h"
/*
 ============================================================================
 Name        : nanoBraggCUDA.cu
 Author      :
 Version     :
 Copyright   : Your copyright notice
 Description : CUDA compute reciprocals
 ============================================================================
 */
#include <sys/time.h>
#include <iostream>
#include <numeric>
#include <stdlib.h>
#include <stdio.h>
#include "nanotypes.h"
#include "cuda_compatibility.h"
#include "cuda_struct.h"
#include "simtbx/gpu/structure_factors.h"
#include "time_logger.h"

static void CheckCudaErrorAux(const char *, unsigned, const char *, hipError_t);
#define CUDA_CHECK_RETURN(value) CheckCudaErrorAux(__FILE__,__LINE__, #value, value)

#define THREADS_PER_BLOCK_X 512
#define THREADS_PER_BLOCK_Y 1
#define THREADS_PER_BLOCK_TOTAL (THREADS_PER_BLOCK_X * THREADS_PER_BLOCK_Y)
#define VECTOR_SIZE 4

/**
 * Check the return value of the CUDA runtime API call and exit
 * the application if the call has failed.
 */
static void CheckCudaErrorAux(const char *file, unsigned line, const char *statement, hipError_t err) {
	if (err == hipSuccess)
		return;
	std::cerr << statement << " returned " << hipGetErrorString(err) << "(" << err << ") at " << file << ":" << line << std::endl;
	exit(1);
}

static hipError_t cudaMemcpyVectorDoubleToDevice(CUDAREAL *dst, double *src, size_t vector_items) {
	CUDAREAL * temp = new CUDAREAL[vector_items];
	for (size_t i = 0; i < vector_items; i++) {
		temp[i] = src[i];
	}
	hipError_t ret = hipMemcpy(dst, temp, sizeof(*dst) * vector_items, hipMemcpyHostToDevice);
	delete temp;
	return ret;
}

/* make a unit vector pointing in same direction and report magnitude (both args can be same vector) */
double cpu_unitize(double *vector, double *new_unit_vector);
double cpu_unitize(double * vector, double * new_unit_vector) {

	double v1 = vector[1];
	double v2 = vector[2];
	double v3 = vector[3];

	double mag = sqrt(v1 * v1 + v2 * v2 + v3 * v3);

	if (mag != 0.0) {
		/* normalize it */
		new_unit_vector[0] = mag;
		new_unit_vector[1] = v1 / mag;
		new_unit_vector[2] = v2 / mag;
		new_unit_vector[3] = v3 / mag;
	} else {
		/* can't normalize, report zero vector */
		new_unit_vector[0] = 0.0;
		new_unit_vector[1] = 0.0;
		new_unit_vector[2] = 0.0;
		new_unit_vector[3] = 0.0;
	}
	return mag;
}


__global__ void nanoBraggSpotsInitCUDAKernel(int spixels, int fpixesl, float * floatimage, float * omega_reduction, float * max_I_x_reduction,
		float * max_I_y_reduction, bool * rangemap);

__global__ void nanoBraggSpotsCUDAKernel(int spixels, int fpixels, int roi_xmin, int roi_xmax, int roi_ymin, int roi_ymax, int oversample, int point_pixel,
CUDAREAL pixel_size, CUDAREAL subpixel_size, int steps, CUDAREAL detector_thickstep, int detector_thicksteps, CUDAREAL detector_thick, CUDAREAL detector_mu,
		const CUDAREAL * __restrict__ sdet_vector, const CUDAREAL * __restrict__ fdet_vector, const CUDAREAL * __restrict__ odet_vector,
		const CUDAREAL * __restrict__ pix0_vector, int curved_detector, CUDAREAL distance, CUDAREAL close_distance, const CUDAREAL * __restrict__ beam_vector,
		CUDAREAL Xbeam, CUDAREAL Ybeam, CUDAREAL dmin, CUDAREAL phi0, CUDAREAL phistep, int phisteps, const CUDAREAL * __restrict__ spindle_vector, int sources,
		const CUDAREAL * __restrict__ source_X, const CUDAREAL * __restrict__ source_Y, const CUDAREAL * __restrict__ source_Z,
		const CUDAREAL * __restrict__ source_I, const CUDAREAL * __restrict__ source_lambda, const CUDAREAL * __restrict__ a0, const CUDAREAL * __restrict__ b0,
		const CUDAREAL * __restrict c0, shapetype xtal_shape, CUDAREAL mosaic_spread, int mosaic_domains, const CUDAREAL * __restrict__ mosaic_umats,
		CUDAREAL Na, CUDAREAL Nb,
		CUDAREAL Nc, CUDAREAL V_cell,
		CUDAREAL water_size, CUDAREAL water_F, CUDAREAL water_MW, CUDAREAL r_e_sqr, CUDAREAL fluence, CUDAREAL Avogadro, CUDAREAL spot_scale, int integral_form, CUDAREAL default_F,
		int interpolate, const CUDAREAL * __restrict__ Fhkl, const hklParams * __restrict__ Fhklparams, int nopolar, const CUDAREAL * __restrict__ polar_vector, CUDAREAL polarization, CUDAREAL fudge,
		const int unsigned short * __restrict__ maskimage, float * floatimage /*out*/, float * omega_reduction/*out*/, float * max_I_x_reduction/*out*/,
		float * max_I_y_reduction /*out*/, bool * rangemap);


extern "C" void nanoBraggSpotsCUDA(int deviceId, int spixels, int fpixels, int roi_xmin, int roi_xmax, int roi_ymin, int roi_ymax, int oversample, int point_pixel,
                double pixel_size, double subpixel_size, int steps, double detector_thickstep, int detector_thicksteps, double detector_thick, double detector_mu,
                double sdet_vector[4], double fdet_vector[4], double odet_vector[4], double pix0_vector[4], int curved_detector, double distance, double close_distance,
                double beam_vector[4], double Xbeam, double Ybeam, double dmin, double phi0, double phistep, int phisteps, double spindle_vector[4], int sources,
                double *source_X, double *source_Y, double * source_Z, double * source_I, double * source_lambda, double a0[4], double b0[4], double c0[4],
                shapetype xtal_shape, double mosaic_spread, int mosaic_domains, double * mosaic_umats, double Na, double Nb, double Nc, double V_cell,
                double water_size, double water_F, double water_MW, double r_e_sqr, double fluence, double Avogadro, int integral_form, double default_F,
                int interpolate, double *** Fhkl, int h_min, int h_max, int h_range, int k_min, int k_max, int k_range, int l_min, int l_max, int l_range, int hkls,
                int nopolar, double polar_vector[4], double polarization, double fudge, int unsigned short * maskimage, float * floatimage /*out*/,
                double * omega_sum/*out*/, int * sumn /*out*/, double * sum /*out*/, double * sumsqr /*out*/, double * max_I/*out*/, double * max_I_x/*out*/,
                double * max_I_y /*out*/, double spot_scale) {

	int total_pixels = spixels * fpixels;

    hipSetDevice(deviceId);

	/*allocate and zero reductions */
	bool * rangemap = (bool*) calloc(total_pixels, sizeof(bool));
	float * omega_reduction = (float*) calloc(total_pixels, sizeof(float));
	float * max_I_x_reduction = (float*) calloc(total_pixels, sizeof(float));
	float * max_I_y_reduction = (float*) calloc(total_pixels, sizeof(float));

	/* clear memory (TODO: consider this being optional) */
	memset(floatimage, 0, sizeof(__typeof__(*floatimage)) * total_pixels);

	/*create transfer arguments to device space*/
	int cu_spixels = spixels, cu_fpixels = fpixels;
	int cu_roi_xmin = roi_xmin, cu_roi_xmax = roi_xmax, cu_roi_ymin = roi_ymin, cu_roi_ymax = roi_ymax;
	int cu_oversample = oversample;
	int cu_point_pixel = point_pixel;
	CUDAREAL cu_pixel_size = pixel_size, cu_subpixel_size = subpixel_size;
	int cu_steps = steps;
	CUDAREAL cu_detector_thickstep = detector_thickstep, cu_detector_thick = detector_thick, cu_detector_mu = detector_mu;
	int cu_detector_thicksteps = detector_thicksteps;
	int cu_curved_detector = curved_detector;

	CUDAREAL cu_distance = distance, cu_close_distance = close_distance;

	CUDAREAL cu_Xbeam = Xbeam, cu_Ybeam = Ybeam;
	CUDAREAL cu_dmin = dmin, cu_phi0 = phi0, cu_phistep = phistep;
	int cu_phisteps = phisteps;

	shapetype cu_xtal_shape = xtal_shape;

	int cu_sources = sources;

	CUDAREAL cu_mosaic_spread = mosaic_spread;
	int cu_mosaic_domains = mosaic_domains;

	CUDAREAL cu_Na = Na, cu_Nb = Nb, cu_Nc = Nc, cu_V_cell = V_cell, cu_water_size = water_size, cu_water_F = water_F, cu_water_MW = water_MW;
	CUDAREAL cu_r_e_sqr = r_e_sqr, cu_fluence = fluence, cu_Avogadro = Avogadro, cu_spot_scale = spot_scale;

	int cu_integral_form = integral_form;
	CUDAREAL cu_default_F = default_F;
	int cu_interpolate = interpolate;

//	int cu_h_min = h_min, cu_h_max = h_max, cu_h_range = h_range;
//	int cu_k_min = k_min, cu_k_max = k_max, cu_k_range = k_range;
//	int cu_l_min = l_min, cu_l_max = l_max, cu_l_range = l_range;
//	int cu_hkls = hkls;

	int cu_nopolar = nopolar;
	CUDAREAL cu_polarization = polarization, cu_fudge = fudge;

	hklParams FhklParams = { hkls, h_min, h_max, h_range, k_min, k_max, k_range, l_min, l_max, l_range };
	hklParams * cu_FhklParams;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_FhklParams, sizeof(*cu_FhklParams)));
	CUDA_CHECK_RETURN(hipMemcpy(cu_FhklParams, &FhklParams, sizeof(*cu_FhklParams), hipMemcpyHostToDevice));

	const int vector_length = 4;
	CUDAREAL * cu_sdet_vector;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_sdet_vector, sizeof(*cu_sdet_vector) * vector_length));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_sdet_vector, sdet_vector, vector_length));

	CUDAREAL * cu_fdet_vector;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_fdet_vector, sizeof(*cu_fdet_vector) * vector_length));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_fdet_vector, fdet_vector, vector_length));

	CUDAREAL * cu_odet_vector;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_odet_vector, sizeof(*cu_odet_vector) * vector_length));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_odet_vector, odet_vector, vector_length));

	CUDAREAL * cu_pix0_vector;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_pix0_vector, sizeof(*cu_pix0_vector) * vector_length));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_pix0_vector, pix0_vector, vector_length));

	CUDAREAL * cu_beam_vector;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_beam_vector, sizeof(*cu_beam_vector) * vector_length));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_beam_vector, beam_vector, vector_length));

	CUDAREAL * cu_spindle_vector;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_spindle_vector, sizeof(*cu_spindle_vector) * vector_length));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_spindle_vector, spindle_vector, vector_length));

	CUDAREAL * cu_a0;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_a0, sizeof(*cu_a0) * vector_length));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_a0, a0, vector_length));

	CUDAREAL * cu_b0;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_b0, sizeof(*cu_b0) * vector_length));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_b0, b0, vector_length));

	CUDAREAL * cu_c0;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_c0, sizeof(*cu_c0) * vector_length));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_c0, c0, vector_length));

	//	Unitize polar vector before sending it to the GPU. Optimization do it only once here rather than multiple time per pixel in the GPU.
	CUDAREAL * cu_polar_vector;
	double polar_vector_unitized[4];
	cpu_unitize(polar_vector, polar_vector_unitized);
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_polar_vector, sizeof(*cu_polar_vector) * vector_length));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_polar_vector, polar_vector_unitized, vector_length));

	CUDAREAL * cu_source_X = NULL;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_source_X, sizeof(*cu_source_X) * sources));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_source_X, source_X, sources));

	CUDAREAL * cu_source_Y = NULL;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_source_Y, sizeof(*cu_source_Y) * sources));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_source_Y, source_Y, sources));

	CUDAREAL * cu_source_Z = NULL;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_source_Z, sizeof(*cu_source_Z) * sources));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_source_Z, source_Z, sources));

	CUDAREAL * cu_source_I = NULL;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_source_I, sizeof(*cu_source_I) * sources));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_source_I, source_I, sources));

	CUDAREAL * cu_source_lambda = NULL;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_source_lambda, sizeof(*cu_source_lambda) * sources));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_source_lambda, source_lambda, sources));

	CUDAREAL * cu_mosaic_umats = NULL;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_mosaic_umats, sizeof(*cu_mosaic_umats) * mosaic_domains * 9));
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_mosaic_umats, mosaic_umats, mosaic_domains * 9));

	float * cu_floatimage = NULL;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_floatimage, sizeof(*cu_floatimage) * total_pixels));
	CUDA_CHECK_RETURN(hipMemcpy(cu_floatimage, floatimage, sizeof(*cu_floatimage) * total_pixels, hipMemcpyHostToDevice));

	float * cu_omega_reduction = NULL;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_omega_reduction, sizeof(*cu_omega_reduction) * total_pixels));
	CUDA_CHECK_RETURN(hipMemcpy(cu_omega_reduction, omega_reduction, sizeof(*cu_omega_reduction) * total_pixels, hipMemcpyHostToDevice));

	float * cu_max_I_x_reduction = NULL;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_max_I_x_reduction, sizeof(*cu_max_I_x_reduction) * total_pixels));
	CUDA_CHECK_RETURN(hipMemcpy(cu_max_I_x_reduction, max_I_x_reduction, sizeof(*cu_max_I_x_reduction) * total_pixels, hipMemcpyHostToDevice));

	float * cu_max_I_y_reduction = NULL;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_max_I_y_reduction, sizeof(*cu_max_I_y_reduction) * total_pixels));
	CUDA_CHECK_RETURN(hipMemcpy(cu_max_I_y_reduction, max_I_y_reduction, sizeof(*cu_max_I_y_reduction) * total_pixels, hipMemcpyHostToDevice));

	bool * cu_rangemap = NULL;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_rangemap, sizeof(*cu_rangemap) * total_pixels));
	CUDA_CHECK_RETURN(hipMemcpy(cu_rangemap, rangemap, sizeof(*cu_rangemap) * total_pixels, hipMemcpyHostToDevice));

	int unsigned short * cu_maskimage = NULL;
	if (maskimage != NULL) {
		CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_maskimage, sizeof(*cu_maskimage) * total_pixels));
		CUDA_CHECK_RETURN(hipMemcpy(cu_maskimage, maskimage, sizeof(*cu_maskimage) * total_pixels, hipMemcpyHostToDevice));
	}

	int hklsize = h_range * k_range * l_range;
	CUDAREAL * FhklLinear = (CUDAREAL*) calloc(hklsize, sizeof(*FhklLinear));
	for (int h = 0; h < h_range; h++) {
		for (int k = 0; k < k_range; k++) {
//			memcpy(FhklLinear + (h * k_range * l_range + k * l_range), Fhkl[h][k], sizeof(*FhklLinear) * l_range);
			for (int l = 0; l < l_range; l++) {

				//	convert Fhkl double to CUDAREAL
				FhklLinear[h * k_range * l_range + k * l_range + l] = Fhkl[h][k][l];
			}
		}
	}

	CUDAREAL * cu_Fhkl = NULL;
	CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_Fhkl, sizeof(*cu_Fhkl) * hklsize));
	CUDA_CHECK_RETURN(hipMemcpy(cu_Fhkl, FhklLinear, sizeof(*cu_Fhkl) * hklsize, hipMemcpyHostToDevice));
    free(FhklLinear);

	//int deviceId = 0;
	CUDA_CHECK_RETURN(hipGetDevice(&deviceId));
	hipDeviceProp_t deviceProps = { 0 };
	CUDA_CHECK_RETURN(hipGetDeviceProperties(&deviceProps, deviceId));
	int smCount = deviceProps.multiProcessorCount;

//	CUDA_CHECK_RETURN(hipFuncSetCacheConfig(reinterpret_cast<const void*>(nanoBraggSpotsCUDAKernel), hipFuncCachePreferShared));
//	CUDA_CHECK_RETURN(hipFuncSetCacheConfig(reinterpret_cast<const void*>(nanoBraggSpotsCUDAKernel), hipFuncCachePreferL1));

	dim3 threadsPerBlock(THREADS_PER_BLOCK_X, THREADS_PER_BLOCK_Y);
	//  dim3 numBlocks((spixels - 1) / threadsPerBlock.x + 1, (fpixels - 1) / threadsPerBlock.y + 1);
	dim3 numBlocks(smCount * 8, 1);

	//  initialize the device memory within a kernel.
	//	hipLaunchKernelGGL(nanoBraggSpotsInitCUDAKernel, dim3(numBlocks), dim3(threadsPerBlock), 0, 0, cu_spixels, cu_fpixels, cu_floatimage, cu_omega_reduction, cu_max_I_x_reduction, cu_max_I_y_reduction, cu_rangemap);
	//  CUDA_CHECK_RETURN(hipPeekAtLastError());
	//  CUDA_CHECK_RETURN(hipDeviceSynchronize());

	hipLaunchKernelGGL(nanoBraggSpotsCUDAKernel, dim3(numBlocks), dim3(threadsPerBlock), 0, 0, cu_spixels, cu_fpixels, cu_roi_xmin, cu_roi_xmax, cu_roi_ymin, cu_roi_ymax, cu_oversample,
			cu_point_pixel, cu_pixel_size, cu_subpixel_size, cu_steps, cu_detector_thickstep, cu_detector_thicksteps, cu_detector_thick, cu_detector_mu,
			cu_sdet_vector, cu_fdet_vector, cu_odet_vector, cu_pix0_vector, cu_curved_detector, cu_distance, cu_close_distance, cu_beam_vector,
			cu_Xbeam, cu_Ybeam, cu_dmin, cu_phi0, cu_phistep, cu_phisteps, cu_spindle_vector,
			cu_sources, cu_source_X, cu_source_Y, cu_source_Z, cu_source_I, cu_source_lambda, cu_a0, cu_b0, cu_c0, cu_xtal_shape,
			cu_mosaic_spread, cu_mosaic_domains, cu_mosaic_umats, cu_Na, cu_Nb, cu_Nc, cu_V_cell, cu_water_size, cu_water_F, cu_water_MW, cu_r_e_sqr, cu_fluence, 
			cu_Avogadro, cu_spot_scale, cu_integral_form, cu_default_F, cu_interpolate, cu_Fhkl, cu_FhklParams,
			cu_nopolar, cu_polar_vector, cu_polarization, cu_fudge, cu_maskimage,
			cu_floatimage /*out*/, cu_omega_reduction/*out*/, cu_max_I_x_reduction/*out*/, cu_max_I_y_reduction /*out*/, cu_rangemap /*out*/);

	CUDA_CHECK_RETURN(hipPeekAtLastError());
	//CUDA_CHECK_RETURN(hipDeviceSynchronize());

	CUDA_CHECK_RETURN(hipMemcpy(floatimage, cu_floatimage, sizeof(*cu_floatimage) * total_pixels, hipMemcpyDeviceToHost));
	CUDA_CHECK_RETURN(hipMemcpy(omega_reduction, cu_omega_reduction, sizeof(*cu_omega_reduction) * total_pixels, hipMemcpyDeviceToHost));
	CUDA_CHECK_RETURN(hipMemcpy(max_I_x_reduction, cu_max_I_x_reduction, sizeof(*cu_max_I_x_reduction) * total_pixels, hipMemcpyDeviceToHost));
	CUDA_CHECK_RETURN(hipMemcpy(max_I_y_reduction, cu_max_I_y_reduction, sizeof(*cu_max_I_y_reduction) * total_pixels, hipMemcpyDeviceToHost));
	CUDA_CHECK_RETURN(hipMemcpy(rangemap, cu_rangemap, sizeof(*cu_rangemap) * total_pixels, hipMemcpyDeviceToHost));

	CUDA_CHECK_RETURN(hipFree(cu_sdet_vector));
	CUDA_CHECK_RETURN(hipFree(cu_fdet_vector));
	CUDA_CHECK_RETURN(hipFree(cu_odet_vector));
	CUDA_CHECK_RETURN(hipFree(cu_pix0_vector));
	CUDA_CHECK_RETURN(hipFree(cu_beam_vector));
	CUDA_CHECK_RETURN(hipFree(cu_spindle_vector));
	CUDA_CHECK_RETURN(hipFree(cu_polar_vector));
	CUDA_CHECK_RETURN(hipFree(cu_a0));
	CUDA_CHECK_RETURN(hipFree(cu_b0));
	CUDA_CHECK_RETURN(hipFree(cu_c0));
	CUDA_CHECK_RETURN(hipFree(cu_source_X));
	CUDA_CHECK_RETURN(hipFree(cu_source_Y));
	CUDA_CHECK_RETURN(hipFree(cu_source_Z));
	CUDA_CHECK_RETURN(hipFree(cu_source_I));
	CUDA_CHECK_RETURN(hipFree(cu_source_lambda));
	CUDA_CHECK_RETURN(hipFree(cu_FhklParams));
	CUDA_CHECK_RETURN(hipFree(cu_mosaic_umats));
	CUDA_CHECK_RETURN(hipFree(cu_floatimage));
	CUDA_CHECK_RETURN(hipFree(cu_omega_reduction));
	CUDA_CHECK_RETURN(hipFree(cu_max_I_x_reduction));
	CUDA_CHECK_RETURN(hipFree(cu_max_I_y_reduction));
	CUDA_CHECK_RETURN(hipFree(cu_maskimage));
	CUDA_CHECK_RETURN(hipFree(cu_rangemap));
    CUDA_CHECK_RETURN(hipFree(cu_Fhkl));

	*max_I = 0;
	*max_I_x = 0;
	*max_I_y = 0;
	*sum = 0.0;
	*sumsqr = 0.0;
	*sumn = 0;
	*omega_sum = 0.0;

	for (int i = 0; i < total_pixels; i++) {
		if (!rangemap[i]) {
			continue;
		}
		float pixel = floatimage[i];
		if (pixel > (double) *max_I) {
			*max_I = pixel;
			*max_I_x = max_I_x_reduction[i];
			*max_I_y = max_I_y_reduction[i];
		}
		*sum += pixel;
		*sumsqr += pixel * pixel;
		++(*sumn);
		*omega_sum += omega_reduction[i];
	}
	free(rangemap);
	free(omega_reduction);
	free(max_I_x_reduction);
	free(max_I_y_reduction);
}

/* cubic spline interpolation functions */
__device__ static void polint(const CUDAREAL *xa, const CUDAREAL *ya, CUDAREAL x, CUDAREAL *y);
__device__ static void polin2(CUDAREAL *x1a, CUDAREAL *x2a, CUDAREAL ya[4][4], CUDAREAL x1, CUDAREAL x2, CUDAREAL *y);
__device__ static void polin3(CUDAREAL *x1a, CUDAREAL *x2a, CUDAREAL *x3a, CUDAREAL ya[4][4][4], CUDAREAL x1, CUDAREAL x2, CUDAREAL x3, CUDAREAL *y);
/* rotate a 3-vector about a unit vector axis */
__device__ static CUDAREAL *rotate_axis(const CUDAREAL * __restrict__ v, CUDAREAL *newv, const CUDAREAL * __restrict__ axis, const CUDAREAL phi);
__device__ static CUDAREAL *rotate_axis_ldg(const CUDAREAL * __restrict__ v, CUDAREAL * newv, const CUDAREAL * __restrict__ axis, const CUDAREAL phi);
/* make a unit vector pointing in same direction and report magnitude (both args can be same vector) */
__device__ static CUDAREAL unitize(CUDAREAL * vector, CUDAREAL *new_unit_vector);
/* vector cross product where vector magnitude is 0th element */
__device__ static CUDAREAL *cross_product(CUDAREAL * x, CUDAREAL * y, CUDAREAL * z);
/* vector inner product where vector magnitude is 0th element */
__device__ static CUDAREAL dot_product(const CUDAREAL * x, const CUDAREAL * y);
__device__ static CUDAREAL dot_product_ldg(const CUDAREAL * __restrict__ x, CUDAREAL * y);
/* measure magnitude of vector and put it in 0th element */
__device__ static void magnitude(CUDAREAL *vector);
/* scale the magnitude of a vector */
__device__ static CUDAREAL vector_scale(CUDAREAL *vector, CUDAREAL *new_vector, CUDAREAL scale);
/* rotate a 3-vector using a 9-element unitary matrix */
__device__ void rotate_umat_ldg(CUDAREAL * v, CUDAREAL *newv, const CUDAREAL * __restrict__ umat);
/* Fourier transform of a truncated lattice */
__device__ static CUDAREAL sincg(CUDAREAL x, CUDAREAL N);
//__device__ static CUDAREAL sincgrad(CUDAREAL x, CUDAREAL N);
/* Fourier transform of a sphere */
__device__ static CUDAREAL sinc3(CUDAREAL x);
/* polarization factor from vectors */
__device__ static CUDAREAL polarization_factor(CUDAREAL kahn_factor, CUDAREAL *incident, CUDAREAL *diffracted, const CUDAREAL * __restrict__ axis);

__device__ __inline__ static int flatten3dindex(int x, int y, int z, int x_range, int y_range, int z_range);

__device__ __inline__ CUDAREAL quickFcell_ldg(int hkls, int h_max, int h_min, int k_max, int k_min, int l_min, int l_max, int h0, int k0, int l0, int h_range, int k_range, int l_range, CUDAREAL defaultF, const CUDAREAL * __restrict__ Fhkl);

__global__ void add_array_CUDAKernel(double * lhs, float * rhs, int array_size){
  const int total_pixels = array_size;
  const int fstride = gridDim.x * blockDim.x;
  const int sstride = gridDim.y * blockDim.y;
  const int stride = fstride * sstride;
  for (int pixIdx = (blockDim.y * blockIdx.y + threadIdx.y) * fstride + blockDim.x * blockIdx.x + threadIdx.x;
       pixIdx < total_pixels; pixIdx += stride) {
    /* position in pixel array */
    const int j = pixIdx;
    lhs[j] = lhs[j] + (double)rhs[j]; // specifically add low precision array to high precision array
    }
  }

__global__ void scale_array_CUDAKernel(double scale_factor, double * lhs, int array_size){
  const int total_pixels = array_size;
  const int fstride = gridDim.x * blockDim.x;
  const int sstride = gridDim.y * blockDim.y;
  const int stride = fstride * sstride;
  for (int pixIdx = (blockDim.y * blockIdx.y + threadIdx.y) * fstride + blockDim.x * blockIdx.x + threadIdx.x;
       pixIdx < total_pixels; pixIdx += stride) {
    /* position in pixel array */
    const int j = pixIdx;
    lhs[j] = lhs[j] * scale_factor;
    }
}

__global__ void nanoBraggSpotsInitCUDAKernel(int spixels, int fpixels, float * floatimage, float * omega_reduction, float * max_I_x_reduction,
		float * max_I_y_reduction, bool * rangemap) {

	const int total_pixels = spixels * fpixels;
	const int fstride = gridDim.x * blockDim.x;
	const int sstride = gridDim.y * blockDim.y;
	const int stride = fstride * sstride;

	for (int pixIdx = (blockDim.y * blockIdx.y + threadIdx.y) * fstride + blockDim.x * blockIdx.x + threadIdx.x; pixIdx < total_pixels; pixIdx += stride) {
		const int fpixel = pixIdx % fpixels;
		const int spixel = pixIdx / fpixels;

		/* position in pixel array */
		int j = spixel * fpixels + fpixel;

		if (j < total_pixels) {
			floatimage[j] = 0;
			omega_reduction[j] = 0;
			max_I_x_reduction[j] = 0;
			max_I_y_reduction[j] = 0;
			rangemap[j] = false;
		}
	}
}

__global__ void nanoBraggSpotsCUDAKernel(int spixels, int fpixels, int roi_xmin, int roi_xmax, int roi_ymin, int roi_ymax, int oversample, int point_pixel,
CUDAREAL pixel_size, CUDAREAL subpixel_size, int steps, CUDAREAL detector_thickstep, int detector_thicksteps, CUDAREAL detector_thick, CUDAREAL detector_mu,
		const CUDAREAL * __restrict__ sdet_vector, const CUDAREAL * __restrict__ fdet_vector, const CUDAREAL * __restrict__ odet_vector,
		const CUDAREAL * __restrict__ pix0_vector, int curved_detector, CUDAREAL distance, CUDAREAL close_distance, const CUDAREAL * __restrict__ beam_vector,
		CUDAREAL Xbeam, CUDAREAL Ybeam, CUDAREAL dmin, CUDAREAL phi0, CUDAREAL phistep, int phisteps, const CUDAREAL * __restrict__ spindle_vector, int sources,
		const CUDAREAL * __restrict__ source_X, const CUDAREAL * __restrict__ source_Y, const CUDAREAL * __restrict__ source_Z,
		const CUDAREAL * __restrict__ source_I, const CUDAREAL * __restrict__ source_lambda, const CUDAREAL * __restrict__ a0, const CUDAREAL * __restrict__ b0,
		const CUDAREAL * __restrict c0, shapetype xtal_shape, CUDAREAL mosaic_spread, int mosaic_domains, const CUDAREAL * __restrict__ mosaic_umats,
		CUDAREAL Na, CUDAREAL Nb, CUDAREAL Nc, CUDAREAL V_cell, CUDAREAL water_size, CUDAREAL water_F, CUDAREAL water_MW, CUDAREAL r_e_sqr, CUDAREAL fluence,
		CUDAREAL Avogadro, CUDAREAL spot_scale, int integral_form, CUDAREAL default_F, int interpolate, const CUDAREAL * __restrict__ Fhkl, const hklParams * __restrict__ FhklParams, int nopolar, const CUDAREAL * __restrict__ polar_vector,
		CUDAREAL polarization, CUDAREAL fudge, const int unsigned short * __restrict__ maskimage, float * floatimage /*out*/, float * omega_reduction/*out*/,
		float * max_I_x_reduction/*out*/, float * max_I_y_reduction /*out*/, bool * rangemap) {

	__shared__ CUDAREAL s_dmin;

	__shared__ bool s_nopolar;

	__shared__ int s_phisteps;
	__shared__ CUDAREAL s_phi0, s_phistep;
	__shared__ int s_mosaic_domains;
	__shared__ CUDAREAL s_mosaic_spread;
	__shared__ shapetype s_xtal_shape;

	__shared__ CUDAREAL s_Na, s_Nb, s_Nc;
	__shared__ bool s_interpolate;
	__shared__ int s_hkls, s_h_max, s_h_min, s_k_max, s_k_min, s_l_max, s_l_min, s_h_range, s_k_range, s_l_range;

	if (threadIdx.x == 0 && threadIdx.y == 0) {

		s_dmin = dmin;

		s_nopolar = nopolar;

		s_phisteps = phisteps;
		s_phi0 = phi0;
		s_phistep = phistep;

		s_mosaic_domains = mosaic_domains;
		s_mosaic_spread = mosaic_spread;

		s_xtal_shape = xtal_shape;
		s_Na = Na;
		s_Nb = Nb;
		s_Nc = Nc;

		s_interpolate = interpolate;

		s_hkls = FhklParams->hkls;
		s_h_max = FhklParams->h_max;
		s_h_min = FhklParams->h_min;
		s_k_max = FhklParams->k_max;
		s_k_min = FhklParams->k_min;
		s_l_max = FhklParams->l_max;
		s_l_min = FhklParams->l_min;
		s_h_range = FhklParams->h_range;
		s_k_range = FhklParams->k_range;
		s_l_range = FhklParams->l_range;

	}
	__syncthreads();

	const int total_pixels = spixels * fpixels;
	const int fstride = gridDim.x * blockDim.x;
	const int sstride = gridDim.y * blockDim.y;
	const int stride = fstride * sstride;
//	const int tidx = blockDim.x * threadIdx.y * +threadIdx.x;

//	__shared__ int sharedVectors[THREADS_PER_BLOCK_TOTAL + 1][1][9];
//	__shared__ CUDAREAL sharedVectors[THREADS_PER_BLOCK_TOTAL + 1][1][VECTOR_SIZE];
//	CUDAREAL * tmpVector1 = sharedVectors[tidx][0];
//	CUDAREAL * tmpVector2 = sharedVectors[tidx][1];

	/* add background from something amorphous */
	CUDAREAL F_bg = water_F;
	CUDAREAL I_bg = F_bg * F_bg * r_e_sqr * fluence * water_size * water_size * water_size * 1e6 * Avogadro / water_MW;

//	hklParams[0] = h_min;
//	hklParams[1] = h_max;
//	hklParams[2] = h_range;
//	hklParams[3] = k_min;
//	hklParams[4] = k_max;
//	hklParams[5] = k_range;
//	hklParams[6] = l_min;
//	hklParams[7] = l_max;
//	hklParams[8] = l_range;

	for (int pixIdx = (blockDim.y * blockIdx.y + threadIdx.y) * fstride + blockDim.x * blockIdx.x + threadIdx.x; pixIdx < total_pixels; pixIdx += stride) {
		const int fpixel = pixIdx % fpixels;
		const int spixel = pixIdx / fpixels;

		/* allow for just one part of detector to be rendered */
		if (fpixel < roi_xmin || fpixel > roi_xmax || spixel < roi_ymin || spixel > roi_ymax) { //ROI region of interest
			continue;
		}

		/* position in pixel array */
		const int j = pixIdx;

		/* allow for the use of a mask */
		if (maskimage != NULL) {
			/* skip any flagged pixels in the mask */
			if (maskimage[j] == 0) {
				continue;
			}
		}

		/* reset photon count for this pixel */
		CUDAREAL I = I_bg;
		CUDAREAL omega_sub_reduction = 0.0;
		CUDAREAL max_I_x_sub_reduction = 0.0;
		CUDAREAL max_I_y_sub_reduction = 0.0;
		CUDAREAL polar = 0.0;
		if (s_nopolar) {
			polar = 1.0;
		}

		/* add this now to avoid problems with skipping later */
		// move this to the bottom to avoid accessing global device memory. floatimage[j] = I_bg;
		/* loop over sub-pixels */
		int subS, subF;
		for (subS = 0; subS < oversample; ++subS) { // Y voxel
			for (subF = 0; subF < oversample; ++subF) { // X voxel
				/* absolute mm position on detector (relative to its origin) */
				CUDAREAL Fdet = subpixel_size * (fpixel * oversample + subF) + subpixel_size / 2.0; // X voxel
				CUDAREAL Sdet = subpixel_size * (spixel * oversample + subS) + subpixel_size / 2.0; // Y voxel
				//                  Fdet = pixel_size*fpixel;
				//                  Sdet = pixel_size*spixel;

				max_I_x_sub_reduction = Fdet;
				max_I_y_sub_reduction = Sdet;

				int thick_tic;
				for (thick_tic = 0; thick_tic < detector_thicksteps; ++thick_tic) {
					/* assume "distance" is to the front of the detector sensor layer */
					CUDAREAL Odet = thick_tic * detector_thickstep; // Z Orthagonal voxel.

					/* construct detector subpixel position in 3D space */
					//                      pixel_X = distance;
					//                      pixel_Y = Sdet-Ybeam;
					//                      pixel_Z = Fdet-Xbeam;
					//CUDAREAL * pixel_pos = tmpVector1;
					CUDAREAL pixel_pos[4];
					pixel_pos[1] = Fdet * __ldg(&fdet_vector[1]) + Sdet * __ldg(&sdet_vector[1]) + Odet * __ldg(&odet_vector[1]) + __ldg(&pix0_vector[1]); // X
					pixel_pos[2] = Fdet * __ldg(&fdet_vector[2]) + Sdet * __ldg(&sdet_vector[2]) + Odet * __ldg(&odet_vector[2]) + __ldg(&pix0_vector[2]); // X
					pixel_pos[3] = Fdet * __ldg(&fdet_vector[3]) + Sdet * __ldg(&sdet_vector[3]) + Odet * __ldg(&odet_vector[3]) + __ldg(&pix0_vector[3]); // X
//					pixel_pos[1] = Fdet * fdet_vector[1] + Sdet * sdet_vector[1] + Odet * odet_vector[1] + pix0_vector[1]; // X
//					pixel_pos[2] = Fdet * fdet_vector[2] + Sdet * sdet_vector[2] + Odet * odet_vector[2] + pix0_vector[2]; // Y
//					pixel_pos[3] = Fdet * fdet_vector[3] + Sdet * sdet_vector[3] + Odet * odet_vector[3] + pix0_vector[3]; // Z
					if (curved_detector) {
						/* construct detector pixel that is always "distance" from the sample */
						CUDAREAL dbvector[4];
						dbvector[1] = distance * beam_vector[1];
						dbvector[2] = distance * beam_vector[2];
						dbvector[3] = distance * beam_vector[3];
						/* treat detector pixel coordinates as radians */
						CUDAREAL newvector[] = { 0.0, 0.0, 0.0, 0.0 };
						rotate_axis(dbvector, newvector, sdet_vector, pixel_pos[2] / distance);
						rotate_axis(newvector, pixel_pos, fdet_vector, pixel_pos[3] / distance);
						//                          rotate(vector,pixel_pos,0,pixel_pos[3]/distance,pixel_pos[2]/distance);
					}
					/* construct the diffracted-beam unit vector to this sub-pixel */
					//CUDAREAL * diffracted = tmpVector2;
					CUDAREAL diffracted[4];
					CUDAREAL airpath = unitize(pixel_pos, diffracted);

					/* solid angle subtended by a pixel: (pix/airpath)^2*cos(2theta) */
					CUDAREAL omega_pixel = pixel_size * pixel_size / airpath / airpath * close_distance / airpath;
					/* option to turn off obliquity effect, inverse-square-law only */
					if (point_pixel) {
						omega_pixel = 1.0 / airpath / airpath;
					}

					/* now calculate detector thickness effects */
					CUDAREAL capture_fraction = 1.0;
					if (detector_thick > 0.0 && detector_mu> 0.0) {
						/* inverse of effective thickness increase */
						CUDAREAL parallax = dot_product_ldg(odet_vector, diffracted);
						capture_fraction = exp(-thick_tic * detector_thickstep / detector_mu / parallax)
								- exp(-(thick_tic + 1) * detector_thickstep / detector_mu / parallax);
					}

					/* loop over sources now */
					int source;
					for (source = 0; source < sources; ++source) {

						/* retrieve stuff from cache */
						//CUDAREAL * incident = tmpVector1;
						CUDAREAL incident[4];
						incident[1] = -__ldg(&source_X[source]);
						incident[2] = -__ldg(&source_Y[source]);
						incident[3] = -__ldg(&source_Z[source]);
						CUDAREAL lambda = __ldg(&source_lambda[source]);
						CUDAREAL source_fraction = __ldg(&source_I[source]);

						/* construct the incident beam unit vector while recovering source distance */
						// TODO[Giles]: Optimization! We can unitize the source vectors before passing them in.
						unitize(incident, incident);
//						CUDAREAL source_path = unitize(incident, incident);
//						CUDAREAL source_path = norm3d(incident[1], incident[2], incident[3]);

//						CUDAREAL * d = tmpVector2;
//						d[0] = diffracted[0];
//						d[1] = diffracted[1];
//						d[2] = diffracted[2];
//						d[3] = diffracted[3];

						/* construct the scattering vector for this pixel */
//						CUDAREAL * scattering = tmpVector1;
						CUDAREAL scattering[4];
						scattering[1] = (diffracted[1] - incident[1]) / lambda;
						scattering[2] = (diffracted[2] - incident[2]) / lambda;
						scattering[3] = (diffracted[3] - incident[3]) / lambda;
//						CUDAREAL scattering[] = { 0.0, (diffracted[1] - incident[1]) / lambda, (diffracted[2] - incident[2]) / lambda, (diffracted[3]
//								- incident[3]) / lambda };

						/* sin(theta)/lambda is half the scattering vector length */
//						magnitude(scattering);
//						CUDAREAL stol = 0.5 * scattering[0];
						CUDAREAL stol = 0.5 * norm3d(scattering[1], scattering[2], scattering[3]);

						/* rough cut to speed things up when we aren't using whole detector */
						if (s_dmin > 0.0 && stol > 0.0) {
							if (s_dmin > 0.5 / stol) {
								continue;
							}
						}

						/* polarization factor */
						if (!s_nopolar) {
							/* need to compute polarization factor */
							polar = polarization_factor(polarization, incident, diffracted, polar_vector);
						} else {
							polar = 1.0;
						}

						/* sweep over phi angles */
						for (int phi_tic = 0; phi_tic < s_phisteps; ++phi_tic) {
							CUDAREAL phi = s_phistep * phi_tic + s_phi0;

//							CUDAREAL ap[] = { 0.0, 0.0, 0.0, 0.0 };
//							CUDAREAL bp[] = { 0.0, 0.0, 0.0, 0.0 };
//							CUDAREAL cp[] = { 0.0, 0.0, 0.0, 0.0 };
							CUDAREAL ap[4];
							CUDAREAL bp[4];
							CUDAREAL cp[4];

							/* rotate about spindle if necessary */
							rotate_axis_ldg(a0, ap, spindle_vector, phi);
							rotate_axis_ldg(b0, bp, spindle_vector, phi);
							rotate_axis_ldg(c0, cp, spindle_vector, phi);

							/* enumerate mosaic domains */
							for (int mos_tic = 0; mos_tic < s_mosaic_domains; ++mos_tic) {
								/* apply mosaic rotation after phi rotation */
								CUDAREAL a[4];
								CUDAREAL b[4];
								CUDAREAL c[4];
								if (s_mosaic_spread > 0.0) {
									rotate_umat_ldg(ap, a, &mosaic_umats[mos_tic * 9]);
									rotate_umat_ldg(bp, b, &mosaic_umats[mos_tic * 9]);
									rotate_umat_ldg(cp, c, &mosaic_umats[mos_tic * 9]);
								} else {
									a[1] = ap[1];
									a[2] = ap[2];
									a[3] = ap[3];
									b[1] = bp[1];
									b[2] = bp[2];
									b[3] = bp[3];
									c[1] = cp[1];
									c[2] = cp[2];
									c[3] = cp[3];
								}
								//                                  printf("%d %f %f %f\n",mos_tic,mosaic_umats[mos_tic*9+0],mosaic_umats[mos_tic*9+1],mosaic_umats[mos_tic*9+2]);
								//                                  printf("%d %f %f %f\n",mos_tic,mosaic_umats[mos_tic*9+3],mosaic_umats[mos_tic*9+4],mosaic_umats[mos_tic*9+5]);
								//                                  printf("%d %f %f %f\n",mos_tic,mosaic_umats[mos_tic*9+6],mosaic_umats[mos_tic*9+7],mosaic_umats[mos_tic*9+8]);

								/* construct fractional Miller indicies */

//								CUDAREAL * scat_s = tmpVector2;
//								scat_s[0] = scattering[0];
//								scat_s[1] = scattering[1];
//								scat_s[2] = scattering[2];
//								scat_s[3] = scattering[3];
//
//								CUDAREAL h = dot_product(a, scat_s);
//								CUDAREAL k = dot_product(b, scat_s);
//								CUDAREAL l = dot_product(c, scat_s);
								CUDAREAL h = dot_product(a, scattering);
								CUDAREAL k = dot_product(b, scattering);
								CUDAREAL l = dot_product(c, scattering);

								/* round off to nearest whole index */
								int h0 = ceil(h - 0.5);
								int k0 = ceil(k - 0.5);
								int l0 = ceil(l - 0.5);

								/* structure factor of the lattice (paralelpiped crystal)
								 F_latt = sin(M_PI*Na*h)*sin(M_PI*Nb*k)*sin(M_PI*Nc*l)/sin(M_PI*h)/sin(M_PI*k)/sin(M_PI*l);
								 */
								CUDAREAL F_latt = 1.0; // Shape transform for the crystal.
								CUDAREAL hrad_sqr = 0.0;
								if (s_xtal_shape == SQUARE) {
									/* xtal is a paralelpiped */
									if (Na > 1) {
//										F_latt *= sincgrad(h, s_Na);
										F_latt *= sincg(M_PI * h, s_Na);
									}
									if (Nb > 1) {
//										F_latt *= sincgrad(k, s_Nb);
										F_latt *= sincg(M_PI * k, s_Nb);
									}
									if (Nc > 1) {
//										F_latt *= sincgrad(l, s_Nc);
										F_latt *= sincg(M_PI * l, s_Nc);
									}
								} else {
									/* handy radius in reciprocal space, squared */
									hrad_sqr = (h - h0) * (h - h0) * Na * Na + (k - k0) * (k - k0) * Nb * Nb + (l - l0) * (l - l0) * Nc * Nc;
								}
								if (s_xtal_shape == ROUND) {
									/* use sinc3 for elliptical xtal shape,
									 correcting for sqrt of volume ratio between cube and sphere */
									F_latt = Na * Nb * Nc * 0.723601254558268 * sinc3(M_PI * sqrt(hrad_sqr * fudge));
								}
								if (s_xtal_shape == GAUSS) {
									/* fudge the radius so that volume and FWHM are similar to square_xtal spots */
									F_latt = Na * Nb * Nc * exp(-(hrad_sqr / 0.63 * fudge));
								}
								if (s_xtal_shape == TOPHAT) {
									/* make a flat-top spot of same height and volume as square_xtal spots */
									F_latt = Na * Nb * Nc * (hrad_sqr * fudge < 0.3969);
								}
								/* no need to go further if result will be zero? */
								if (F_latt == 0.0 && water_size == 0.0)
									continue;

								/* find nearest point on Ewald sphere surface? */
								if (integral_form) {

									/* need to calculate reciprocal matrix */
									/* various cross products */
									CUDAREAL a_cross_b[] = { 0.0, 0.0, 0.0, 0.0 };
									CUDAREAL b_cross_c[] = { 0.0, 0.0, 0.0, 0.0 };
									CUDAREAL c_cross_a[] = { 0.0, 0.0, 0.0, 0.0 };
									cross_product(a, b, a_cross_b);
									cross_product(b, c, b_cross_c);
									cross_product(c, a, c_cross_a);

									/* new reciprocal-space cell vectors */
									CUDAREAL a_star[] = { 0.0, 0.0, 0.0, 0.0 };
									CUDAREAL b_star[] = { 0.0, 0.0, 0.0, 0.0 };
									CUDAREAL c_star[] = { 0.0, 0.0, 0.0, 0.0 };
									vector_scale(b_cross_c, a_star, 1e20 / V_cell);
									vector_scale(c_cross_a, b_star, 1e20 / V_cell);
									vector_scale(a_cross_b, c_star, 1e20 / V_cell);

									/* reciprocal-space coordinates of nearest relp */
									CUDAREAL relp[] = { 0.0, 0.0, 0.0, 0.0 };
									relp[1] = h0 * a_star[1] + k0 * b_star[1] + l0 * c_star[1];
									relp[2] = h0 * a_star[2] + k0 * b_star[2] + l0 * c_star[2];
									relp[3] = h0 * a_star[3] + k0 * b_star[3] + l0 * c_star[3];
									//                                      d_star = magnitude(relp)

									/* reciprocal-space coordinates of center of Ewald sphere */
									CUDAREAL Ewald0[] = { 0.0, 0.0, 0.0, 0.0 };
									Ewald0[1] = -incident[1] / lambda / 1e10;
									Ewald0[2] = -incident[2] / lambda / 1e10;
									Ewald0[3] = -incident[3] / lambda / 1e10;
									//                                      1/lambda = magnitude(Ewald0)

									/* distance from Ewald sphere in lambda=1 units */
									CUDAREAL dEwald0[] = { 0.0, 0.0, 0.0, 0.0 };
									dEwald0[1] = relp[1] - Ewald0[1];
									dEwald0[2] = relp[2] - Ewald0[2];
									dEwald0[3] = relp[3] - Ewald0[3];
									magnitude(dEwald0);
									CUDAREAL d_r = dEwald0[0] - 1.0;

									/* unit vector of diffracted ray through relp */
									CUDAREAL diffracted0[] = { 0.0, 0.0, 0.0, 0.0 };
									unitize(dEwald0, diffracted0);

									/* intersection with detector plane */
									CUDAREAL xd = dot_product_ldg(fdet_vector, diffracted0);
									CUDAREAL yd = dot_product_ldg(sdet_vector, diffracted0);
									CUDAREAL zd = dot_product_ldg(odet_vector, diffracted0);

									/* where does the central direct-beam hit */
									CUDAREAL xd0 = dot_product_ldg(fdet_vector, incident);
									CUDAREAL yd0 = dot_product_ldg(sdet_vector, incident);
									CUDAREAL zd0 = dot_product_ldg(odet_vector, incident);

									/* convert to mm coordinates */
									CUDAREAL Fdet0 = distance * (xd / zd) + Xbeam;
									CUDAREAL Sdet0 = distance * (yd / zd) + Ybeam;

									//printf("GOTHERE %g %g   %g %g\n",Fdet,Sdet,Fdet0,Sdet0);
									CUDAREAL test = exp(-((Fdet - Fdet0) * (Fdet - Fdet0) + (Sdet - Sdet0) * (Sdet - Sdet0) + d_r * d_r) / 1e-8);
								} // end of integral form

								/* structure factor of the unit cell */
								CUDAREAL F_cell = default_F;
								if (s_interpolate) {
									int h0_flr = floor(h);
									int k0_flr = floor(k);
									int l0_flr = floor(l);

									if (((h - s_h_min + 3) > s_h_range) || (h - 2 < s_h_min) || ((k - s_k_min + 3) > s_k_range) || (k - 2 < s_k_min)
											|| ((l - s_l_min + 3) > s_l_range) || (l - 2 < s_l_min)) {
										//											if (babble) {
										//												babble = 0;
										//												printf("WARNING: out of range for three point interpolation: h,k,l,h0,k0,l0: %g,%g,%g,%d,%d,%d \n", h, k, l, h0,
										//														k0, l0);
										//												printf("WARNING: further warnings will not be printed! ");
										//											}
										F_cell = quickFcell_ldg(s_hkls, s_h_max, s_h_min, s_k_max, s_k_min, s_l_max, s_l_min, h0, k0, l0, s_h_range, s_k_range, s_l_range, default_F, Fhkl);
									} else {
										/* integer versions of nearest HKL indicies */
										int h_interp[] = { 0, 0, 0, 0 };
										int k_interp[] = { 0, 0, 0, 0 };
										int l_interp[] = { 0, 0, 0, 0 };
										h_interp[0] = h0_flr - 1;
										h_interp[1] = h0_flr;
										h_interp[2] = h0_flr + 1;
										h_interp[3] = h0_flr + 2;
										k_interp[0] = k0_flr - 1;
										k_interp[1] = k0_flr;
										k_interp[2] = k0_flr + 1;
										k_interp[3] = k0_flr + 2;
										l_interp[0] = l0_flr - 1;
										l_interp[1] = l0_flr;
										l_interp[2] = l0_flr + 1;
										l_interp[3] = l0_flr + 2;

										/* polin function needs doubles */
										CUDAREAL h_interp_d[] = { 0.0, 0.0, 0.0, 0.0 };
										CUDAREAL k_interp_d[] = { 0.0, 0.0, 0.0, 0.0 };
										CUDAREAL l_interp_d[] = { 0.0, 0.0, 0.0, 0.0 };
										h_interp_d[0] = (CUDAREAL) h_interp[0];
										h_interp_d[1] = (CUDAREAL) h_interp[1];
										h_interp_d[2] = (CUDAREAL) h_interp[2];
										h_interp_d[3] = (CUDAREAL) h_interp[3];
										k_interp_d[0] = (CUDAREAL) k_interp[0];
										k_interp_d[1] = (CUDAREAL) k_interp[1];
										k_interp_d[2] = (CUDAREAL) k_interp[2];
										k_interp_d[3] = (CUDAREAL) k_interp[3];
										l_interp_d[0] = (CUDAREAL) l_interp[0];
										l_interp_d[1] = (CUDAREAL) l_interp[1];
										l_interp_d[2] = (CUDAREAL) l_interp[2];
										l_interp_d[3] = (CUDAREAL) l_interp[3];

										/* now populate the "y" values (nearest four structure factors in each direction) */
										CUDAREAL sub_Fhkl[4][4][4];
										int i1, i2, i3;
										for (i1 = 0; i1 < 4; i1++) {
											for (i2 = 0; i2 < 4; i2++) {
												for (i3 = 0; i3 < 4; i3++) {
													sub_Fhkl[i1][i2][i3] = __ldg(
															&Fhkl[flatten3dindex(h_interp[i1] - s_h_min, k_interp[i2] - s_k_min, l_interp[i3] - s_l_min, s_h_range,
																	s_k_range, s_l_range)]);
												}
											}
										}

										/* run the tricubic polynomial interpolation */
										polin3(h_interp_d, k_interp_d, l_interp_d, sub_Fhkl, h, k, l, &F_cell);
									}
								} else {
//								if (!interpolate) {
//									if (hkls && (h0 <= hklParams[1]) && (h0 >= hklParams[0]) && (k0 <= hklParams[4]) && (k0 >= hklParams[3]) && (l0 <= hklParams[7]) && (l0 >= hklParams[6])) {
//										/* just take nearest-neighbor */
//										F_cell = __ldg(&Fhkl[flatten3dindex(h0 - hklParams[0], k0 - hklParams[3], l0 - hklParams[6], hklParams[2], hklParams[5], hklParams[8])]);
//									} else {
//										F_cell = default_F;  // usually zero
//									}
//								}
									F_cell = quickFcell_ldg(s_hkls, s_h_max, s_h_min, s_k_max, s_k_min, s_l_max, s_l_min, h0, k0, l0, s_h_range, s_k_range, s_l_range, default_F, Fhkl);
//									if (s_hkls && (h0 <= s_h_max) && (h0 >= s_h_min) && (k0 <= s_k_max) && (k0 >= s_k_min) && (l0 <= s_l_max) && (l0 >= s_l_min)) {
//										/* just take nearest-neighbor */
//										F_cell = __ldg(&Fhkl[flatten3dindex(h0 - s_h_min, k0 - s_k_min, l0 - s_l_min, s_h_range, s_k_range, s_l_range)]);
////										F_cell = __ldg(&Fhkl[flatten3dindex(h0 - __ldg(&FhklParams->h_min), k0 - __ldg(&FhklParams->k_min), l0 - __ldg(&FhklParams->l_min), s_h_range, s_k_range, s_l_range)]);
////										F_cell = __ldg(&Fhkl[flatten3dindex(h0 - FhklParams->h_min, k0 - FhklParams->k_min, l0 - FhklParams->l_min, FhklParams->h_range, FhklParams->k_range, FhklParams->l_range)]);
//									}
								}

								/* now we have the structure factor for this pixel */

								/* convert amplitudes into intensity (photons per steradian) */
								I += F_cell * F_cell * F_latt * F_latt * source_fraction * capture_fraction * omega_pixel;
								omega_sub_reduction += omega_pixel;
							}
							/* end of mosaic loop */
						}
						/* end of phi loop */
					}
					/* end of source loop */
				}
				/* end of detector thickness loop */
			}
			/* end of sub-pixel y loop */
		}
		/* end of sub-pixel x loop */
		const double photons = I_bg + (r_e_sqr * spot_scale * fluence * polar * I) / steps;
		floatimage[j] = photons;
		omega_reduction[j] = omega_sub_reduction; // shared contention
		max_I_x_reduction[j] = max_I_x_sub_reduction;
		max_I_y_reduction[j] = max_I_y_sub_reduction;
		rangemap[j] = true;
	}
}

__device__ __inline__ CUDAREAL quickFcell_ldg(int hkls, int h_max, int h_min, int k_max, int k_min, int l_max, int l_min, int h0, int k0, int l0, int h_range, int k_range, int l_range, CUDAREAL defaultF, const CUDAREAL * __restrict__ Fhkl) {
	if (hkls && (h0 <= h_max) && (h0 >= h_min) && (k0 <= k_max) && (k0 >= k_min) && (l0 <= l_max) && (l0 >= l_min)) {
		/* just take nearest-neighbor */
//      F_cell = __ldg(&Fhkl[flatten3dindex(h0 - s_h_min, k0 - s_k_min, l0 - s_l_min, s_h_range, s_k_range, s_l_range)]);
		return __ldg(&Fhkl[flatten3dindex(h0 - h_min, k0 - k_min, l0 - l_min, h_range, k_range, l_range)]);
	} else {
		return defaultF;  // usually zero
	}
}

__device__ __inline__ int flatten3dindex(int x, int y, int z, int x_range, int y_range, int z_range) {
	return x * y_range * z_range + y * z_range + z;
}

/* rotate a point about a unit vector axis */
__device__ CUDAREAL *rotate_axis(const CUDAREAL * __restrict__ v, CUDAREAL * newv, const CUDAREAL * __restrict__ axis, const CUDAREAL phi) {

	const CUDAREAL sinphi = sin(phi);
	const CUDAREAL cosphi = cos(phi);
	const CUDAREAL a1 = axis[1];
	const CUDAREAL a2 = axis[2];
	const CUDAREAL a3 = axis[3];
	const CUDAREAL v1 = v[1];
	const CUDAREAL v2 = v[2];
	const CUDAREAL v3 = v[3];
	const CUDAREAL dot = (a1 * v1 + a2 * v2 + a3 * v3) * (1.0 - cosphi);

	newv[1] = a1 * dot + v1 * cosphi + (-a3 * v2 + a2 * v3) * sinphi;
	newv[2] = a2 * dot + v2 * cosphi + (+a3 * v1 - a1 * v3) * sinphi;
	newv[3] = a3 * dot + v3 * cosphi + (-a2 * v1 + a1 * v2) * sinphi;

	return newv;
}

/* rotate a point about a unit vector axis */
__device__ CUDAREAL *rotate_axis_ldg(const CUDAREAL * __restrict__ v, CUDAREAL * newv, const CUDAREAL * __restrict__ axis, const CUDAREAL phi) {

	const CUDAREAL sinphi = sin(phi);
	const CUDAREAL cosphi = cos(phi);
	const CUDAREAL a1 = __ldg(&axis[1]);
	const CUDAREAL a2 = __ldg(&axis[2]);
	const CUDAREAL a3 = __ldg(&axis[3]);
	const CUDAREAL v1 = __ldg(&v[1]);
	const CUDAREAL v2 = __ldg(&v[2]);
	const CUDAREAL v3 = __ldg(&v[3]);
	const CUDAREAL dot = (a1 * v1 + a2 * v2 + a3 * v3) * (1.0 - cosphi);

	newv[1] = a1 * dot + v1 * cosphi + (-a3 * v2 + a2 * v3) * sinphi;
	newv[2] = a2 * dot + v2 * cosphi + (+a3 * v1 - a1 * v3) * sinphi;
	newv[3] = a3 * dot + v3 * cosphi + (-a2 * v1 + a1 * v2) * sinphi;

	return newv;
}

/* make provided vector a unit vector */
__device__ CUDAREAL unitize(CUDAREAL * vector, CUDAREAL * new_unit_vector) {

	CUDAREAL v1 = vector[1];
	CUDAREAL v2 = vector[2];
	CUDAREAL v3 = vector[3];
	//	CUDAREAL mag = sqrt(v1 * v1 + v2 * v2 + v3 * v3);

	CUDAREAL mag = norm3d(v1, v2, v3);

	if (mag != 0.0) {
		/* normalize it */
		new_unit_vector[0] = mag;
		new_unit_vector[1] = v1 / mag;
		new_unit_vector[2] = v2 / mag;
		new_unit_vector[3] = v3 / mag;
	} else {
		/* can't normalize, report zero vector */
		new_unit_vector[0] = 0.0;
		new_unit_vector[1] = 0.0;
		new_unit_vector[2] = 0.0;
		new_unit_vector[3] = 0.0;
	}
	return mag;
}

/* vector cross product where vector magnitude is 0th element */
__device__ CUDAREAL *cross_product(CUDAREAL * x, CUDAREAL * y, CUDAREAL * z) {
	z[1] = x[2] * y[3] - x[3] * y[2];
	z[2] = x[3] * y[1] - x[1] * y[3];
	z[3] = x[1] * y[2] - x[2] * y[1];
	z[0] = 0.0;

	return z;
}

/* vector inner product where vector magnitude is 0th element */
__device__ CUDAREAL dot_product(const CUDAREAL * x, const CUDAREAL * y) {
	return x[1] * y[1] + x[2] * y[2] + x[3] * y[3];
}

__device__ CUDAREAL dot_product_ldg(const CUDAREAL * __restrict__ x, CUDAREAL * y) {
	return __ldg(&x[1]) * y[1] + __ldg(&x[2]) * y[2] + __ldg(&x[3]) * y[3];
}

/* measure magnitude of provided vector */
__device__ void magnitude(CUDAREAL *vector) {

	/* measure the magnitude */
	vector[0] = sqrt(vector[1] * vector[1] + vector[2] * vector[2] + vector[3] * vector[3]);
}

/* scale magnitude of provided vector */
__device__ CUDAREAL vector_scale(CUDAREAL *vector, CUDAREAL *new_vector, CUDAREAL scale) {

	new_vector[1] = scale * vector[1];
	new_vector[2] = scale * vector[2];
	new_vector[3] = scale * vector[3];
	magnitude(new_vector);

	return new_vector[0];
}

/* rotate a vector using a 9-element unitary matrix */
__device__ void rotate_umat_ldg(CUDAREAL * v, CUDAREAL *newv, const CUDAREAL * __restrict__ umat) {

	/* for convenience, assign matrix x-y coordinate */
	CUDAREAL uxx = __ldg(&umat[0]);
	CUDAREAL uxy = __ldg(&umat[1]);
	CUDAREAL uxz = __ldg(&umat[2]);
	CUDAREAL uyx = __ldg(&umat[3]);
	CUDAREAL uyy = __ldg(&umat[4]);
	CUDAREAL uyz = __ldg(&umat[5]);
	CUDAREAL uzx = __ldg(&umat[6]);
	CUDAREAL uzy = __ldg(&umat[7]);
	CUDAREAL uzz = __ldg(&umat[8]);
	CUDAREAL v1 = v[1];
	CUDAREAL v2 = v[2];
	CUDAREAL v3 = v[3];

	/* rotate the vector (x=1,y=2,z=3) */
	newv[1] = uxx * v1 + uxy * v2 + uxz * v3;
	newv[2] = uyx * v1 + uyy * v2 + uyz * v3;
	newv[3] = uzx * v1 + uzy * v2 + uzz * v3;
}

/* Fourier transform of a grating */
__device__ CUDAREAL sincg(CUDAREAL x, CUDAREAL N) {
	if (x != 0.0)
		return sin(x * N) / sin(x);

	return N;

}

__device__ CUDAREAL sincgrad(CUDAREAL x, CUDAREAL N) {
	if (x != 0.0)
		return sinpi(x * N) / sinpi(x);

	return N;
}

/* Fourier transform of a sphere */
__device__ CUDAREAL sinc3(CUDAREAL x) {
	if (x != 0.0)
		return 3.0 * (sin(x) / x - cos(x)) / (x * x);

	return 1.0;

}

__device__ void polint(const CUDAREAL *xa, const CUDAREAL *ya, CUDAREAL x, CUDAREAL *y) {
	CUDAREAL x0, x1, x2, x3;
	x0 = (x - xa[1]) * (x - xa[2]) * (x - xa[3]) * ya[0] / ((xa[0] - xa[1]) * (xa[0] - xa[2]) * (xa[0] - xa[3]));
	x1 = (x - xa[0]) * (x - xa[2]) * (x - xa[3]) * ya[1] / ((xa[1] - xa[0]) * (xa[1] - xa[2]) * (xa[1] - xa[3]));
	x2 = (x - xa[0]) * (x - xa[1]) * (x - xa[3]) * ya[2] / ((xa[2] - xa[0]) * (xa[2] - xa[1]) * (xa[2] - xa[3]));
	x3 = (x - xa[0]) * (x - xa[1]) * (x - xa[2]) * ya[3] / ((xa[3] - xa[0]) * (xa[3] - xa[1]) * (xa[3] - xa[2]));
	*y = x0 + x1 + x2 + x3;
}

__device__ void polin2(CUDAREAL *x1a, CUDAREAL *x2a, CUDAREAL ya[4][4], CUDAREAL x1, CUDAREAL x2, CUDAREAL *y) {
	int j;
	CUDAREAL ymtmp[4];
	for (j = 1; j <= 4; j++) {
		polint(x2a, ya[j - 1], x2, &ymtmp[j - 1]);
	}
	polint(x1a, ymtmp, x1, y);
}

__device__ void polin3(CUDAREAL *x1a, CUDAREAL *x2a, CUDAREAL *x3a, CUDAREAL ya[4][4][4], CUDAREAL x1, CUDAREAL x2, CUDAREAL x3, CUDAREAL *y) {
	int j;
	CUDAREAL ymtmp[4];

	for (j = 1; j <= 4; j++) {
		polin2(x2a, x3a, &ya[j - 1][0], x2, x3, &ymtmp[j - 1]);
	}
	polint(x1a, ymtmp, x1, y);
}

/* polarization factor */
__device__ CUDAREAL polarization_factor(CUDAREAL kahn_factor, CUDAREAL *incident, CUDAREAL *diffracted, const CUDAREAL * __restrict__ axis) {
	CUDAREAL cos2theta, cos2theta_sqr, sin2theta_sqr;
	CUDAREAL psi = 0.0;
	CUDAREAL E_in[4], B_in[4], E_out[4], B_out[4];

	//  these are already unitized before entering this loop. Optimize this out.
	//	unitize(incident, incident);
	//	unitize(diffracted, diffracted);

	/* component of diffracted unit vector along incident beam unit vector */
	cos2theta = dot_product(incident, diffracted);
	cos2theta_sqr = cos2theta * cos2theta;
	sin2theta_sqr = 1 - cos2theta_sqr;

	if (kahn_factor != 0.0) {
		/* tricky bit here is deciding which direciton the E-vector lies in for each source
		 here we assume it is closest to the "axis" defined above */

		CUDAREAL unitAxis[] = { axis[0], axis[1], axis[2], axis[3] };
		// this is already unitized. Optimize this out.
		unitize(unitAxis, unitAxis);

		/* cross product to get "vertical" axis that is orthogonal to the cannonical "polarization" */
		cross_product(unitAxis, incident, B_in);
		/* make it a unit vector */
		unitize(B_in, B_in);

		/* cross product with incident beam to get E-vector direction */
		cross_product(incident, B_in, E_in);
		/* make it a unit vector */
		unitize(E_in, E_in);

		/* get components of diffracted ray projected onto the E-B plane */
		E_out[0] = dot_product(diffracted, E_in);
		B_out[0] = dot_product(diffracted, B_in);

		/* compute the angle of the diffracted ray projected onto the incident E-B plane */
		psi = -atan2(B_out[0], E_out[0]);
	}

	/* correction for polarized incident beam */
	return 0.5 * (1.0 + cos2theta_sqr - kahn_factor * cos(2 * psi) * sin2theta_sqr);
}

// new exafel_api

// allocation function based on nanoBraggSpotsCUDA
// device pointers are stored in struct to avoid including nanoBragg.h in nanoBragg_cuda.cpp
// Boost incompatibility with newer version of nvcc
// These functions are just copies of sections from nanoBraggSpotsCuda
//   allocate_cuda_cu does the transfers to the GPU
//   add_energy_channel_cuda_cu accumulates contributions on the GPU
//   get_raw_pixels_cuda_cu transfers floatimage back to the CPU
//   deallocate_cuda_cu deallocates the device arrays
// These map to the same function names in the simtbx::nanoBragg::nanoBragg class, but without the "_cu"
// at the end.
extern "C" void allocate_cuda_cu(int deviceId, int spixels, int fpixels, int roi_xmin, int roi_xmax, int roi_ymin, int roi_ymax, int oversample, int point_pixel,
                double pixel_size, double subpixel_size, int steps, double detector_thickstep, int detector_thicksteps, double detector_thick, double detector_mu,
                double sdet_vector[4], double fdet_vector[4], double odet_vector[4], double pix0_vector[4], int curved_detector, double distance, double close_distance,
                double beam_vector[4], double Xbeam, double Ybeam, double dmin, double phi0, double phistep, int phisteps, double spindle_vector[4], int sources,
                double *source_X, double *source_Y, double * source_Z, double * source_I, double * source_lambda, double a0[4], double b0[4], double c0[4],
                shapetype xtal_shape, double mosaic_spread, int mosaic_domains, double * mosaic_umats, double Na, double Nb, double Nc, double V_cell,
                double water_size, double water_F, double water_MW, double r_e_sqr, double fluence, double Avogadro, double spot_scale, int integral_form, double default_F,
                int interpolate, double *** Fhkl, int h_min, int h_max, int h_range, int k_min, int k_max, int k_range, int l_min, int l_max, int l_range, int hkls,
                int nopolar, double polar_vector[4], double polarization, double fudge, int unsigned short * maskimage, float * floatimage /*out*/,
                double * omega_sum/*out*/, int * sumn /*out*/, double * sum /*out*/, double * sumsqr /*out*/, double * max_I/*out*/, double * max_I_x/*out*/,
                                 double * max_I_y /*out*/, cudaPointers &cp /* output for pointers */,
                new_api_cudaPointers &newapi_cp) {

        int total_pixels = spixels * fpixels;
        hipSetDevice(deviceId);

        /*allocate and zero reductions */
        bool * rangemap = (bool*) calloc(total_pixels, sizeof(bool));
        float * omega_reduction = (float*) calloc(total_pixels, sizeof(float));
        float * max_I_x_reduction = (float*) calloc(total_pixels, sizeof(float));
        float * max_I_y_reduction = (float*) calloc(total_pixels, sizeof(float));

        /* clear memory */
        memset(floatimage, 0, sizeof(__typeof__(*floatimage)) * total_pixels);
        int n_panels=1; /* one panel for now, extend later */

        /*create transfer arguments to device space*/
        cp.cu_spixels = spixels;
        cp.cu_fpixels = fpixels;
        newapi_cp.cu_slow_pixels = spixels;
        newapi_cp.cu_fast_pixels = fpixels;
        cp.cu_roi_xmin = roi_xmin;
        cp.cu_roi_xmax = roi_xmax;
        cp.cu_roi_ymin = roi_ymin;
        cp.cu_roi_ymax = roi_ymax;
        cp.cu_oversample = oversample;
        cp.cu_point_pixel = point_pixel;
        cp.cu_pixel_size = pixel_size;
        cp.cu_subpixel_size = subpixel_size;
        cp.cu_steps = steps;
        cp.cu_detector_thickstep = detector_thickstep;
        cp.cu_detector_thick = detector_thick;
        cp.cu_detector_mu = detector_mu;
        cp.cu_detector_thicksteps = detector_thicksteps;
        cp.cu_curved_detector = curved_detector;

        cp.cu_distance = distance;
        cp.cu_close_distance = close_distance;

        cp.cu_Xbeam = Xbeam;
        cp.cu_Ybeam = Ybeam;
        cp.cu_dmin = dmin;
        cp.cu_phi0 = phi0;
        cp.cu_phistep = phistep;
        cp.cu_phisteps = phisteps;

        cp.cu_xtal_shape = xtal_shape;

        cp.cu_sources = sources;

        cp.cu_mosaic_spread = mosaic_spread;
        cp.cu_mosaic_domains = mosaic_domains;

        cp.cu_Na = Na;
        cp.cu_Nb = Nb;
        cp.cu_Nc = Nc;
        cp.cu_V_cell = V_cell;
        cp.cu_water_size = water_size;
        cp.cu_water_F = water_F;
        cp.cu_water_MW = water_MW;
        cp.cu_r_e_sqr = r_e_sqr;
        cp.cu_fluence = fluence;
        cp.cu_Avogadro = Avogadro;
        cp.cu_spot_scale = spot_scale;

        cp.cu_integral_form = integral_form;
        cp.cu_default_F = default_F;
        cp.cu_interpolate = interpolate;

//      int cu_h_min = h_min, cu_h_max = h_max, cu_h_range = h_range;
//      int cu_k_min = k_min, cu_k_max = k_max, cu_k_range = k_range;
//      int cu_l_min = l_min, cu_l_max = l_max, cu_l_range = l_range;
//      int cu_hkls = hkls;

        cp.cu_nopolar = nopolar;
        cp.cu_polarization = polarization;
        cp.cu_fudge = fudge;

        hklParams FhklParams = { hkls, h_min, h_max, h_range, k_min, k_max, k_range, l_min, l_max, l_range };
        hklParams * cu_FhklParams;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_FhklParams, sizeof(*cu_FhklParams)));
        CUDA_CHECK_RETURN(hipMemcpy(cu_FhklParams, &FhklParams, sizeof(*cu_FhklParams), hipMemcpyHostToDevice));
        cp.cu_FhklParams = cu_FhklParams;

        const int vector_length = 4;
        CUDAREAL * cu_sdet_vector;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_sdet_vector, sizeof(*cu_sdet_vector) * vector_length));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_sdet_vector, sdet_vector, vector_length));
        cp.cu_sdet_vector = cu_sdet_vector;

        CUDAREAL * cu_fdet_vector;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_fdet_vector, sizeof(*cu_fdet_vector) * vector_length));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_fdet_vector, fdet_vector, vector_length));
        cp.cu_fdet_vector = cu_fdet_vector;

        CUDAREAL * cu_odet_vector;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_odet_vector, sizeof(*cu_odet_vector) * vector_length));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_odet_vector, odet_vector, vector_length));
        cp.cu_odet_vector = cu_odet_vector;

        CUDAREAL * cu_pix0_vector;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_pix0_vector, sizeof(*cu_pix0_vector) * vector_length));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_pix0_vector, pix0_vector, vector_length));
        cp.cu_pix0_vector = cu_pix0_vector;

        CUDAREAL * cu_beam_vector;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_beam_vector, sizeof(*cu_beam_vector) * vector_length));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_beam_vector, beam_vector, vector_length));
        cp.cu_beam_vector = cu_beam_vector;

        CUDAREAL * cu_spindle_vector;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_spindle_vector, sizeof(*cu_spindle_vector) * vector_length));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_spindle_vector, spindle_vector, vector_length));
        cp.cu_spindle_vector = cu_spindle_vector;

        CUDAREAL * cu_a0;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_a0, sizeof(*cu_a0) * vector_length));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_a0, a0, vector_length));
        cp.cu_a0 = cu_a0;

        CUDAREAL * cu_b0;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_b0, sizeof(*cu_b0) * vector_length));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_b0, b0, vector_length));
        cp.cu_b0 = cu_b0;

        CUDAREAL * cu_c0;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_c0, sizeof(*cu_c0) * vector_length));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_c0, c0, vector_length));
        cp.cu_c0 = cu_c0;

        //      Unitize polar vector before sending it to the GPU. Optimization do it only once here rather than multiple time per pixel in the GPU.
        CUDAREAL * cu_polar_vector;
        double polar_vector_unitized[4];
        cpu_unitize(polar_vector, polar_vector_unitized);
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_polar_vector, sizeof(*cu_polar_vector) * vector_length));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_polar_vector, polar_vector_unitized, vector_length));
        cp.cu_polar_vector = cu_polar_vector;

        CUDAREAL * cu_source_X = NULL;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_source_X, sizeof(*cu_source_X) * sources));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_source_X, source_X, sources));
        cp.cu_source_X = cu_source_X;

        CUDAREAL * cu_source_Y = NULL;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_source_Y, sizeof(*cu_source_Y) * sources));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_source_Y, source_Y, sources));
        cp.cu_source_Y = cu_source_Y;

        CUDAREAL * cu_source_Z = NULL;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_source_Z, sizeof(*cu_source_Z) * sources));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_source_Z, source_Z, sources));
        cp.cu_source_Z = cu_source_Z;

        CUDAREAL * cu_source_I = NULL;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_source_I, sizeof(*cu_source_I) * sources));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_source_I, source_I, sources));
        cp.cu_source_I = cu_source_I;

        CUDAREAL * cu_source_lambda = NULL;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_source_lambda, sizeof(*cu_source_lambda) * sources));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_source_lambda, source_lambda, sources));
        cp.cu_source_lambda = cu_source_lambda;

        CUDAREAL * cu_mosaic_umats = NULL;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_mosaic_umats, sizeof(*cu_mosaic_umats) * mosaic_domains * 9));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_mosaic_umats, mosaic_umats, mosaic_domains * 9));
        cp.cu_mosaic_umats = cu_mosaic_umats;

        float * cu_floatimage = NULL;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_floatimage, sizeof(*cu_floatimage) * total_pixels));
        // In contrast to old API, new API initializes its own accumulator, does not take values from CPU
        // CUDA_CHECK_RETURN(hipMemcpy(cu_floatimage, floatimage, sizeof(*cu_floatimage) * total_pixels, hipMemcpyHostToDevice));
        cp.cu_floatimage = cu_floatimage;

        /* separate accumulator image outside the usual nanoBragg data structure.
           1. accumulate contributions from a sequence of source energy channels computed separately
           2. represent multiple panels, all same rectangular shape; slowest dimension = n_panels */
        double * cu_accumulate_floatimage = NULL;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_accumulate_floatimage, sizeof(*cu_accumulate_floatimage) * total_pixels * n_panels));
        CUDA_CHECK_RETURN(hipMemset((void *)cu_accumulate_floatimage, 0, sizeof(*cu_accumulate_floatimage) * total_pixels * n_panels));
        newapi_cp.cu_accumulate_floatimage = cu_accumulate_floatimage;

        float * cu_omega_reduction = NULL;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_omega_reduction, sizeof(*cu_omega_reduction) * total_pixels));
        CUDA_CHECK_RETURN(hipMemcpy(cu_omega_reduction, omega_reduction, sizeof(*cu_omega_reduction) * total_pixels, hipMemcpyHostToDevice));
        cp.cu_omega_reduction = cu_omega_reduction;

        float * cu_max_I_x_reduction = NULL;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_max_I_x_reduction, sizeof(*cu_max_I_x_reduction) * total_pixels));
        CUDA_CHECK_RETURN(hipMemcpy(cu_max_I_x_reduction, max_I_x_reduction, sizeof(*cu_max_I_x_reduction) * total_pixels, hipMemcpyHostToDevice));
        cp.cu_max_I_x_reduction = cu_max_I_x_reduction;

        float * cu_max_I_y_reduction = NULL;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_max_I_y_reduction, sizeof(*cu_max_I_y_reduction) * total_pixels));
        CUDA_CHECK_RETURN(hipMemcpy(cu_max_I_y_reduction, max_I_y_reduction, sizeof(*cu_max_I_y_reduction) * total_pixels, hipMemcpyHostToDevice));
        cp.cu_max_I_y_reduction = cu_max_I_y_reduction;

        bool * cu_rangemap = NULL;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_rangemap, sizeof(*cu_rangemap) * total_pixels));
        CUDA_CHECK_RETURN(hipMemcpy(cu_rangemap, rangemap, sizeof(*cu_rangemap) * total_pixels, hipMemcpyHostToDevice));
        cp.cu_rangemap = cu_rangemap;

        int unsigned short * cu_maskimage = NULL;
        if (maskimage != NULL) {
                CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_maskimage, sizeof(*cu_maskimage) * total_pixels));
                CUDA_CHECK_RETURN(hipMemcpy(cu_maskimage, maskimage, sizeof(*cu_maskimage) * total_pixels, hipMemcpyHostToDevice));
        }
        cp.cu_maskimage = cu_maskimage;

        int hklsize = h_range * k_range * l_range;
        CUDAREAL * FhklLinear = (CUDAREAL*) calloc(hklsize, sizeof(*FhklLinear));
        for (int h = 0; h < h_range; h++) {
          for (int k = 0; k < k_range; k++) {
            for (int l = 0; l < l_range; l++) {
              //      convert Fhkl double to CUDAREAL
              FhklLinear[h * k_range * l_range + k * l_range + l] = Fhkl[h][k][l];
            }
          }
        }

        CUDAREAL * cu_Fhkl = NULL;
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_Fhkl, sizeof(*cu_Fhkl) * hklsize));
        CUDA_CHECK_RETURN(hipMemcpy(cu_Fhkl, FhklLinear, sizeof(*cu_Fhkl) * hklsize, hipMemcpyHostToDevice));
        cp.cu_Fhkl = cu_Fhkl;
        free(FhklLinear);

        // deallocate host arrays
        // potential memory leaks
        free(rangemap);
        free(omega_reduction);
        free(max_I_x_reduction);
        free(max_I_y_reduction);
}

extern "C"
void add_energy_channel_from_gpu_amplitudes_cuda_cu(int deviceId, double * source_I, double * source_lambda,
                                                    double const& fluence, int const& ichannel,
                                                    simtbx::gpu::gpu_energy_channels &gec,
                                                    cudaPointers &cp, new_api_cudaPointers &newapi_cp, int verbose){
        

struct timeval t1, t2;	
gettimeofday(&t1, 0);
        cp.cu_fluence = fluence; // new for this energy channel
        hipSetDevice(deviceId);
gettimeofday(&t2, 0);
double time = (1000000.0*(t2.tv_sec-t1.tv_sec) + t2.tv_usec-t1.tv_usec)/1000.0;
printf("Time set device:  %3.10f ms \n", time);
	
        // transfer source_I, source_lambda, and Fhkl
        // the int arguments are for sizes of the arrays
gettimeofday(&t1, 0);
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cp.cu_source_I, source_I, cp.cu_sources));
gettimeofday(&t2, 0);
time = (1000000.0*(t2.tv_sec-t1.tv_sec) + t2.tv_usec-t1.tv_usec)/1000.0;
printf("Time memCopyVec source I :  %3.10f ms \n", time);
        
gettimeofday(&t1, 0);
	CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cp.cu_source_lambda, source_lambda, cp.cu_sources));
	//CUDAREAL * temp = new CUDAREAL[vector_items];
	//for (size_t i = 0; i < vector_items; i++) {
	//	temp[i] = src[i];
	//}
	//hipError_t ret = hipMemcpy(dst, temp, sizeof(*dst) * vector_items, hipMemcpyHostToDevice);
	//delete temp;
gettimeofday(&t2, 0);
time = (1000000.0*(t2.tv_sec-t1.tv_sec) + t2.tv_usec-t1.tv_usec)/1000.0;
printf("Time memCopyVec source lambda :  %3.10f ms \n", time);
        int hklsize = gec.h_range * gec.k_range * gec.l_range;

        hklParams FhklParams = { hklsize, gec.h_min, gec.h_max, gec.h_range,
                gec.k_min, gec.k_max, gec.k_range, gec.l_min, gec.l_max, gec.l_range };
        hklParams * cu_FhklParams;
gettimeofday(&t1, 0);
        CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_FhklParams, sizeof(*cu_FhklParams)));
gettimeofday(&t2, 0);
time = (1000000.0*(t2.tv_sec-t1.tv_sec) + t2.tv_usec-t1.tv_usec)/1000.0;
printf("Allocate FhklParams :  %3.10f ms \n", time);

gettimeofday(&t1, 0);
        CUDA_CHECK_RETURN(hipMemcpy(cu_FhklParams, &FhklParams, sizeof(*cu_FhklParams), hipMemcpyHostToDevice));
gettimeofday(&t2, 0);
time = (1000000.0*(t2.tv_sec-t1.tv_sec) + t2.tv_usec-t1.tv_usec)/1000.0;
printf("Copy FhklParams :  %3.10f ms \n", time);
        cp.cu_FhklParams = cu_FhklParams;

        // first time through make sure to deallocate cu_Fhkl already used
        if (cp.cu_Fhkl != NULL) {CUDA_CHECK_RETURN(hipFree(cp.cu_Fhkl));}
        // magic happens here: take pointer from singleton, temporarily use it for add Bragg iteration:
gettimeofday(&t1, 0);
        cp.cu_Fhkl = gec.d_channel_Fhkl[ichannel];
gettimeofday(&t2, 0);
time = (1000000.0*(t2.tv_sec-t1.tv_sec) + t2.tv_usec-t1.tv_usec)/1000.0;
printf("channel_Fhkl :  %3.10f ms \n", time);

        hipDeviceProp_t deviceProps = { 0 };
gettimeofday(&t1, 0);
        CUDA_CHECK_RETURN(hipGetDeviceProperties(&deviceProps, deviceId));
gettimeofday(&t2, 0);
time = (1000000.0*(t2.tv_sec-t1.tv_sec) + t2.tv_usec-t1.tv_usec)/1000.0;
printf("det device properties :  %3.10f ms \n", time);

gettimeofday(&t1, 0);
        int smCount = deviceProps.multiProcessorCount;
        dim3 threadsPerBlock(THREADS_PER_BLOCK_X, THREADS_PER_BLOCK_Y);
        dim3 numBlocks(smCount * 8, 1);
gettimeofday(&t2, 0);
time = (1000000.0*(t2.tv_sec-t1.tv_sec) + t2.tv_usec-t1.tv_usec)/1000.0;
printf("dimz:  %3.10f ms \n", time);
	
gettimeofday(&t1, 0);
        hipLaunchKernelGGL(nanoBraggSpotsCUDAKernel, dim3(numBlocks), dim3(threadsPerBlock), 0, 0, cp.cu_spixels, cp.cu_fpixels, cp.cu_roi_xmin,
          cp.cu_roi_xmax, cp.cu_roi_ymin, cp.cu_roi_ymax, cp.cu_oversample, cp.cu_point_pixel,
          cp.cu_pixel_size, cp.cu_subpixel_size, cp.cu_steps, cp.cu_detector_thickstep, cp.cu_detector_thicksteps,
          cp.cu_detector_thick, cp.cu_detector_mu, cp.cu_sdet_vector, cp.cu_fdet_vector, cp.cu_odet_vector,
          cp.cu_pix0_vector, cp.cu_curved_detector, cp.cu_distance, cp.cu_close_distance, cp.cu_beam_vector,
          cp.cu_Xbeam, cp.cu_Ybeam, cp.cu_dmin, cp.cu_phi0, cp.cu_phistep, cp.cu_phisteps, cp.cu_spindle_vector,
          cp.cu_sources, cp.cu_source_X, cp.cu_source_Y, cp.cu_source_Z,
          cp.cu_source_I, cp.cu_source_lambda, cp.cu_a0, cp.cu_b0,
          cp.cu_c0, cp.cu_xtal_shape, cp.cu_mosaic_spread, cp.cu_mosaic_domains, cp.cu_mosaic_umats,
          cp.cu_Na, cp.cu_Nb, cp.cu_Nc, cp.cu_V_cell,
          cp.cu_water_size, cp.cu_water_F, cp.cu_water_MW, cp.cu_r_e_sqr, cp.cu_fluence,
          cp.cu_Avogadro, cp.cu_spot_scale, cp.cu_integral_form, cp.cu_default_F,
          cp.cu_interpolate, cp.cu_Fhkl, cp.cu_FhklParams, cp.cu_nopolar,
          cp.cu_polar_vector, cp.cu_polarization, cp.cu_fudge,
          cp.cu_maskimage, cp.cu_floatimage /*out*/, cp.cu_omega_reduction/*out*/,
          cp.cu_max_I_x_reduction/*out*/, cp.cu_max_I_y_reduction /*out*/, cp.cu_rangemap /*out*/);
CUDA_CHECK_RETURN(hipDeviceSynchronize());

gettimeofday(&t2, 0);
time = (1000000.0*(t2.tv_sec-t1.tv_sec) + t2.tv_usec-t1.tv_usec)/1000.0;
printf("Time run kernel:  %3.10f ms \n", time);

gettimeofday(&t1, 0);
        //dont want to free the gec data when the nanoBragg goes out of scope, so switch the pointer
        cp.cu_Fhkl = NULL;

        CUDA_CHECK_RETURN(hipPeekAtLastError());
        CUDA_CHECK_RETURN(hipDeviceSynchronize());
gettimeofday(&t2, 0);
time = (1000000.0*(t2.tv_sec-t1.tv_sec) + t2.tv_usec-t1.tv_usec)/1000.0;
printf("Time to peak:  %3.10f ms \n", time);

//gettimeofday(&t1, 0);
//        CUDA_CHECK_RETURN(hipDeviceSynchronize());
//
//gettimeofday(&t2, 0);
//time = (1000000.0*(t2.tv_sec-t1.tv_sec) + t2.tv_usec-t1.tv_usec)/1000.0;
//printf("Time to synchronize:  %3.10f ms \n", time);

gettimeofday(&t1, 0);
        //dont want to free the gec data when the nanoBragg goes out of scope, so switch the pointer
        hipLaunchKernelGGL(add_array_CUDAKernel, dim3(numBlocks), dim3(threadsPerBlock), 0, 0, newapi_cp.cu_accumulate_floatimage, cp.cu_floatimage,
          cp.cu_spixels * cp.cu_fpixels);
CUDA_CHECK_RETURN(hipDeviceSynchronize());

gettimeofday(&t2, 0);
time = (1000000.0*(t2.tv_sec-t1.tv_sec) + t2.tv_usec-t1.tv_usec)/1000.0;
printf("Time run add kernel:  %3.10f ms \n", time);
}

extern "C" void add_energy_channel_cuda_cu(int deviceId, double * source_I, double * source_lambda, double const& fluence,
                                           double *** Fhkl, int h_min, int k_min, int l_min, int h_range, int k_range, int l_range,
                                           cudaPointers &cp, new_api_cudaPointers &newapi_cp) {

        cp.cu_fluence = fluence; // new for this energy channel
        hipSetDevice(deviceId);
        // transfer source_I, source_lambda, and Fhkl
        // the int arguments are for sizes of the arrays
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cp.cu_source_I, source_I, cp.cu_sources));
        CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cp.cu_source_lambda, source_lambda, cp.cu_sources));

        int hklsize = h_range * k_range * l_range;
        CUDAREAL * FhklLinear = (CUDAREAL*) calloc(hklsize, sizeof(*FhklLinear));
        for (int h = 0; h < h_range; h++) {
          for (int k = 0; k < k_range; k++) {
            for (int l = 0; l < l_range; l++) {
              //        convert Fhkl double to CUDAREAL
              FhklLinear[h * k_range * l_range + k * l_range + l] = Fhkl[h][k][l];
            }
          }
        }
        CUDA_CHECK_RETURN(hipMemcpy(cp.cu_Fhkl, FhklLinear, sizeof(*cp.cu_Fhkl) * hklsize, hipMemcpyHostToDevice));
        free(FhklLinear);

        CUDA_CHECK_RETURN(hipGetDevice(&deviceId));
        hipDeviceProp_t deviceProps = { 0 };
        CUDA_CHECK_RETURN(hipGetDeviceProperties(&deviceProps, deviceId));
        int smCount = deviceProps.multiProcessorCount;

//      CUDA_CHECK_RETURN(hipFuncSetCacheConfig(reinterpret_cast<const void*>(nanoBraggSpotsCUDAKernel), hipFuncCachePreferShared));
//      CUDA_CHECK_RETURN(hipFuncSetCacheConfig(reinterpret_cast<const void*>(nanoBraggSpotsCUDAKernel), hipFuncCachePreferL1));

        dim3 threadsPerBlock(THREADS_PER_BLOCK_X, THREADS_PER_BLOCK_Y);
        //  dim3 numBlocks((spixels - 1) / threadsPerBlock.x + 1, (fpixels - 1) / threadsPerBlock.y + 1);
        dim3 numBlocks(smCount * 8, 1);

        //  initialize the device memory within a kernel.
        //      hipLaunchKernelGGL(nanoBraggSpotsInitCUDAKernel, dim3(numBlocks), dim3(threadsPerBlock), 0, 0, cu_spixels, cu_fpixels, cu_floatimage, cu_omega_reduction, cu_max_I_x_reduction, cu_max_I_y_reduction, cu_rangemap);
        //  CUDA_CHECK_RETURN(hipPeekAtLastError());
        //  CUDA_CHECK_RETURN(hipDeviceSynchronize());

        hipLaunchKernelGGL(nanoBraggSpotsCUDAKernel, dim3(numBlocks), dim3(threadsPerBlock), 0, 0, cp.cu_spixels, cp.cu_fpixels, cp.cu_roi_xmin, cp.cu_roi_xmax, cp.cu_roi_ymin, cp.cu_roi_ymax, cp.cu_oversample,
                        cp.cu_point_pixel,
                        cp.cu_pixel_size, cp.cu_subpixel_size, cp.cu_steps, cp.cu_detector_thickstep, cp.cu_detector_thicksteps, cp.cu_detector_thick, cp.cu_detector_mu,
                        cp.cu_sdet_vector, cp.cu_fdet_vector, cp.cu_odet_vector,
                        cp.cu_pix0_vector, cp.cu_curved_detector, cp.cu_distance, cp.cu_close_distance, cp.cu_beam_vector,
                        cp.cu_Xbeam, cp.cu_Ybeam, cp.cu_dmin, cp.cu_phi0, cp.cu_phistep, cp.cu_phisteps, cp.cu_spindle_vector, cp.cu_sources,
                        cp.cu_source_X, cp.cu_source_Y, cp.cu_source_Z,
                        cp.cu_source_I, cp.cu_source_lambda, cp.cu_a0, cp.cu_b0,
                        cp.cu_c0, cp.cu_xtal_shape, cp.cu_mosaic_spread, cp.cu_mosaic_domains, cp.cu_mosaic_umats,
                        cp.cu_Na, cp.cu_Nb, cp.cu_Nc, cp.cu_V_cell,
                        cp.cu_water_size, cp.cu_water_F, cp.cu_water_MW, cp.cu_r_e_sqr, cp.cu_fluence, cp.cu_Avogadro, cp.cu_spot_scale, cp.cu_integral_form, cp.cu_default_F,
                        cp.cu_interpolate, cp.cu_Fhkl, cp.cu_FhklParams, cp.cu_nopolar, cp.cu_polar_vector, cp.cu_polarization, cp.cu_fudge,
                        cp.cu_maskimage, cp.cu_floatimage /*out*/, cp.cu_omega_reduction/*out*/, cp.cu_max_I_x_reduction/*out*/, cp.cu_max_I_y_reduction /*out*/,
                        cp.cu_rangemap /*out*/);

        CUDA_CHECK_RETURN(hipPeekAtLastError());
        //CUDA_CHECK_RETURN(hipDeviceSynchronize());

        hipLaunchKernelGGL(add_array_CUDAKernel, dim3(numBlocks), dim3(threadsPerBlock), 0, 0, newapi_cp.cu_accumulate_floatimage, cp.cu_floatimage,
          cp.cu_spixels * cp.cu_fpixels);
}

extern "C"
void scale_in_place_cuda_cu(int deviceId, double const& scale_factor, new_api_cudaPointers &newapi_cp){
  hipSetDevice(deviceId);
  hipDeviceProp_t deviceProps = { 0 };
  CUDA_CHECK_RETURN(hipGetDeviceProperties(&deviceProps, deviceId));
  int smCount = deviceProps.multiProcessorCount;
  dim3 threadsPerBlock(THREADS_PER_BLOCK_X, THREADS_PER_BLOCK_Y);
  dim3 numBlocks(smCount * 8, 1);
  int total_pixels = newapi_cp.cu_slow_pixels * newapi_cp.cu_fast_pixels;
  hipLaunchKernelGGL(scale_array_CUDAKernel, dim3(numBlocks), dim3(threadsPerBlock), 0, 0, 
          scale_factor, newapi_cp.cu_accumulate_floatimage, total_pixels);
}

__global__ void add_background_CUDAKernel(int sources, int nanoBragg_oversample,
    CUDAREAL pixel_size, int spixels, int fpixels, int detector_thicksteps, CUDAREAL detector_thickstep, CUDAREAL detector_attnlen,
    const CUDAREAL * __restrict__ sdet_vector, const CUDAREAL * __restrict__ fdet_vector,
    const CUDAREAL * __restrict__ odet_vector, const CUDAREAL * __restrict__ pix0_vector,
    CUDAREAL close_distance, int point_pixel, CUDAREAL detector_thick,
    const CUDAREAL * __restrict__ source_X, const CUDAREAL * __restrict__ source_Y, const CUDAREAL * __restrict__ source_Z,
    const CUDAREAL * __restrict__ source_lambda, const CUDAREAL * __restrict__ source_I,
    int stols, const CUDAREAL * stol_of, const CUDAREAL * Fbg_of,
    int nopolar, CUDAREAL polarization, const CUDAREAL * __restrict__ polar_vector,
    CUDAREAL r_e_sqr, CUDAREAL fluence, CUDAREAL amorphous_molecules,
    float * floatimage){
    int oversample=-1, override_source=-1; //override features that usually slow things down,
                                           //like oversampling pixels & multiple sources
    int source_start = 0;
    /* allow user to override automated oversampling decision at call time with arguments */
    if(oversample<=0) oversample = nanoBragg_oversample;
    if(oversample<=0) oversample = 1;
    if(override_source>=0) {
        /* user-specified source in the argument */
        source_start = override_source;
        sources = source_start +1;
    }
    /* make sure we are normalizing with the right number of sub-steps */
    int steps = oversample*oversample;
    CUDAREAL subpixel_size = pixel_size/oversample;

    /* sweep over detector */
    const int total_pixels = spixels * fpixels;
    const int fstride = gridDim.x * blockDim.x;
    const int sstride = gridDim.y * blockDim.y;
    const int stride = fstride * sstride;
    for (int pixIdx = (blockDim.y * blockIdx.y + threadIdx.y) * fstride + blockDim.x * blockIdx.x + threadIdx.x;
         pixIdx < total_pixels; pixIdx += stride) {
      const int fpixel = pixIdx % fpixels;
      const int spixel = pixIdx / fpixels;
      /* position in pixel array */
      const int j = pixIdx;
      /* reset background photon count for this pixel */
      CUDAREAL Ibg = 0;
      int nearest = 0; // sort-stable alogorithm, instead of holding value over from previous pixel
            /* loop over sub-pixels */
            for(int subS=0;subS<oversample;++subS){
                for(int subF=0;subF<oversample;++subF){
                    /* absolute mm position on detector (relative to its origin) */
                    CUDAREAL Fdet = subpixel_size*(fpixel*oversample + subF ) + subpixel_size/2.0;
                    CUDAREAL Sdet = subpixel_size*(spixel*oversample + subS ) + subpixel_size/2.0;

                    for(int thick_tic=0;thick_tic<detector_thicksteps;++thick_tic){
                        /* assume "distance" is to the front of the detector sensor layer */
                        CUDAREAL Odet = thick_tic*detector_thickstep;
                        CUDAREAL pixel_pos[4];

                        pixel_pos[1] = Fdet * __ldg(&fdet_vector[1]) + Sdet * __ldg(&sdet_vector[1]) + Odet * __ldg(&odet_vector[1]) + __ldg(&pix0_vector[1]); // X
                        pixel_pos[2] = Fdet * __ldg(&fdet_vector[2]) + Sdet * __ldg(&sdet_vector[2]) + Odet * __ldg(&odet_vector[2]) + __ldg(&pix0_vector[2]); // X
                        pixel_pos[3] = Fdet * __ldg(&fdet_vector[3]) + Sdet * __ldg(&sdet_vector[3]) + Odet * __ldg(&odet_vector[3]) + __ldg(&pix0_vector[3]); // X
                        pixel_pos[0] = 0.0;
                        /* no curved detector option (future implementation) */
                        /* construct the diffracted-beam unit vector to this pixel */
                        CUDAREAL diffracted[4];
                        CUDAREAL airpath = unitize(pixel_pos,diffracted);

                        /* solid angle subtended by a pixel: (pix/airpath)^2*cos(2theta) */
                        CUDAREAL omega_pixel = pixel_size*pixel_size/airpath/airpath*close_distance/airpath;
                        /* option to turn off obliquity effect, inverse-square-law only */
                        if(point_pixel) omega_pixel = 1.0/airpath/airpath;

                        /* now calculate detector thickness effects */
                        CUDAREAL capture_fraction = 1.0;
                        if(detector_thick > 0.0){
                            /* inverse of effective thickness increase */
                            CUDAREAL parallax = dot_product(diffracted,odet_vector);
                            capture_fraction = exp(-thick_tic*detector_thickstep/detector_attnlen/parallax)
                                              -exp(-(thick_tic+1)*detector_thickstep/detector_attnlen/parallax);
                        }

                        /* loop over sources now */
                        for(int source=source_start;source<sources;++source){

                            /* retrieve stuff from cache */
                            CUDAREAL incident[4];
                            incident[1] = -__ldg(&source_X[source]);
                            incident[2] = -__ldg(&source_Y[source]);
                            incident[3] = -__ldg(&source_Z[source]);
                            CUDAREAL lambda = __ldg(&source_lambda[source]);
                            CUDAREAL source_fraction = __ldg(&source_I[source]);
                            /* construct the incident beam unit vector while recovering source distance */
                            unitize(incident,incident);

                            /* construct the scattering vector for this pixel */
                            CUDAREAL scattering[4];
                            scattering[1] = (diffracted[1]-incident[1])/lambda;
                            scattering[2] = (diffracted[2]-incident[2])/lambda;
                            scattering[3] = (diffracted[3]-incident[3])/lambda;
                            magnitude(scattering);
                            /* sin(theta)/lambda is half the scattering vector length */
                            CUDAREAL stol = 0.5*scattering[0];

                            /* now we need to find the nearest four "stol file" points */
                            while(stol > stol_of[nearest] && nearest <= stols){++nearest; };
                            while(stol < stol_of[nearest] && nearest >= 2){--nearest; };

                            /* cubic spline interpolation */
                            CUDAREAL Fbg;
                            polint(stol_of+nearest-1, Fbg_of+nearest-1, stol, &Fbg);

                            /* allow negative F values to yield negative intensities */
                            CUDAREAL sign=1.0;
                            if(Fbg<0.0) sign=-1.0;

                            /* now we have the structure factor for this pixel */

                            /* polarization factor */
                            CUDAREAL polar = 1.0;
                            if(! nopolar){
                                /* need to compute polarization factor */
                                polar = polarization_factor(polarization,incident,diffracted,polar_vector);
                            }

                            /* accumulate unscaled pixel intensity from this */
                            Ibg += sign*Fbg*Fbg*polar*omega_pixel*source_fraction*capture_fraction;
                        } /* end of source loop */
                    } /* end of detector thickness loop */
                } /* end of sub-pixel y loop */
            } /* end of sub-pixel x loop */
            /* save photons/pixel (if fluence specified), or F^2/omega if no fluence given */
            floatimage[j] += Ibg*r_e_sqr*fluence*amorphous_molecules/steps;    } // end of pixIdx loop
}

extern "C"
void add_background_cuda_cu(int deviceId, int stols, double* stol_of, double* Fbg_of, double fluence,
  double * source_I, double * source_lambda,
  double amorphous_molecules, cudaPointers &cp, new_api_cudaPointers &newapi_cp){

  // transfer source_I, source_lambda
  CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cp.cu_source_I, source_I, cp.cu_sources));
  CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cp.cu_source_lambda, source_lambda, cp.cu_sources));

  CUDAREAL * cu_stol_of;
  CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_stol_of, sizeof(*cu_stol_of) * stols));
  CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_stol_of, stol_of, stols));

  CUDAREAL * cu_Fbg_of;
  CUDA_CHECK_RETURN(hipMalloc((void ** )&cu_Fbg_of, sizeof(*cu_Fbg_of) * stols));
  CUDA_CHECK_RETURN(cudaMemcpyVectorDoubleToDevice(cu_Fbg_of, Fbg_of, stols));

  hipDeviceProp_t deviceProps = { 0 };
  CUDA_CHECK_RETURN(hipGetDeviceProperties(&deviceProps, deviceId));
  int smCount = deviceProps.multiProcessorCount;
  dim3 threadsPerBlock(THREADS_PER_BLOCK_X, THREADS_PER_BLOCK_Y);
  dim3 numBlocks(smCount * 8, 1);

  //  initialize the device memory within a kernel. //have not analyzed to see if initializaiton is needed
  hipLaunchKernelGGL(nanoBraggSpotsInitCUDAKernel, dim3(numBlocks), dim3(threadsPerBlock), 0, 0, cp.cu_spixels, cp.cu_fpixels,
          cp.cu_floatimage, cp.cu_omega_reduction, cp.cu_max_I_x_reduction, cp.cu_max_I_y_reduction,
          cp.cu_rangemap);
  CUDA_CHECK_RETURN(hipPeekAtLastError());
  //CUDA_CHECK_RETURN(hipDeviceSynchronize());

  hipLaunchKernelGGL(add_background_CUDAKernel, dim3(numBlocks), dim3(threadsPerBlock), 0, 0, cp.cu_sources, cp.cu_oversample,
    cp.cu_pixel_size, cp.cu_spixels, cp.cu_fpixels, cp.cu_detector_thicksteps,
    cp.cu_detector_thickstep, cp.cu_detector_mu,
    cp.cu_sdet_vector, cp.cu_fdet_vector, cp.cu_odet_vector, cp.cu_pix0_vector,
    cp.cu_close_distance, cp.cu_point_pixel, cp.cu_detector_thick,
    cp.cu_source_X, cp.cu_source_Y, cp.cu_source_Z,
    cp.cu_source_lambda, cp.cu_source_I,
    stols, cu_stol_of, cu_Fbg_of,
    cp.cu_nopolar, cp.cu_polarization, cp.cu_polar_vector,
    cp.cu_r_e_sqr, fluence, amorphous_molecules,
    cp.cu_floatimage /*out*/);

  CUDA_CHECK_RETURN(hipPeekAtLastError());
  //CUDA_CHECK_RETURN(hipDeviceSynchronize());
  hipLaunchKernelGGL(add_array_CUDAKernel, dim3(numBlocks), dim3(threadsPerBlock), 0, 0, newapi_cp.cu_accumulate_floatimage, cp.cu_floatimage,
          cp.cu_spixels * cp.cu_fpixels);

  CUDA_CHECK_RETURN(hipFree(cu_stol_of));
  CUDA_CHECK_RETURN(hipFree(cu_Fbg_of));
}

extern "C" void get_raw_pixels_cuda_cu(int deviceId, double * floatimage, new_api_cudaPointers &newapi_cp) {
  int total_pixels = newapi_cp.cu_slow_pixels * newapi_cp.cu_fast_pixels;
  hipSetDevice(deviceId);
  CUDA_CHECK_RETURN(
  hipMemcpy(floatimage, newapi_cp.cu_accumulate_floatimage, sizeof(*newapi_cp.cu_accumulate_floatimage) * total_pixels, hipMemcpyDeviceToHost));
}

extern "C" void deallocate_cuda_cu(int deviceId, cudaPointers &cp, new_api_cudaPointers &newapi_cp) {
        hipSetDevice(deviceId);
        CUDA_CHECK_RETURN(hipFree(cp.cu_sdet_vector));
        CUDA_CHECK_RETURN(hipFree(cp.cu_fdet_vector));
        CUDA_CHECK_RETURN(hipFree(cp.cu_odet_vector));
        CUDA_CHECK_RETURN(hipFree(cp.cu_pix0_vector));
        CUDA_CHECK_RETURN(hipFree(cp.cu_beam_vector));
        CUDA_CHECK_RETURN(hipFree(cp.cu_spindle_vector));
        CUDA_CHECK_RETURN(hipFree(cp.cu_source_X));
        CUDA_CHECK_RETURN(hipFree(cp.cu_source_Y));
        CUDA_CHECK_RETURN(hipFree(cp.cu_source_Z));
        CUDA_CHECK_RETURN(hipFree(cp.cu_source_I));
        CUDA_CHECK_RETURN(hipFree(cp.cu_source_lambda));
        CUDA_CHECK_RETURN(hipFree(cp.cu_a0));
        CUDA_CHECK_RETURN(hipFree(cp.cu_b0));
        CUDA_CHECK_RETURN(hipFree(cp.cu_c0));
        CUDA_CHECK_RETURN(hipFree(cp.cu_mosaic_umats));
        CUDA_CHECK_RETURN(hipFree(cp.cu_Fhkl));
        CUDA_CHECK_RETURN(hipFree(cp.cu_FhklParams));
        CUDA_CHECK_RETURN(hipFree(cp.cu_polar_vector));
        CUDA_CHECK_RETURN(hipFree(cp.cu_maskimage));
        CUDA_CHECK_RETURN(hipFree(cp.cu_floatimage));
        CUDA_CHECK_RETURN(hipFree(cp.cu_omega_reduction));
        CUDA_CHECK_RETURN(hipFree(cp.cu_max_I_x_reduction));
        CUDA_CHECK_RETURN(hipFree(cp.cu_max_I_y_reduction));
        CUDA_CHECK_RETURN(hipFree(cp.cu_rangemap));
        // future use: CUDA_CHECK_RETURN(hipFree(newapi_cp.cu_energy_Fhkl));
        CUDA_CHECK_RETURN(hipFree(newapi_cp.cu_accumulate_floatimage));
}