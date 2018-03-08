addpath('bin')
output_path = [pwd '/bin'];

try
	
	sources = {
			'src/matshare_.c',...
			'src/matlabutils.c',...
			'src/mshutils.c',...
			'src/init.c'
			};
	
	mexflags = {'-g', '-v', 'CXXFLAGS="$CXXFLAGS -Wall"',  '-outdir', output_path};
	
	if(ispc)
		mexflags = [mexflags,{'-DMATLAB_WINDOWS'}];
	else
		mexflags = [mexflags,{'-DMATLAB_UNIX','-lrt'}];
	end
	
	mex(mexflags{:}, sources{:})
	
	
catch ME
	
	rethrow(ME)
	
end