addpath('bin')
output_path = [pwd '/bin'];

try
	
	sources = {'src/matshare_.c',...
			};
	
	mexflags = {'-g', '-v', '-outdir', output_path};
	
	if(ispc)
		% stub
	else
		mexflags = [mexflags,{'-lrt'}];
	end
	
	mex(mexflags{:}, sources{:})
	
	
catch ME
	
	rethrow(ME)
	
end