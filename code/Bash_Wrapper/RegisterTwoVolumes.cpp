/*
 * BROCCOLI: An open source multi-platform software for parallel analysis of fMRI data on many core CPUs and GPUS
 * Copyright (C) <2013>  Anders Eklund, andek034@gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "broccoli_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include "nifti1_io.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include "opencl.h"
#include <time.h>
#include <sys/time.h>

#define ADD_FILENAME true
#define DONT_ADD_FILENAME true

//#define HAVE_ZLIB 1

#define CHECK_EXISTING_FILE true
#define DONT_CHECK_EXISTING_FILE false

void ConvertFloat2ToFloats(float* Real, float* Imag, cl_float2* Complex, int DATA_W, int DATA_H, int DATA_D)
{
    for (int z = 0; z < DATA_D; z++)
    {
        for (int y = 0; y < DATA_H; y++)
        {   
           for (int x = 0; x < DATA_W; x++)
           {    
               Real[x + y * DATA_W + z * DATA_W * DATA_H] = Complex[x + y * DATA_W + z * DATA_W * DATA_H].x;
               Imag[x + y * DATA_W + z * DATA_W * DATA_H] = Complex[x + y * DATA_W + z * DATA_W * DATA_H].y;
           }
        }
    }
    
}

void FreeAllMemory(void **pointers, int N)
{
    for (int i = 0; i < N; i++)
    {
        if (pointers[i] != NULL)
        {
            free(pointers[i]);
        }
    }
}

void FreeAllNiftiImages(nifti_image **niftiImages, int N)
{
    for (int i = 0; i < N; i++)
    {
		if (niftiImages[i] != NULL)
		{
			nifti_image_free(niftiImages[i]);
		}
    }
}

void ReadBinaryFile(float* pointer, int size, const char* filename, void** pointers, int& Npointers, nifti_image** niftiImages, int Nimages)
{
	if (pointer == NULL)
    {
        printf("The provided pointer for file %s is NULL, aborting! \n",filename);
        FreeAllMemory(pointers,Npointers);
		FreeAllNiftiImages(niftiImages,Nimages);
        exit(EXIT_FAILURE);
	}	

	FILE *fp = NULL; 
	fp = fopen(filename,"rb");

    if (fp != NULL)
    {
        fread(pointer,sizeof(float),size,fp);
        fclose(fp);
    }
    else
    {
        printf("Could not open %s , aborting! \n",filename);
        FreeAllMemory(pointers,Npointers);
		FreeAllNiftiImages(niftiImages,Nimages);
        exit(EXIT_FAILURE);
    }
}

void AllocateMemory(float *& pointer, int size, void** pointers, int& Npointers, nifti_image** niftiImages, int Nimages, const char* variable)
{
    pointer = (float*)malloc(size);
    if (pointer != NULL)
    {
        pointers[Npointers] = (void*)pointer;
        Npointers++;
    }
    else
    {
        printf("Could not allocate host memory for variable %s ! \n",variable);        
		FreeAllMemory(pointers, Npointers);
		FreeAllNiftiImages(niftiImages, Nimages);
		exit(EXIT_FAILURE);        
    }
}
    
void AllocateMemoryFloat2(cl_float2 *& pointer, int size, void** pointers, int& Npointers, nifti_image** niftiImages, int Nimages, const char* variable)
{
    pointer = (cl_float2*)malloc(size);
    if (pointer != NULL)
    {
        pointers[Npointers] = (void*)pointer;
        Npointers++;
    }
    else
    {
        printf("Could not allocate host memory for variable %s ! \n",variable);        
		FreeAllMemory(pointers, Npointers);
		FreeAllNiftiImages(niftiImages, Nimages);
		exit(EXIT_FAILURE);        
    }
}

bool WriteNifti(nifti_image* inputNifti, float* data, const char* filename, bool addFilename, bool checkFilename)
{       
	if (data == NULL)
    {
        printf("The provided data pointer for file %s is NULL, aborting writing nifti file! \n",filename);
		return false;
	}	
	if (inputNifti == NULL)
    {
        printf("The provided nifti pointer for file %s is NULL, aborting writing nifti file! \n",filename);
		return false;
	}	


    char* filenameWithExtension;
    
    // Add the provided filename to the original filename, before the dot
    if (addFilename)
    {
        // Find the dot in the original filename
        const char* p = inputNifti->fname;
        int dotPosition = 0;
        while ( (p != NULL) && ((*p) != '.') )
        {
            p++;
            dotPosition++;
        }
    
        // Allocate temporary array
        filenameWithExtension = (char*)malloc(strlen(inputNifti->fname) + strlen(filename) + 1);
        if (filenameWithExtension == NULL)
        {
            printf("Could not allocate temporary host memory! \n");      
            return false;
        }
    
        // Copy filename to the dot
        strncpy(filenameWithExtension,inputNifti->fname,dotPosition);
        filenameWithExtension[dotPosition] = '\0';
        // Add the extension
        strcat(filenameWithExtension,filename);
        // Add the rest of the original filename
        strcat(filenameWithExtension,inputNifti->fname+dotPosition);    
    }
        
    // Copy information from input data
    nifti_image *outputNifti = nifti_copy_nim_info(inputNifti);    
    // Set data pointer 
    outputNifti->data = (void*)data;        
    // Set data type to float
    outputNifti->datatype = DT_FLOAT;
    outputNifti->nbyper = 4;    
    
    // Change filename and write
    bool written = false;
    if (addFilename)
    {
        if ( nifti_set_filenames(outputNifti, filenameWithExtension, checkFilename, 1) == 0)
        {
            nifti_image_write(outputNifti);
            written = true;
        }
    }
    else if (!addFilename)
    {
        if ( nifti_set_filenames(outputNifti, filename, checkFilename, 1) == 0)
        {
            nifti_image_write(outputNifti);
            written = true;
        }                
    }    
    
    outputNifti->data = NULL;
    nifti_image_free(outputNifti);

    if (addFilename)
    {
        free(filenameWithExtension);
    } 
        
    if (written)
    {      
        return true;
    }
    else
    {
        return false;
    }                        
}

double GetWallTime()
{
    struct timeval time;
    if (gettimeofday(&time,NULL))
    {
        //  Handle error
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}


int main(int argc, char **argv)
{
    //-----------------------
    // Input pointers
    
    float           *h_T1_Volume, *h_MNI_Volume, *h_MNI_Brain_Volume, *h_MNI_Brain_Mask;
    
    float           *h_Quadrature_Filter_1_Parametric_Registration_Real, *h_Quadrature_Filter_2_Parametric_Registration_Real, *h_Quadrature_Filter_3_Parametric_Registration_Real, *h_Quadrature_Filter_1_Parametric_Registration_Imag, *h_Quadrature_Filter_2_Parametric_Registration_Imag, *h_Quadrature_Filter_3_Parametric_Registration_Imag;
    float           *h_Quadrature_Filter_1_NonParametric_Registration_Real, *h_Quadrature_Filter_2_NonParametric_Registration_Real, *h_Quadrature_Filter_3_NonParametric_Registration_Real, *h_Quadrature_Filter_1_NonParametric_Registration_Imag, *h_Quadrature_Filter_2_NonParametric_Registration_Imag, *h_Quadrature_Filter_3_NonParametric_Registration_Imag;
    float           *h_Quadrature_Filter_4_NonParametric_Registration_Real, *h_Quadrature_Filter_5_NonParametric_Registration_Real, *h_Quadrature_Filter_6_NonParametric_Registration_Real, *h_Quadrature_Filter_4_NonParametric_Registration_Imag, *h_Quadrature_Filter_5_NonParametric_Registration_Imag, *h_Quadrature_Filter_6_NonParametric_Registration_Imag;
        
    float           *h_Projection_Tensor_1, *h_Projection_Tensor_2, *h_Projection_Tensor_3, *h_Projection_Tensor_4, *h_Projection_Tensor_5, *h_Projection_Tensor_6;
        
    float           *h_Filter_Directions_X, *h_Filter_Directions_Y, *h_Filter_Directions_Z;
        
    //-----------------------
    // Output pointers        
    
    float           *h_t11, *h_t12, *h_t13, *h_t22, *h_t23, *h_t33;
    
    float           *h_Displacement_Field_X, *h_Displacement_Field_Y, *h_Displacement_Field_Z;
    
    cl_float2       *h_Quadrature_Filter_Response_1, *h_Quadrature_Filter_Response_2, *h_Quadrature_Filter_Response_3, *h_Quadrature_Filter_Response_4, *h_Quadrature_Filter_Response_5, *h_Quadrature_Filter_Response_6;
    
    float           *h_Quadrature_Filter_Response_1_Real, *h_Quadrature_Filter_Response_1_Imag;
    float           *h_Quadrature_Filter_Response_2_Real, *h_Quadrature_Filter_Response_2_Imag;
    float           *h_Quadrature_Filter_Response_3_Real, *h_Quadrature_Filter_Response_3_Imag;  
    float           *h_Quadrature_Filter_Response_4_Real, *h_Quadrature_Filter_Response_4_Imag;
    float           *h_Quadrature_Filter_Response_5_Real, *h_Quadrature_Filter_Response_5_Imag;
    float           *h_Quadrature_Filter_Response_6_Real, *h_Quadrature_Filter_Response_6_Imag;      
    float           *h_Phase_Differences, *h_Phase_Certainties, *h_Phase_Gradients;
    float           *h_Aligned_T1_Volume, *h_Aligned_T1_Volume_NonParametric, *h_Interpolated_T1_Volume, *h_Registration_Parameters;
    float           *h_Downsampled_Volume;
    float           *h_Skullstripped_T1_Volume;
    float           *h_Slice_Sums, *h_Top_Slice;
    float           *h_A_Matrix, *h_h_Vector;
        
    void*           allMemoryPointers[500];
    int             numberOfMemoryPointers = 0;

	nifti_image*	allNiftiImages[500];
	int				numberOfNiftiImages = 0;
    
    // Default parameters
        
    int             NUMBER_OF_FILTERS_FOR_NONLINEAR_REGISTRATION = 6;
    int             IMAGE_REGISTRATION_FILTER_SIZE = 7;
    int             NUMBER_OF_AFFINE_IMAGE_REGISTRATION_PARAMETERS = 12;
    int             NUMBER_OF_ITERATIONS_FOR_LINEAR_IMAGE_REGISTRATION = 15;
    int             NUMBER_OF_ITERATIONS_FOR_NONLINEAR_IMAGE_REGISTRATION = 10;
    int             COARSEST_SCALE = 4;
    int             MM_T1_Z_CUT = 0;
    int             OPENCL_PLATFORM = 0;
    int             OPENCL_DEVICE = 0;
    bool            DEBUG = false;
    const char*     FILENAME_EXTENSION = "_MNI";
    bool            PRINT = true;
	bool			VERBOS = false;
    bool            FLIPUD = false;
    bool            FLIPBF = false;
    bool            FLIPLR = false;
    bool            WRITE_DISPLACEMENT_FIELD = false;
	bool			WRITE_INTERPOLATED = false;
   	bool			CHANGE_OUTPUT_NAME = false;    
	float			TSIGMA = 2.25f;
	float			ESIGMA = 2.25f;
	float			DSIGMA = 2.25f;

	const char*		outputFilename;

    // Size parameters
    int             T1_DATA_H, T1_DATA_W, T1_DATA_D;
    int             MNI_DATA_W, MNI_DATA_H, MNI_DATA_D;
    
    float           T1_VOXEL_SIZE_X, T1_VOXEL_SIZE_Y, T1_VOXEL_SIZE_Z;
    float           MNI_VOXEL_SIZE_X, MNI_VOXEL_SIZE_Y, MNI_VOXEL_SIZE_Z;    
        
    //---------------------    
    
    /* Input arguments */
    FILE *fp = NULL; 
    
    // No inputs, so print help text
    if (argc == 1)
    {   
		printf("\nThe function aligns two volumes using linear as well as non-linear registration. The function automagically resizes and rescales the input volume to match the reference volume.\n\n");     
        printf("Usage:\n\n");
        printf("RegisterTwoVolumes input_volume.nii reference_volume.nii [options]\n\n");
        printf("Options:\n\n");
        printf(" -platform                  The OpenCL platform to use (default 0) \n");
        printf(" -device                    The OpenCL device to use for the specificed platform (default 0) \n");
        printf(" -iterationslinear          Number of iterations for the linear registration (default 10) \n");        
        printf(" -iterationsnonlinear       Number of iterations for the non-linear registration (default 10), 0 means that no non-linear registration is performed \n");        
        printf(" -lowestscale               The lowest scale for the linear and non-linear registration, should be 1, 2, 4 or 8 (default 4), x means downsampling a factor x in each dimension  \n");        
        printf(" -tsigma                    Amount of Gaussian smoothing applied to the estimated tensor components, defined as sigma of the Gaussian kernel (default 2.25)  \n");        
        printf(" -esigma                    Amount of Gaussian smoothing applied to the equation systems (one in each voxel), defined as sigma of the Gaussian kernel (default 2.25)  \n");        
        printf(" -dsigma                    Amount of Gaussian smoothing applied to the displacement fields (x,y,z), defined as sigma of the Gaussian kernel (default 2.25)  \n");        
        printf(" -zcut                      Number of mm to cut from the bottom of the input volume, can be negative, useful if the head in the volume is placed very high or low (default 0) \n");        
		printf(" -savefield                 Saves the displacement field to file (default false) \n");        
		printf(" -saveinterpolated          Saves the input volume rescaled and resized to the size and resolution of the reference volume, before alignment (default false) \n");        
        //printf(" -flipbf                    Flip the volume back to front before registration (invert x-axis) \n");        
        //printf(" -fliplr                    Flip the volume left to right before registration (invert y-axis) \n");        
        //printf(" -flipud                    Flip the volume upside down before registration (invert z-axis) \n");                
		printf(" -output                    Set output filename (default input_volume_aligned_linear.nii and input_volume_aligned_nonlinear.nii) \n");
        printf(" -quiet                     Don't print anything to the terminal (default false) \n");
        printf(" -verbose                   Print extra stuff (default false) \n");
        printf(" -debug                     Get additional debug information saved as nifti files (default no). Warning: This will use a lot of extra memory! \n");
        printf("\n\n");
        
        return EXIT_SUCCESS;
    }
    else if (argc < 3)
    {
        printf("\nNeed two input volumes!\n\n");
        printf("Usage:\n\n");
        printf("RegisterTwoVolumes input_volume.nii reference_volume.nii [options]\n\n");
		return EXIT_FAILURE;
    }
    // Try to open files
    else if (argc > 1)
    {        
        fp = fopen(argv[1],"r");
        if (fp == NULL)
        {
            printf("Could not open file %s !\n",argv[1]);
            return EXIT_FAILURE;
        }
        fclose(fp);     
        
        fp = fopen(argv[2],"r");
        if (fp == NULL)
        {
            printf("Could not open file %s !\n",argv[2]);
            return EXIT_FAILURE;
        }
        fclose(fp);            
    }
    
    // Loop over additional inputs
    int i = 3;
    while (i < argc)
    {
        char *input = argv[i];
        char *p;
        if (strcmp(input,"-platform") == 0)
        {
			if ( (i+1) >= argc  )
			{
			    printf("Unable to read value after -platform !\n");
                return EXIT_FAILURE;
			}

            OPENCL_PLATFORM = (int)strtol(argv[i+1], &p, 10);
			
			if (!isspace(*p) && *p != 0)
		    {
		        printf("OpenCL platform must be an integer! You provided %s \n",argv[i+1]);
				return EXIT_FAILURE;
		    }
            else if (OPENCL_PLATFORM < 0)
            {
                printf("OpenCL platform must be >= 0!\n");
                return EXIT_FAILURE;
            }
            i += 2;
        }
        else if (strcmp(input,"-device") == 0)
        {
			if ( (i+1) >= argc  )
			{
			    printf("Unable to read value after -device !\n");
                return EXIT_FAILURE;
			}

            OPENCL_DEVICE = (int)strtol(argv[i+1], &p, 10);

			if (!isspace(*p) && *p != 0)
		    {
		        printf("OpenCL device must be an integer! You provided %s \n",argv[i+1]);
				return EXIT_FAILURE;
		    }
            else if (OPENCL_DEVICE < 0)
            {
                printf("OpenCL device must be >= 0!\n");
                return EXIT_FAILURE;
            }
            i += 2;
        }
        else if (strcmp(input,"-iterationslinear") == 0)
        {
			if ( (i+1) >= argc  )
			{
			    printf("Unable to read value after -iterationslinear !\n");
                return EXIT_FAILURE;
			}

            NUMBER_OF_ITERATIONS_FOR_LINEAR_IMAGE_REGISTRATION = (int)strtol(argv[i+1], &p, 10);

			if (!isspace(*p) && *p != 0)
		    {
		        printf("Number of iterations must be an integer! You provided %s \n",argv[i+1]);
				return EXIT_FAILURE;
		    }
            else if (NUMBER_OF_ITERATIONS_FOR_LINEAR_IMAGE_REGISTRATION <= 0)
            {
                printf("Number of linear iterations must be a positive number!\n");
                return EXIT_FAILURE;
            }
            i += 2;
        }
        else if (strcmp(input,"-iterationsnonlinear") == 0)
        {
			if ( (i+1) >= argc  )
			{
			    printf("Unable to read value after -iterationslinear !\n");
                return EXIT_FAILURE;
			}

            NUMBER_OF_ITERATIONS_FOR_NONLINEAR_IMAGE_REGISTRATION = (int)strtol(argv[i+1], &p, 10);

			if (!isspace(*p) && *p != 0)
		    {
		        printf("Number of iterations must be an integer! You provided %s \n",argv[i+1]);
				return EXIT_FAILURE;
		    }
            else if (NUMBER_OF_ITERATIONS_FOR_NONLINEAR_IMAGE_REGISTRATION < 0)
            {
                printf("Number of non-linear iterations must be >= 0 !\n");
                return EXIT_FAILURE;
            }
            i += 2;
        }
        else if (strcmp(input,"-lowestscale") == 0)
        {
			if ( (i+1) >= argc  )
			{
			    printf("Unable to read value after -lowestscale !\n");
                return EXIT_FAILURE;
			}

            COARSEST_SCALE = (int)strtol(argv[i+1], &p, 10);

			if (!isspace(*p) && *p != 0)
		    {
		        printf("Lowest scale must be an integer! You provided %s \n",argv[i+1]);
				return EXIT_FAILURE;
		    }
  			else if ( (COARSEST_SCALE != 1) && (COARSEST_SCALE != 2) && (COARSEST_SCALE != 4) && (COARSEST_SCALE != 8) )
            {
                printf("Lowest scale must be 1, 2, 4 or 8!\n");
                return EXIT_FAILURE;
            }
            i += 2;
        }
        else if (strcmp(input,"-tsigma") == 0)
        {
			if ( (i+1) >= argc  )
			{
			    printf("Unable to read value after -tsigma !\n");
                return EXIT_FAILURE;
			}

            TSIGMA = strtod(argv[i+1], &p);

			if (!isspace(*p) && *p != 0)
		    {
		        printf("tsigma must be a float! You provided %s \n",argv[i+1]);
				return EXIT_FAILURE;
		    }
  			else if ( TSIGMA < 0.0f )
            {
                printf("tsigma must be >= 0.0 !\n");
                return EXIT_FAILURE;
            }
            i += 2;
        }
        else if (strcmp(input,"-esigma") == 0)
        {
			if ( (i+1) >= argc  )
			{
			    printf("Unable to read value after -esigma !\n");
                return EXIT_FAILURE;
			}

            ESIGMA = strtod(argv[i+1], &p);

			if (!isspace(*p) && *p != 0)
		    {
		        printf("esigma must be a float! You provided %s \n",argv[i+1]);
				return EXIT_FAILURE;
		    }
  			else if ( ESIGMA < 0.0f )
            {
                printf("esigma must be >= 0.0 !\n");
                return EXIT_FAILURE;
            }
            i += 2;
        }
        else if (strcmp(input,"-dsigma") == 0)
        {
			if ( (i+1) >= argc  )
			{
			    printf("Unable to read value after -dsigma !\n");
                return EXIT_FAILURE;
			}

            DSIGMA = strtod(argv[i+1], &p);

			if (!isspace(*p) && *p != 0)
		    {
		        printf("dsigma must be a float! You provided %s \n",argv[i+1]);
				return EXIT_FAILURE;
		    }
  			else if ( DSIGMA < 0.0f )
            {
                printf("dsigma must be >= 0.0 !\n");
                return EXIT_FAILURE;
            }
            i += 2;
        }
        else if (strcmp(input,"-zcut") == 0)
        {
			if ( (i+1) >= argc  )
			{
			    printf("Unable to read value after -zcut !\n");
                return EXIT_FAILURE;
			}

            MM_T1_Z_CUT = (int)strtol(argv[i+1], &p, 10);

			if (!isspace(*p) && *p != 0)
		    {
		        printf("zcut must be an integer! You provided %s \n",argv[i+1]);
				return EXIT_FAILURE;
		    }

            i += 2;
        }
        else if (strcmp(input,"-savefield") == 0)
        {
            WRITE_DISPLACEMENT_FIELD = true;
            i += 1;
        }
        else if (strcmp(input,"-saveinterpolated") == 0)
        {
            WRITE_INTERPOLATED = true;
            i += 1;
        }
        else if (strcmp(input,"-flipbf") == 0)
        {
            FLIPBF = true;
            i += 1;
        }
        else if (strcmp(input,"-fliplr") == 0)
        {
            FLIPLR = true;
            i += 1;
        }
        else if (strcmp(input,"-flipud") == 0)
        {
            FLIPUD = true;
            i += 1;
        }
        
        else if (strcmp(input,"-debug") == 0)
        {
            DEBUG = true;
            i += 1;
        }
        else if (strcmp(input,"-quiet") == 0)
        {
            PRINT = false;
            i += 1;
        }
        else if (strcmp(input,"-verbose") == 0)
        {
            VERBOS = true;
            i += 1;
        }
        else if (strcmp(input,"-output") == 0)
        {
			if ( (i+1) >= argc  )
			{
			    printf("Unable to read name after -output !\n");
                return EXIT_FAILURE;
			}

			CHANGE_OUTPUT_NAME = true;
            outputFilename = argv[i+1];
            i += 2;
        }
        else
        {
            printf("Unrecognized option! %s \n",argv[i]);
            return EXIT_FAILURE;
        }                
    }
    	
	double startTime = GetWallTime();
    
    // Read data

    nifti_image *inputT1 = nifti_image_read(argv[1],1);
    
    if (inputT1 == NULL)
    {
        printf("Could not open first volume!\n");
        return EXIT_FAILURE;
    }
	allNiftiImages[numberOfNiftiImages] = inputT1;
	numberOfNiftiImages++;
    
    nifti_image *inputMNI = nifti_image_read(argv[2],1);
    
    if (inputMNI == NULL)
    {
        printf("Could not open second volume!\n");
        return EXIT_FAILURE;
    }
	allNiftiImages[numberOfNiftiImages] = inputMNI;
	numberOfNiftiImages++;
    	
	double endTime = GetWallTime();

	if (VERBOS)
 	{
		printf("It took %f seconds to read the two nifti files\n",(float)(endTime - startTime));
	}

    // Get data dimensions from input data
    T1_DATA_W = inputT1->nx;
    T1_DATA_H = inputT1->ny;
    T1_DATA_D = inputT1->nz;    
    
    MNI_DATA_W = inputMNI->nx;
    MNI_DATA_H = inputMNI->ny;
    MNI_DATA_D = inputMNI->nz;    
    
    // Get voxel sizes from input data
    T1_VOXEL_SIZE_X = inputT1->dx;
    T1_VOXEL_SIZE_Y = inputT1->dy;
    T1_VOXEL_SIZE_Z = inputT1->dz;
                             
    MNI_VOXEL_SIZE_X = inputMNI->dx;
    MNI_VOXEL_SIZE_Y = inputMNI->dy;
    MNI_VOXEL_SIZE_Z = inputMNI->dz;
    
    if ( (MNI_VOXEL_SIZE_X == 2.0f) && (COARSEST_SCALE == 8) )
    {
        printf("\n\nWARNING: It is not recommended to use 8 as a lowest scale for a 2 mm reference volume.\n\n");   
    }
    
    // Calculate size, in bytes
    
    int DOWNSAMPLED_DATA_W = (int)round((float)MNI_DATA_W/(float)COARSEST_SCALE);
	int DOWNSAMPLED_DATA_H = (int)round((float)MNI_DATA_H/(float)COARSEST_SCALE);
	int DOWNSAMPLED_DATA_D = (int)round((float)MNI_DATA_D/(float)COARSEST_SCALE);
                
   	int T1_DATA_W_INTERPOLATED = (int)round((float)T1_DATA_W * T1_VOXEL_SIZE_X / MNI_VOXEL_SIZE_X);
	int T1_DATA_H_INTERPOLATED = (int)round((float)T1_DATA_H * T1_VOXEL_SIZE_Y / MNI_VOXEL_SIZE_Y);
	int T1_DATA_D_INTERPOLATED = (int)round((float)T1_DATA_D * T1_VOXEL_SIZE_Z / MNI_VOXEL_SIZE_Z);

    int IMAGE_REGISTRATION_PARAMETERS_SIZE = NUMBER_OF_AFFINE_IMAGE_REGISTRATION_PARAMETERS * sizeof(float);
    int FILTER_SIZE = IMAGE_REGISTRATION_FILTER_SIZE * IMAGE_REGISTRATION_FILTER_SIZE * IMAGE_REGISTRATION_FILTER_SIZE * sizeof(float);
    int FILTER_SIZE2 = IMAGE_REGISTRATION_FILTER_SIZE * IMAGE_REGISTRATION_FILTER_SIZE * IMAGE_REGISTRATION_FILTER_SIZE * sizeof(cl_float2);
    int T1_VOLUME_SIZE = T1_DATA_W * T1_DATA_H * T1_DATA_D * sizeof(float);
    int MNI_VOLUME_SIZE = MNI_DATA_W * MNI_DATA_H * MNI_DATA_D * sizeof(float);
    int MNI2_VOLUME_SIZE = MNI_DATA_W * MNI_DATA_H * MNI_DATA_D * sizeof(cl_float2);
    int INTERPOLATED_T1_VOLUME_SIZE = T1_DATA_W_INTERPOLATED * T1_DATA_H_INTERPOLATED * T1_DATA_D_INTERPOLATED * sizeof(float);
    int DOWNSAMPLED_VOLUME_SIZE = DOWNSAMPLED_DATA_W * DOWNSAMPLED_DATA_H * DOWNSAMPLED_DATA_D * sizeof(float);
    int PROJECTION_TENSOR_SIZE = NUMBER_OF_FILTERS_FOR_NONLINEAR_REGISTRATION * sizeof(float);
    int FILTER_DIRECTIONS_SIZE = NUMBER_OF_FILTERS_FOR_NONLINEAR_REGISTRATION * sizeof(float);
    
    // Print some info
    if (PRINT)
    {
        printf("Authored by K.A. Eklund \n");
        printf("Volume 1 size: %i x %i x %i \n",  T1_DATA_W, T1_DATA_H, T1_DATA_D);
        printf("Volume 1 voxel size: %f x %f x %f mm \n", T1_VOXEL_SIZE_X, T1_VOXEL_SIZE_Y, T1_VOXEL_SIZE_Z);    
        printf("Volume 2 size: %i x %i x %i \n",  MNI_DATA_W, MNI_DATA_H, MNI_DATA_D);
        printf("Volume 2 voxel size: %f x %f x %f mm \n", MNI_VOXEL_SIZE_X, MNI_VOXEL_SIZE_Y, MNI_VOXEL_SIZE_Z);    
        printf("Dsigma %f \n", DSIGMA);    
    }
            
    // ------------------------------------------------
    
    // Allocate memory on the host

	startTime = GetWallTime();
    
	AllocateMemory(h_T1_Volume, T1_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "INPUT_VOLUME");
	AllocateMemory(h_Interpolated_T1_Volume, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "INTERPOLATED_INPUT_VOLUME");
	AllocateMemory(h_MNI_Volume, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "REFERENCE_VOLUME");
	AllocateMemory(h_Aligned_T1_Volume, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "LINEARLY_ALIGNED_INPUT_VOLUME");    
   	AllocateMemory(h_Aligned_T1_Volume_NonParametric, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "NONLINEARLY_ALIGNED_INPUT_VOLUME");    
   	AllocateMemory(h_Registration_Parameters, IMAGE_REGISTRATION_PARAMETERS_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "REGISTRATION_PARAMETERS");    
        
	AllocateMemory(h_Quadrature_Filter_1_Parametric_Registration_Real, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_1_LINEAR_REGISTRATION_REAL");    
	AllocateMemory(h_Quadrature_Filter_1_Parametric_Registration_Imag, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_1_LINEAR_REGISTRATION_IMAG");    
	AllocateMemory(h_Quadrature_Filter_2_Parametric_Registration_Real, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_2_LINEAR_REGISTRATION_REAL");    
	AllocateMemory(h_Quadrature_Filter_2_Parametric_Registration_Imag, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_2_LINEAR_REGISTRATION_IMAG");    
	AllocateMemory(h_Quadrature_Filter_3_Parametric_Registration_Real, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_3_LINEAR_REGISTRATION_REAL");    
	AllocateMemory(h_Quadrature_Filter_3_Parametric_Registration_Imag, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_3_LINEAR_REGISTRATION_IMAG");    
    
	AllocateMemory(h_Quadrature_Filter_1_NonParametric_Registration_Real, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_1_NONLINEAR_REGISTRATION_REAL");    
	AllocateMemory(h_Quadrature_Filter_1_NonParametric_Registration_Imag, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_1_NONLINEAR_REGISTRATION_IMAG");    
	AllocateMemory(h_Quadrature_Filter_2_NonParametric_Registration_Real, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_2_NONLINEAR_REGISTRATION_REAL");    
	AllocateMemory(h_Quadrature_Filter_2_NonParametric_Registration_Imag, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_2_NONLINEAR_REGISTRATION_IMAG");    
	AllocateMemory(h_Quadrature_Filter_3_NonParametric_Registration_Real, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_3_NONLINEAR_REGISTRATION_REAL");    
	AllocateMemory(h_Quadrature_Filter_3_NonParametric_Registration_Imag, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_3_NONLINEAR_REGISTRATION_IMAG");    
	AllocateMemory(h_Quadrature_Filter_4_NonParametric_Registration_Real, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_4_NONLINEAR_REGISTRATION_REAL");    
	AllocateMemory(h_Quadrature_Filter_4_NonParametric_Registration_Imag, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_4_NONLINEAR_REGISTRATION_IMAG");    
	AllocateMemory(h_Quadrature_Filter_5_NonParametric_Registration_Real, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_5_NONLINEAR_REGISTRATION_REAL");    
	AllocateMemory(h_Quadrature_Filter_5_NonParametric_Registration_Imag, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_5_NONLINEAR_REGISTRATION_IMAG");    
	AllocateMemory(h_Quadrature_Filter_6_NonParametric_Registration_Real, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_6_NONLINEAR_REGISTRATION_REAL");    
	AllocateMemory(h_Quadrature_Filter_6_NonParametric_Registration_Imag, FILTER_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_6_NONLINEAR_REGISTRATION_IMAG");    

    AllocateMemory(h_Projection_Tensor_1, PROJECTION_TENSOR_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "PROJECTION_TENSOR_1");    
    AllocateMemory(h_Projection_Tensor_2, PROJECTION_TENSOR_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "PROJECTION_TENSOR_2");    
    AllocateMemory(h_Projection_Tensor_3, PROJECTION_TENSOR_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "PROJECTION_TENSOR_3");    
    AllocateMemory(h_Projection_Tensor_4, PROJECTION_TENSOR_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "PROJECTION_TENSOR_4");    
    AllocateMemory(h_Projection_Tensor_5, PROJECTION_TENSOR_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "PROJECTION_TENSOR_5");    
    AllocateMemory(h_Projection_Tensor_6, PROJECTION_TENSOR_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "PROJECTION_TENSOR_6");    

    AllocateMemory(h_Filter_Directions_X, FILTER_DIRECTIONS_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "FILTER_DIRECTIONS_X");
    AllocateMemory(h_Filter_Directions_Y, FILTER_DIRECTIONS_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "FILTER_DIRECTIONS_Y");        
    AllocateMemory(h_Filter_Directions_Z, FILTER_DIRECTIONS_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "FILTER_DIRECTIONS_Z");                
      
    if (WRITE_DISPLACEMENT_FIELD)
    {
	    AllocateMemory(h_Displacement_Field_X, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "DISPLACEMENT_FIELD_X");
		AllocateMemory(h_Displacement_Field_Y, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "DISPLACEMENT_FIELD_Y");        
		AllocateMemory(h_Displacement_Field_Z, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "DISPLACEMENT_FIELD_Z");                
    }
    
    if (DEBUG)
    {                    
		AllocateMemory(h_Quadrature_Filter_Response_1_Real, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_RESPONSE_1_REAL");
		AllocateMemory(h_Quadrature_Filter_Response_1_Imag, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_RESPONSE_1_IMAG");                
		AllocateMemory(h_Quadrature_Filter_Response_2_Real, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_RESPONSE_2_REAL");
		AllocateMemory(h_Quadrature_Filter_Response_2_Imag, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_RESPONSE_2_IMAG");                
		AllocateMemory(h_Quadrature_Filter_Response_3_Real, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_RESPONSE_3_REAL");
		AllocateMemory(h_Quadrature_Filter_Response_3_Imag, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_RESPONSE_3_IMAG");                

		AllocateMemoryFloat2(h_Quadrature_Filter_Response_1, MNI2_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_RESPONSE_1");
		AllocateMemoryFloat2(h_Quadrature_Filter_Response_2, MNI2_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_RESPONSE_2");
		AllocateMemoryFloat2(h_Quadrature_Filter_Response_3, MNI2_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "QUADRATURE_FILTER_RESPONSE_3");

		AllocateMemory(h_Phase_Differences, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "PHASE_DIFFERENCES");                
		AllocateMemory(h_Phase_Certainties, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "PHASE_CERTAINTIES");                
		AllocateMemory(h_Phase_Gradients, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "PHASE_GRADIENTS");                

		AllocateMemory(h_t11, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "TENSOR_11");
		AllocateMemory(h_t12, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "TENSOR_12");                
		AllocateMemory(h_t13, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "TENSOR_13");                
		AllocateMemory(h_t22, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "TENSOR_22");                
		AllocateMemory(h_t23, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "TENSOR_23");                
		AllocateMemory(h_t33, MNI_VOLUME_SIZE, allMemoryPointers, numberOfMemoryPointers, allNiftiImages, numberOfNiftiImages, "TENSOR_33");                                
    }

	endTime = GetWallTime();
    
	if (VERBOS)
 	{
		printf("It took %f seconds to allocate memory\n",(float)(endTime - startTime));
	}

	startTime = GetWallTime();

    // Convert data to floats
    if ( inputT1->datatype == DT_SIGNED_SHORT )
    {
        short int *p = (short int*)inputT1->data;
    
        for (int i = 0; i < T1_DATA_W * T1_DATA_H * T1_DATA_D; i++)
        {
            h_T1_Volume[i] = (float)p[i];
        }
    }
    else if ( inputT1->datatype == DT_FLOAT )
    {
        float *p = (float*)inputT1->data;
    
        for (int i = 0; i < T1_DATA_W * T1_DATA_H * T1_DATA_D; i++)
        {
            h_T1_Volume[i] = p[i];
        }
    }
    else
    {
        printf("Unknown data type in input volume, aborting!\n");
        FreeAllMemory(allMemoryPointers,numberOfMemoryPointers);
		FreeAllNiftiImages(allNiftiImages,numberOfNiftiImages);
        return EXIT_FAILURE;
    }
    
    if ( inputMNI->datatype == DT_SIGNED_SHORT )
    {
        short int *p = (short int*)inputMNI->data;
    
        for (int i = 0; i < MNI_DATA_W * MNI_DATA_H * MNI_DATA_D; i++)
        {
            h_MNI_Volume[i] = (float)p[i];
        }
    }
    else if ( inputMNI->datatype == DT_FLOAT )
    {
        float *p = (float*)inputMNI->data;
    
        for (int i = 0; i < MNI_DATA_W * MNI_DATA_H * MNI_DATA_D; i++)
        {
            h_MNI_Volume[i] = p[i];
        }
    }
    else
    {
        printf("Unknown data type in reference volume, aborting!\n");
        FreeAllMemory(allMemoryPointers,numberOfMemoryPointers);
		FreeAllNiftiImages(allNiftiImages,numberOfNiftiImages);
        return EXIT_FAILURE;
    }
    
	endTime = GetWallTime();

	if (VERBOS)
 	{
		printf("It took %f seconds to convert data to floats\n",(float)(endTime - startTime));
	}

    if (FLIPUD)
    {
        float* temp = (float*)calloc(T1_DATA_W*T1_DATA_H*T1_DATA_D,sizeof(float));
        
        int inverted_z = T1_DATA_D-1;
        for (int z = 0; z < T1_DATA_D; z++)
        {
            for (int y = 0; y < T1_DATA_H; y++)
            {
                for (int x = 0; x < T1_DATA_W; x++)
                {
                    temp[x + y * T1_DATA_W + z * T1_DATA_W * T1_DATA_H] = h_T1_Volume[x + y * T1_DATA_W + inverted_z * T1_DATA_W * T1_DATA_H];
                }
            }
            inverted_z--;
        }
        
        memcpy(h_T1_Volume,temp,T1_DATA_W*T1_DATA_H*T1_DATA_D*sizeof(float));
        
        free(temp);
    }
    if (FLIPBF)
    {   
        float* temp = (float*)calloc(T1_DATA_W*T1_DATA_H*T1_DATA_D,sizeof(float));
        
        for (int z = 0; z < T1_DATA_D; z++)
        {
            for (int y = 0; y < T1_DATA_H; y++)
            {
                int inverted_x = T1_DATA_W-1;
                for (int x = 0; x < T1_DATA_W; x++)
                {
                    temp[x + y * T1_DATA_W + z * T1_DATA_W * T1_DATA_H] = h_T1_Volume[inverted_x + y * T1_DATA_W + z * T1_DATA_W * T1_DATA_H];
                    inverted_x--;
                }
            }            
        }
        
        memcpy(h_T1_Volume,temp,T1_DATA_W*T1_DATA_H*T1_DATA_D*sizeof(float));
        
        free(temp);
    }
    if (FLIPLR)
    {        
        float* temp = (float*)calloc(T1_DATA_W*T1_DATA_H*T1_DATA_D,sizeof(float));
        
        for (int z = 0; z < T1_DATA_D; z++)
        {
            int inverted_y = T1_DATA_H-1;
            for (int y = 0; y < T1_DATA_H; y++)
            {                
                for (int x = 0; x < T1_DATA_W; x++)
                {
                    temp[x + y * T1_DATA_W + z * T1_DATA_W * T1_DATA_H] = h_T1_Volume[x + inverted_y * T1_DATA_W + z * T1_DATA_W * T1_DATA_H];                    
                }
                inverted_y--;
            }            
        }
        
        memcpy(h_T1_Volume,temp,T1_DATA_W*T1_DATA_H*T1_DATA_D*sizeof(float));
        
        free(temp);
    }

	startTime = GetWallTime();
    
    // Read quadrature filters for parametric registration, three real valued and three imaginary valued
	ReadBinaryFile(h_Quadrature_Filter_1_Parametric_Registration_Real,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter1_real_parametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_1_Parametric_Registration_Imag,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter1_imag_parametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_2_Parametric_Registration_Real,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter2_real_parametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_2_Parametric_Registration_Imag,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter2_imag_parametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_3_Parametric_Registration_Real,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter3_real_parametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_3_Parametric_Registration_Imag,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter3_imag_parametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 

	// Read quadrature filters for nonparametric registration, six real valued and six imaginary valued
	ReadBinaryFile(h_Quadrature_Filter_1_NonParametric_Registration_Real,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter1_real_nonparametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_1_NonParametric_Registration_Imag,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter1_imag_nonparametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_2_NonParametric_Registration_Real,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter2_real_nonparametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_2_NonParametric_Registration_Imag,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter2_imag_nonparametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_3_NonParametric_Registration_Real,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter3_real_nonparametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_3_NonParametric_Registration_Imag,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter3_imag_nonparametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_4_NonParametric_Registration_Real,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter4_real_nonparametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_4_NonParametric_Registration_Imag,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter4_imag_nonparametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_5_NonParametric_Registration_Real,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter5_real_nonparametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_5_NonParametric_Registration_Imag,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter5_imag_nonparametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_6_NonParametric_Registration_Real,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter6_real_nonparametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
	ReadBinaryFile(h_Quadrature_Filter_6_NonParametric_Registration_Imag,IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE*IMAGE_REGISTRATION_FILTER_SIZE,"filter6_imag_nonparametric_registration.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 

    // Read projection tensors   
    ReadBinaryFile(h_Projection_Tensor_1,NUMBER_OF_FILTERS_FOR_NONLINEAR_REGISTRATION,"projection_tensor1.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
    ReadBinaryFile(h_Projection_Tensor_2,NUMBER_OF_FILTERS_FOR_NONLINEAR_REGISTRATION,"projection_tensor2.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
    ReadBinaryFile(h_Projection_Tensor_3,NUMBER_OF_FILTERS_FOR_NONLINEAR_REGISTRATION,"projection_tensor3.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
    ReadBinaryFile(h_Projection_Tensor_4,NUMBER_OF_FILTERS_FOR_NONLINEAR_REGISTRATION,"projection_tensor4.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
    ReadBinaryFile(h_Projection_Tensor_5,NUMBER_OF_FILTERS_FOR_NONLINEAR_REGISTRATION,"projection_tensor5.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
    ReadBinaryFile(h_Projection_Tensor_6,NUMBER_OF_FILTERS_FOR_NONLINEAR_REGISTRATION,"projection_tensor6.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
        
    // Read filter directions
    ReadBinaryFile(h_Filter_Directions_X,NUMBER_OF_FILTERS_FOR_NONLINEAR_REGISTRATION,"filter_directions_x.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages);
    ReadBinaryFile(h_Filter_Directions_Y,NUMBER_OF_FILTERS_FOR_NONLINEAR_REGISTRATION,"filter_directions_y.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages); 
    ReadBinaryFile(h_Filter_Directions_Z,NUMBER_OF_FILTERS_FOR_NONLINEAR_REGISTRATION,"filter_directions_z.bin",allMemoryPointers,numberOfMemoryPointers,allNiftiImages,numberOfNiftiImages);  

	endTime = GetWallTime();

	if (VERBOS)
 	{
		printf("It took %f seconds to read all binary files\n",(float)(endTime - startTime));
	}       
    
    //------------------------
    
	startTime = GetWallTime();

	// Initialize BROCCOLI
    BROCCOLI_LIB BROCCOLI(OPENCL_PLATFORM,OPENCL_DEVICE,2); // 2 = Bash wrapper

	endTime = GetWallTime();

	if (VERBOS)
 	{
		printf("It took %f seconds to initiate BROCCOLI\n",(float)(endTime - startTime));
	}
    
    // Something went wrong...
    if (BROCCOLI.GetOpenCLInitiated() == 0)
    {              
		if (BROCCOLI.GetOpenCLPlatformIDsError() != 0)
		{
        	printf("Get platform IDs error is %s \n",BROCCOLI.GetOpenCLErrorMessage(BROCCOLI.GetOpenCLPlatformIDsError()));
		}
		if (BROCCOLI.GetOpenCLDeviceIDsError() != 0)
        {
	        printf("Get device IDs error is %s \n",BROCCOLI.GetOpenCLErrorMessage(BROCCOLI.GetOpenCLDeviceIDsError()));
		}
		if (BROCCOLI.GetOpenCLCreateContextError() != 0)
        {
	        printf("Create context error is %s \n",BROCCOLI.GetOpenCLErrorMessage(BROCCOLI.GetOpenCLCreateContextError()));
		}
		if (BROCCOLI.GetOpenCLContextInfoError() != 0)
		{
        	printf("Get create context info error is %s \n",BROCCOLI.GetOpenCLErrorMessage(BROCCOLI.GetOpenCLContextInfoError()));
        }
		if (BROCCOLI.GetOpenCLCreateCommandQueueError() != 0)
   		{
	        printf("Create command queue error is %s \n",BROCCOLI.GetOpenCLErrorMessage(BROCCOLI.GetOpenCLCreateCommandQueueError()));
		}
		if (BROCCOLI.GetOpenCLCreateProgramError() != 0)
		{
	        printf("Create program error is %s \n",BROCCOLI.GetOpenCLErrorMessage(BROCCOLI.GetOpenCLCreateProgramError()));
		}
		if (BROCCOLI.GetOpenCLBuildProgramError() != 0)
		{
	        printf("Build program error is %s \n",BROCCOLI.GetOpenCLErrorMessage(BROCCOLI.GetOpenCLBuildProgramError()));
		}
		if (BROCCOLI.GetOpenCLProgramBuildInfoError() != 0)
 		{
	        printf("Get program build info error is %s \n",BROCCOLI.GetOpenCLErrorMessage(BROCCOLI.GetOpenCLProgramBuildInfoError()));
		}
    
        // Print create kernel errors
        int* createKernelErrors = BROCCOLI.GetOpenCLCreateKernelErrors();
        for (int i = 0; i < BROCCOLI.GetNumberOfOpenCLKernels(); i++)
        {
            if (createKernelErrors[i] != 0)
            {
                printf("Create kernel error %i is %s \n",i,BROCCOLI.GetOpenCLErrorMessage(createKernelErrors[i]));
            }
        }                
        
        // Print build info to file    
        fp = fopen("buildinfo.txt","w");
        if (fp == NULL)
        {     
            printf("Could not open buildinfo.txt! \n");
        }
        if (BROCCOLI.GetOpenCLBuildInfoChar() != NULL)
        {
            int error = fputs(BROCCOLI.GetOpenCLBuildInfoChar(),fp);
            if (error == EOF)
            {
                printf("Could not write to buildinfo.txt! \n");
            }
        }
        fclose(fp);
        
        printf("OpenCL initialization failed, aborting! \nSee buildinfo.txt for output of OpenCL compilation!\n");      
        FreeAllMemory(allMemoryPointers,numberOfMemoryPointers);
        FreeAllNiftiImages(allNiftiImages,numberOfNiftiImages);
        return EXIT_FAILURE;
    }
    // Initialization OK
    else if (BROCCOLI.GetOpenCLInitiated() == 1)
    {
        // Set all necessary pointers and values
        BROCCOLI.SetInputT1Volume(h_T1_Volume);
        //BROCCOLI.SetInputMNIVolume(h_MNI_Volume);
        //BROCCOLI.SetInputMNIBrainVolume(h_MNI_Brain_Volume);
        //BROCCOLI.SetInputMNIBrainMask(h_MNI_Brain_Mask);
        
        BROCCOLI.SetInputMNIBrainVolume(h_MNI_Volume);
        
        BROCCOLI.SetT1Width(T1_DATA_W);
        BROCCOLI.SetT1Height(T1_DATA_H);
        BROCCOLI.SetT1Depth(T1_DATA_D);
        BROCCOLI.SetT1VoxelSizeX(T1_VOXEL_SIZE_X);
        BROCCOLI.SetT1VoxelSizeY(T1_VOXEL_SIZE_Y);
        BROCCOLI.SetT1VoxelSizeZ(T1_VOXEL_SIZE_Z);   
        
        BROCCOLI.SetMNIWidth(MNI_DATA_W);
        BROCCOLI.SetMNIHeight(MNI_DATA_H);
        BROCCOLI.SetMNIDepth(MNI_DATA_D);    
        BROCCOLI.SetMNIVoxelSizeX(MNI_VOXEL_SIZE_X);
        BROCCOLI.SetMNIVoxelSizeY(MNI_VOXEL_SIZE_Y);
        BROCCOLI.SetMNIVoxelSizeZ(MNI_VOXEL_SIZE_Z);                
        
        BROCCOLI.SetInterpolationMode(LINEAR);
        BROCCOLI.SetNumberOfIterationsForParametricImageRegistration(NUMBER_OF_ITERATIONS_FOR_LINEAR_IMAGE_REGISTRATION);
        BROCCOLI.SetNumberOfIterationsForNonParametricImageRegistration(NUMBER_OF_ITERATIONS_FOR_NONLINEAR_IMAGE_REGISTRATION);
        BROCCOLI.SetImageRegistrationFilterSize(IMAGE_REGISTRATION_FILTER_SIZE);    
        BROCCOLI.SetParametricImageRegistrationFilters(h_Quadrature_Filter_1_Parametric_Registration_Real, h_Quadrature_Filter_1_Parametric_Registration_Imag, h_Quadrature_Filter_2_Parametric_Registration_Real, h_Quadrature_Filter_2_Parametric_Registration_Imag, h_Quadrature_Filter_3_Parametric_Registration_Real, h_Quadrature_Filter_3_Parametric_Registration_Imag);
        BROCCOLI.SetNonParametricImageRegistrationFilters(h_Quadrature_Filter_1_NonParametric_Registration_Real, h_Quadrature_Filter_1_NonParametric_Registration_Imag, h_Quadrature_Filter_2_NonParametric_Registration_Real, h_Quadrature_Filter_2_NonParametric_Registration_Imag, h_Quadrature_Filter_3_NonParametric_Registration_Real, h_Quadrature_Filter_3_NonParametric_Registration_Imag, h_Quadrature_Filter_4_NonParametric_Registration_Real, h_Quadrature_Filter_4_NonParametric_Registration_Imag, h_Quadrature_Filter_5_NonParametric_Registration_Real, h_Quadrature_Filter_5_NonParametric_Registration_Imag, h_Quadrature_Filter_6_NonParametric_Registration_Real, h_Quadrature_Filter_6_NonParametric_Registration_Imag);    
        BROCCOLI.SetProjectionTensorMatrixFirstFilter(h_Projection_Tensor_1[0],h_Projection_Tensor_1[1],h_Projection_Tensor_1[2],h_Projection_Tensor_1[3],h_Projection_Tensor_1[4],h_Projection_Tensor_1[5]);
        BROCCOLI.SetProjectionTensorMatrixSecondFilter(h_Projection_Tensor_2[0],h_Projection_Tensor_2[1],h_Projection_Tensor_2[2],h_Projection_Tensor_2[3],h_Projection_Tensor_2[4],h_Projection_Tensor_2[5]);
        BROCCOLI.SetProjectionTensorMatrixThirdFilter(h_Projection_Tensor_3[0],h_Projection_Tensor_3[1],h_Projection_Tensor_3[2],h_Projection_Tensor_3[3],h_Projection_Tensor_3[4],h_Projection_Tensor_3[5]);
        BROCCOLI.SetProjectionTensorMatrixFourthFilter(h_Projection_Tensor_4[0],h_Projection_Tensor_4[1],h_Projection_Tensor_4[2],h_Projection_Tensor_4[3],h_Projection_Tensor_4[4],h_Projection_Tensor_4[5]);
        BROCCOLI.SetProjectionTensorMatrixFifthFilter(h_Projection_Tensor_5[0],h_Projection_Tensor_5[1],h_Projection_Tensor_5[2],h_Projection_Tensor_5[3],h_Projection_Tensor_5[4],h_Projection_Tensor_5[5]);
        BROCCOLI.SetProjectionTensorMatrixSixthFilter(h_Projection_Tensor_6[0],h_Projection_Tensor_6[1],h_Projection_Tensor_6[2],h_Projection_Tensor_6[3],h_Projection_Tensor_6[4],h_Projection_Tensor_6[5]);
        BROCCOLI.SetFilterDirections(h_Filter_Directions_X, h_Filter_Directions_Y, h_Filter_Directions_Z);
        BROCCOLI.SetCoarsestScaleT1MNI(COARSEST_SCALE);
        BROCCOLI.SetMMT1ZCUT(MM_T1_Z_CUT);   

		BROCCOLI.SetTsigma(TSIGMA);
		BROCCOLI.SetEsigma(ESIGMA);
		BROCCOLI.SetDsigma(DSIGMA);
        
        BROCCOLI.SetOutputInterpolatedT1Volume(h_Interpolated_T1_Volume);
        BROCCOLI.SetOutputAlignedT1Volume(h_Aligned_T1_Volume);
        BROCCOLI.SetOutputAlignedT1VolumeNonParametric(h_Aligned_T1_Volume_NonParametric);                
        BROCCOLI.SetOutputT1MNIRegistrationParameters(h_Registration_Parameters);
        
        if (WRITE_DISPLACEMENT_FIELD)
        {
            BROCCOLI.SetOutputDisplacementField(h_Displacement_Field_X,h_Displacement_Field_Y,h_Displacement_Field_Z);
        }        

        if (DEBUG)
        {
            BROCCOLI.SetDebug(true);
            
            //BROCCOLI.SetOutputSkullstrippedT1Volume(h_Skullstripped_T1_Volume);
            
            //BROCCOLI.SetOutputDownsampledVolume(h_Downsampled_Volume);
        
            BROCCOLI.SetOutputQuadratureFilterResponses(h_Quadrature_Filter_Response_1, h_Quadrature_Filter_Response_2, h_Quadrature_Filter_Response_3);
            //BROCCOLI.SetOutputQuadratureFilterResponses(h_Quadrature_Filter_Response_1_Real, h_Quadrature_Filter_Response_1_Imag, h_Quadrature_Filter_Response_2_Real, h_Quadrature_Filter_Response_2_Imag, h_Quadrature_Filter_Response_3_Real, h_Quadrature_Filter_Response_3_Imag);
            //BROCCOLI.SetOutputQuadratureFilterResponses(h_Quadrature_Filter_Response_1_Real, h_Quadrature_Filter_Response_1_Imag, h_Quadrature_Filter_Response_2_Real, h_Quadrature_Filter_Response_2_Imag, h_Quadrature_Filter_Response_3_Real, h_Quadrature_Filter_Response_3_Imag);
            //BROCCOLI.SetOutputQuadratureFilterResponses(h_Quadrature_Filter_Response_1, h_Quadrature_Filter_Response_2, h_Quadrature_Filter_Response_3, h_Quadrature_Filter_Response_4, h_Quadrature_Filter_Response_5, h_Quadrature_Filter_Response_6);
            //BROCCOLI.SetOutputTensorComponents(h_t11, h_t12, h_t13, h_t22, h_t23, h_t33);            
            BROCCOLI.SetOutputPhaseDifferences(h_Phase_Differences);
            BROCCOLI.SetOutputPhaseCertainties(h_Phase_Certainties);
            BROCCOLI.SetOutputPhaseGradients(h_Phase_Gradients);
            //BROCCOLI.SetOutputSliceSums(h_Slice_Sums);
            //BROCCOLI.SetOutputTopSlice(h_Top_Slice);
            //BROCCOLI.SetOutputAMatrix(h_A_Matrix);
            //BROCCOLI.SetOutputHVector(h_h_Vector);
        }
                            
        // Run the actual registration

		startTime = GetWallTime();
        BROCCOLI.PerformRegistrationTwoVolumesWrapper();     
		endTime = GetWallTime();

		if (VERBOS)
	 	{
			printf("\nIt took %f seconds to run the registration\n",(float)(endTime - startTime));
		}

        // Print create buffer errors
        int* createBufferErrors = BROCCOLI.GetOpenCLCreateBufferErrors();
        for (int i = 0; i < BROCCOLI.GetNumberOfOpenCLKernels(); i++)
        {
            if (createBufferErrors[i] != 0)
            {
                printf("Create buffer error %i is %s \n",i,BROCCOLI.GetOpenCLErrorMessage(createBufferErrors[i]));
            }
        }
        
        // Print run kernel errors
        int* runKernelErrors = BROCCOLI.GetOpenCLRunKernelErrors();
        for (int i = 0; i < BROCCOLI.GetNumberOfOpenCLKernels(); i++)
        {
            if (runKernelErrors[i] != 0)
            {
                printf("Run kernel error %i is %s \n",i,BROCCOLI.GetOpenCLErrorMessage(runKernelErrors[i]));
            }
        } 
    }        
           
	printf("\nAffine registration parameters\n");
	printf(" %f %f %f %f\n", h_Registration_Parameters[3]+1.0f,h_Registration_Parameters[4],h_Registration_Parameters[5],h_Registration_Parameters[0]);
	printf(" %f %f %f %f\n", h_Registration_Parameters[6],h_Registration_Parameters[7]+1.0f,h_Registration_Parameters[8],h_Registration_Parameters[1]);
	printf(" %f %f %f %f\n", h_Registration_Parameters[9],h_Registration_Parameters[10],h_Registration_Parameters[11]+1.0f,h_Registration_Parameters[2]);
	printf(" %f %f %f %f\n\n", 0.0f,0.0f,0.0f,1.0f);


    // Print registration parameters to file
    /*
    std::ofstream motion;
    motion.open("motion.1D");      
    if ( motion.good() )
    {
        //motion.setf(ios::scientific);
        motion.precision(6);
        for (int t = 0; t < DATA_T; t++)
        {
            //printf("X translation for timepoint %i is %f\n",t+1,h_Motion_Parameters[t + DATA_T]);
            //motion << h_Motion_Parameters[t + 0*DATA_T] << std::setw(2) << " " << h_Motion_Parameters[t + 1*DATA_T] << std::setw(2) << " " << h_Motion_Parameters[t + 2*DATA_T] << std::setw(2) << " " << h_Motion_Parameters[t + 3*DATA_T] << std::setw(2) << " " << h_Motion_Parameters[t + 4*DATA_T] << std::setw(2) << " " << h_Motion_Parameters[t + 5*DATA_T] << std::endl;
            motion << h_Motion_Parameters[t + 4*DATA_T] << std::setw(2) << " " << -h_Motion_Parameters[t + 3*DATA_T] << std::setw(2) << " " << h_Motion_Parameters[t + 5*DATA_T] << std::setw(2) << " " << -h_Motion_Parameters[t + 2*DATA_T] << std::setw(2) << " " << -h_Motion_Parameters[t + 0*DATA_T] << std::setw(2) << " " << -h_Motion_Parameters[t + 1*DATA_T] << std::endl;
        }
        motion.close();
    }
    else
    {
        printf("Could not open motion.1D for writing!\n");
    }
    */

    // Create new nifti image
	nifti_image *outputNifti = nifti_copy_nim_info(inputMNI);      
	allNiftiImages[numberOfNiftiImages] = outputNifti;
	numberOfNiftiImages++;    

    // Copy information from input data    	
	if (!CHANGE_OUTPUT_NAME)
	{
    	nifti_set_filenames(outputNifti, inputT1->fname, 0, 1);    
	}
	else
	{
		nifti_set_filenames(outputNifti, outputFilename, 0, 1);    
	}

    startTime = GetWallTime();

    // Write aligned data to file, as the size of the MNI volume  
	if (WRITE_INTERPOLATED)
	{          
    	WriteNifti(outputNifti,h_Interpolated_T1_Volume,"_interpolated",ADD_FILENAME,DONT_CHECK_EXISTING_FILE);
	}

    WriteNifti(outputNifti,h_Aligned_T1_Volume,"_aligned_linear",ADD_FILENAME,DONT_CHECK_EXISTING_FILE);
	
	if (NUMBER_OF_ITERATIONS_FOR_NONLINEAR_IMAGE_REGISTRATION > 0)
	{
   		WriteNifti(outputNifti,h_Aligned_T1_Volume_NonParametric,"_aligned_nonlinear",ADD_FILENAME,DONT_CHECK_EXISTING_FILE);
	}
              	
	if (WRITE_DISPLACEMENT_FIELD)
    {
        WriteNifti(outputNifti,h_Displacement_Field_X,"_displacement_x",ADD_FILENAME,DONT_CHECK_EXISTING_FILE);
        WriteNifti(outputNifti,h_Displacement_Field_Y,"_displacement_y",ADD_FILENAME,DONT_CHECK_EXISTING_FILE);
        WriteNifti(outputNifti,h_Displacement_Field_Z,"_displacement_z",ADD_FILENAME,DONT_CHECK_EXISTING_FILE);
    }
    
    if (DEBUG)
    {
        WriteNifti(outputNifti,h_Phase_Gradients,"_phase_gradients",ADD_FILENAME,DONT_CHECK_EXISTING_FILE);
        WriteNifti(outputNifti,h_Phase_Certainties,"_phase_certainties",ADD_FILENAME,DONT_CHECK_EXISTING_FILE);
        WriteNifti(outputNifti,h_Phase_Differences,"_phase_differences",ADD_FILENAME,DONT_CHECK_EXISTING_FILE);
        
        ConvertFloat2ToFloats(h_Quadrature_Filter_Response_1_Real, h_Quadrature_Filter_Response_1_Imag, h_Quadrature_Filter_Response_1, MNI_DATA_W, MNI_DATA_H, MNI_DATA_D);
        ConvertFloat2ToFloats(h_Quadrature_Filter_Response_2_Real, h_Quadrature_Filter_Response_2_Imag, h_Quadrature_Filter_Response_2, MNI_DATA_W, MNI_DATA_H, MNI_DATA_D);
        ConvertFloat2ToFloats(h_Quadrature_Filter_Response_3_Real, h_Quadrature_Filter_Response_3_Imag, h_Quadrature_Filter_Response_3, MNI_DATA_W, MNI_DATA_H, MNI_DATA_D);
        
        WriteNifti(outputNifti,h_Quadrature_Filter_Response_1_Real,"_quadrature_filter_response_1_real",ADD_FILENAME,DONT_CHECK_EXISTING_FILE);
        WriteNifti(outputNifti,h_Quadrature_Filter_Response_1_Imag,"_quadrature_filter_response_1_imag",ADD_FILENAME,DONT_CHECK_EXISTING_FILE);
        WriteNifti(outputNifti,h_Quadrature_Filter_Response_2_Real,"_quadrature_filter_response_2_real",ADD_FILENAME,DONT_CHECK_EXISTING_FILE);
        WriteNifti(outputNifti,h_Quadrature_Filter_Response_2_Imag,"_quadrature_filter_response_2_imag",ADD_FILENAME,DONT_CHECK_EXISTING_FILE);
        WriteNifti(outputNifti,h_Quadrature_Filter_Response_3_Real,"_quadrature_filter_response_3_real",ADD_FILENAME,DONT_CHECK_EXISTING_FILE);
        WriteNifti(outputNifti,h_Quadrature_Filter_Response_3_Imag,"_quadrature_filter_response_3_imag",ADD_FILENAME,DONT_CHECK_EXISTING_FILE);        
    }
    
	endTime = GetWallTime();

	if (VERBOS)
 	{
		printf("It took %f seconds to write the nifti files\n",(float)(endTime - startTime));
	}

    // Free all memory
    FreeAllMemory(allMemoryPointers,numberOfMemoryPointers);
    FreeAllNiftiImages(allNiftiImages,numberOfNiftiImages);
        
    return EXIT_SUCCESS;
}

/*
fid = fopen('filter6_real_nonparametric_registration.bin','w')
a = f6_nonparametric_registration;
a = real(a);
a(:,:,1) = a(:,:,1)';
a(:,:,2) = a(:,:,2)';
a(:,:,3) = a(:,:,3)';
a(:,:,4) = a(:,:,4)';
a(:,:,5) = a(:,:,5)';
a(:,:,6) = a(:,:,6)';
a(:,:,7) = a(:,:,7)';        
count = fwrite(fid,single(a(:)),'float')
fclose(fid)
 
 fid = fopen('projection_tensor6.bin','w')
 count = fwrite(fid,single(m6),'float')
 fclose(fid)
 
 
*/

