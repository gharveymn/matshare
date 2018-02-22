addpath('bin')
output_path = [pwd '/bin'];

try
	
	sources = {'src/matshare_.cpp',...
			};
	
	mexflags = {'-g', '-v', 'CFLAGS="$CFLAGS -std=c99"', '-outdir', output_path};
	
	if(strcmp(mex.getCompilerConfigurations('C','Selected').ShortName, 'mingw64'))
		
		sources = [sources,...
			{'src/extlib/mman-win32/mman.c'}
			];
		
	elseif(strncmpi(mex.getCompilerConfigurations('C','Selected').ShortName, 'MSVC',4))
		
		sources = [sources,...
			{'src/extlib/mman-win32/mman.c'}
			];
		
	elseif(strcmp(mex.getCompilerConfigurations('C','Selected').ShortName, 'gcc'))
		
		mexflags = [mexflags,{'-lrt'}];
		
	end
	
	%include boost
% 	if ispc
% 		BOOST_dir = 'C:\local\boost\';
% 		BOOST_lib = [BOOST_dir 'stage\lib\'];
% 		%BOOST_lib = [BOOST_dir 'lib64-msvc-14.0\'];
% 	else
% 		% on Ubuntu: sudo aptitude install libboost-all-dev libboost-doc
% 		BOOST_dir = '/usr/include/';
% 	end
% 
% 	if ~exist(fullfile(BOOST_dir,'boost','interprocess','windows_shared_memory.hpp'), 'file')
% 		error('%s\n%s\n', ...
% 			'Could not find the BOOST library. Please edit this file to include the BOOST location', ...
% 			'<BOOST_dir>\boost\interprocess\windows_shared_memory.hpp must exist');
% 	end
% 	mexflags = [mexflags {['-L' BOOST_lib], ['-I' BOOST_dir]}];
	
	mex(mexflags{:}, sources{:})
	
	
catch ME
	
	rethrow(ME)
	
end