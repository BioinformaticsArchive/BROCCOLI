%  	 BROCCOLI: An open source multi-platform software for parallel analysis of fMRI data on many core CPUs and GPUS
%    Copyright (C) <2013>  Anders Eklund, andek034@gmail.com
%
%    This program is free software: you can redistribute it and/or modify
%    it under the terms of the GNU General Public License as published by
%    the Free Software Foundation, either version 3 of the License, or
%    (at your option) any later version.
%
%    This program is distributed in the hope that it will be useful,
%    but WITHOUT ANY WARRANTY; without even the implied warranty of
%    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
%    GNU General Public License for more details.
%
%    You should have received a copy of the GNU General Public License
%    along with this program.  If not, see <http://www.gnu.org/licenses/>.
%-----------------------------------------------------------------------------

%---------------------------------------------------------------------------------------------------------------------
% README
% If you run this code in Windows, your graphics driver might stop working
% for large volumes / large filter sizes. This is not a bug in my code but is due to the
% fact that the Nvidia driver thinks that something is wrong if the GPU
% takes more than 2 seconds to complete a task. This link solved my problem
% https://forums.geforce.com/default/topic/503962/tdr-fix-here-for-nvidia-driver-crashing-randomly-in-firefox/
%---------------------------------------------------------------------------------------------------------------------

clear all
clc
close all

ismiha = 0

if ismiha
    addpath('/home/miha/Delo/BROCCOLI/nifti')
    basepath = '/home/miha/Programiranje/BROCCOLI/test_data/fcon1000/classic/';
    basepath_BROCCOLI = '/data/miha/BROCCOLI_test_data/BROCCOLI/normalization';        
    
    mex RegisterEPIT1.cpp -lOpenCL -lBROCCOLI_LIB -I/opt/cuda/include/ -I/opt/cuda/include/CL -L/usr/lib -I/home/miha/Programiranje/BROCCOLI/code/BROCCOLI_LIB -L/home/miha/Programiranje/BROCCOLI/code/BROCCOLI_LIB    -I/home/miha/Programiranje/BROCCOLI/code/BROCCOLI_LIB/Eigen
    
    opencl_platform = 0;  % 0 Intel, 1 AMD, 2 Nvidia
    opencl_device = 0;
elseif ispc
    addpath('D:\nifti_matlab')
    addpath('D:\BROCCOLI_test_data')
    %basepath = 'D:\BROCCOLI_test_data\';
    basepath = 'D:\';
    
    %mex -g RegisterEPIT1.cpp -lOpenCL -lBROCCOLI_LIB -IC:/Program' Files'/NVIDIA' GPU Computing Toolkit'/CUDA/v5.0/include -IC:/Program' Files'/NVIDIA' GPU Computing Toolkit'/CUDA/v5.0/include/CL -LC:/Program' Files'/NVIDIA' GPU Computing Toolkit'/CUDA/v5.0/lib/x64 -LC:/users/wande/Documents/Visual' Studio 2010'/Projects/BROCCOLI_LIB/x64/Debug/ -IC:/users/wande/Documents/Visual' Studio 2010'/Projects/BROCCOLI_LIB/BROCCOLI_LIB -IC:\Users\wande\Documents\Visual' Studio 2010'\Projects\BROCCOLI_LIB\nifticlib-2.0.0\niftilib -IC:\Users\wande\Documents\Visual' Studio 2010'\Projects\BROCCOLI_LIB\nifticlib-2.0.0\znzlib -IC:\Users\wande\Documents\Visual' Studio 2010'\Projects\BROCCOLI_LIB\Eigen
    mex RegisterEPIT1.cpp -lOpenCL -lBROCCOLI_LIB -IC:/Program' Files'/NVIDIA' GPU Computing Toolkit'/CUDA/v5.0/include -IC:/Program' Files'/NVIDIA' GPU Computing Toolkit'/CUDA/v5.0/include/CL -LC:/Program' Files'/NVIDIA' GPU Computing Toolkit'/CUDA/v5.0/lib/x64 -LC:/users/wande/Documents/Visual' Studio 2010'/Projects/BROCCOLI_LIB/x64/Release/ -IC:/users/wande/Documents/Visual' Studio 2010'/Projects/BROCCOLI_LIB/BROCCOLI_LIB -IC:\Users\wande\Documents\Visual' Studio 2010'\Projects\BROCCOLI_LIB\nifticlib-2.0.0\niftilib -IC:\Users\wande\Documents\Visual' Studio 2010'\Projects\BROCCOLI_LIB\nifticlib-2.0.0\znzlib -IC:\Users\wande\Documents\Visual' Studio 2010'\Projects\BROCCOLI_LIB\Eigen
    
    opencl_platform = 2; % 0 Nvidia, 1 Intel, 2 AMD
    opencl_device = 0;
elseif isunix
    addpath('/home/andek/Research_projects/nifti_matlab')
    basepath = '/data/andek/BROCCOLI_test_data/';
    basepath_BROCCOLI = '/data/andek/BROCCOLI_test_data/BROCCOLI/normalization';
    
    %mex -g RegisterEPIT1.cpp -lOpenCL -lBROCCOLI_LIB -I/usr/local/cuda-5.0/include/ -I/usr/local/cuda-5.0/include/CL -L/usr/lib -I/home/andek/Research_projects/BROCCOLI/BROCCOLI/code/BROCCOLI_LIB/ -L/home/andek/cuda-workspace/BROCCOLI_LIB/Debug -I/home/andek/Research_projects/BROCCOLI/BROCCOLI/code/BROCCOLI_LIB/Eigen/
    mex RegisterEPIT1.cpp -lOpenCL -lBROCCOLI_LIB -I/usr/local/cuda-5.0/include/ -I/usr/local/cuda-5.0/include/CL -L/usr/lib -I/home/andek/Research_projects/BROCCOLI/BROCCOLI/code/BROCCOLI_LIB/ -L/home/andek/cuda-workspace/BROCCOLI_LIB/Release -I/home/andek/Research_projects/BROCCOLI/BROCCOLI/code/BROCCOLI_LIB/Eigen/
    
    opencl_platform = 2; % 0 Intel, 1 AMD, 2 Nvidia
    opencl_device = 0;
end



%------------------------------------

show_results = 1;                   % Show resulting registration or not
save_warped_volume_matlab = 0;      % Save warped volume as Matlab file or not
save_warped_volume_nifti = 0;       % Save warped volume as nifti file or not

%------------------------------------

%study = 'Baltimore';
study = 'Cambridge'; N = 10;

skullstripped = 1;
voxel_size = 1;

number_of_iterations_for_parametric_image_registration = 20;
coarsest_scale = 8/voxel_size;
MM_EPI_Z_CUT = 30;

%% Only used in Octave for compatibility with Matlab
if exist('do_braindead_shortcircuit_evaluation', 'builtin')
  do_braindead_shortcircuit_evaluation(1);
  warning('off', 'Octave:possible-matlab-short-circuit-operator');
end

% Load quadrature filters
load filters_for_parametric_registration.mat
load filters_for_nonparametric_registration.mat

dirs = dir([basepath study]);

normalization_times = zeros(N,1);

% Loop over subjects
for s = 1:N
    
    subject = dirs(s+2).name % Skip . and .. 'folders'
    
    s
    
    %close all
    
    if ( (strcmp(study,'Beijing')) || (strcmp(study,'Cambridge')) || (strcmp(study,'ICBM')) || (strcmp(study,'Oulu'))  || (strcmp(study,'Baltimore')) )
        T1_nii = load_nii([basepath study '/' subject '/anat/mprage_skullstripped.nii']);
    elseif ( strcmp(study,'OpenfMRI'))
        T1_nii = load_nii([basepath study '/' substudy '/highres' subject '.nii.gz']);
    end
    T1 = double(T1_nii.img);
    
    [T1_sy T1_sx T1_sz] = size(T1);
    [T1_sy T1_sx T1_sz]
    
    if (strcmp(study,'Beijing'))
        T1_voxel_size_x = T1_nii.hdr.dime.pixdim(1);
        T1_voxel_size_y = T1_nii.hdr.dime.pixdim(2);
        T1_voxel_size_z = T1_nii.hdr.dime.pixdim(3);
    elseif (strcmp(study,'OpenfMRI'))
        T1_voxel_size_x = T1_nii.hdr.dime.pixdim(3);
        T1_voxel_size_y = T1_nii.hdr.dime.pixdim(2);
        T1_voxel_size_z = T1_nii.hdr.dime.pixdim(4);
    else
        T1_voxel_size_x = T1_nii.hdr.dime.pixdim(2);
        T1_voxel_size_y = T1_nii.hdr.dime.pixdim(3);
        T1_voxel_size_z = T1_nii.hdr.dime.pixdim(4);
    end
    
    
    if ( (strcmp(study,'Beijing')) || (strcmp(study,'Cambridge')) || (strcmp(study,'ICBM')) || (strcmp(study,'Oulu')) || (strcmp(study,'Baltimore')) )
        EPI_nii = load_nii([basepath study '/' subject '/func/rest.nii']);
    elseif ( strcmp(study,'OpenfMRI'))
        EPI_nii = load_nii([basepath study '\' substudy '/bold' num2str(subject) '.nii.gz']);
    end
    
    fMRI_volumes = double(EPI_nii.img);
    EPI = fMRI_volumes(:,:,:,1);
    
    [EPI_sy EPI_sx EPI_sz] = size(EPI);
    [EPI_sy EPI_sx EPI_sz]
        
    EPI_voxel_size_x = EPI_nii.hdr.dime.pixdim(2)
    EPI_voxel_size_y = EPI_nii.hdr.dime.pixdim(3)
    EPI_voxel_size_z = EPI_nii.hdr.dime.pixdim(4)
    
    % Run registration
    tic
    [aligned_EPI_opencl, interpolated_EPI_opencl, registration_parameters_opencl, ...
        quadrature_filter_response_1_opencl, quadrature_filter_response_2_opencl, quadrature_filter_response_3_opencl,  ...
        phase_differences_x_opencl, phase_certainties_x_opencl, phase_gradients_x_opencl] = ...
        RegisterEPIT1(EPI,T1,EPI_voxel_size_x,EPI_voxel_size_y,EPI_voxel_size_z,T1_voxel_size_x,T1_voxel_size_y,T1_voxel_size_z, ...
        f1_parametric_registration,f2_parametric_registration,f3_parametric_registration, ...                
        f1_nonparametric_registration,f2_nonparametric_registration,f3_nonparametric_registration,f4_nonparametric_registration,f5_nonparametric_registration,f6_nonparametric_registration, ...                
        m1, m2, m3, m4, m5, m6, ...
        number_of_iterations_for_parametric_image_registration,coarsest_scale,MM_EPI_Z_CUT,opencl_platform, opencl_device);
    toc
    
    registration_parameters_opencl
            
    % Show some nice results
    if show_results == 1
        slice = round(0.55*T1_sy);
        figure(1); imagesc(flipud(squeeze(interpolated_EPI_opencl(slice,:,:))')); colormap gray        
        figure(2); imagesc(flipud(squeeze(aligned_EPI_opencl(slice,:,:))')); colormap gray
        figure(3); imagesc(flipud(squeeze(T1(slice,:,:))')); colormap gray
                
        slice = round(0.62*T1_sz);
        figure(5); imagesc(squeeze(interpolated_EPI_opencl(:,:,slice))); colormap gray        
        figure(6); imagesc(squeeze(aligned_EPI_opencl(:,:,slice))); colormap gray
        figure(7); imagesc(squeeze((T1(:,:,slice)))); colormap gray        
    end
    
    % Save registered volume as a Matlab file
    if save_warped_volume_matlab == 1
        filename = [basepath_BROCCOLI '/BROCCOLI_aligned_EPI_subject' num2str(s) '.mat'];
        save(filename,'aligned_EPI_opencl')
    end
    
    % Save normalized volume as a nifti file
    if save_warped_volume_nifti == 1
        new_file.hdr = T1_nii.hdr;
        new_file.hdr.dime.datatype = 16;
        new_file.hdr.dime.bitpix = 16;
        new_file.img = single(aligned_EPI_opencl);
        
        filename = [basepath_BROCCOLI '/BROCCOLI_aligned_EPI_subject' num2str(s) '.nii'];
        
        save_nii(new_file,filename);
    end
    
    %pause(1)
    pause
    
end



