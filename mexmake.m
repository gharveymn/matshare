addpath('bin')
output_path = [pwd '/bin'];

try
	
	sources = {'src/matshare_.c',...
			'src/utils.c'};
	
	mexflags = {'-g', '-v', '-outdir', output_path};
	
	if(ispc)
		mexflags = [mexflags,{'-DMATLAB_WINDOWS'}];
	else
		mexflags = [mexflags,{'-DMATLAB_UNIX','-lrt'}];
	end
	
	mex(mexflags{:}, sources{:})
	
	
catch ME
	
	rethrow(ME)
	
end