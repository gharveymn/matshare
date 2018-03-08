addpath('bin')
output_path = [pwd '/bin'];

try
	
	if(exist('doINSTALL','var'))
		mexflags = {'-O', '-silent', '-outdir', output_path};
	else
		mexflags = {'-g', '-v', '-outdir', output_path};
	end
	
	[comp,maxsz,endi] = computer;
	
	sources = {
			'matshare_.c',...
			'matlabutils.c',...
			'mshutils.c',...
			'init.c'
			};
		
	for i = 1:numel(sources)
		sources{i} = fullfile(pwd,'src',sources{i});
	end
	
	if(ispc)
		
		mexflags = [mexflags,{'-DMATLAB_WINDOWS'}];
		
	else
		
		warning('off','MATLAB:mex:GccVersion_link');
		mexflags = [mexflags,{'-DMATLAB_UNIX','-lrt'}];
		
	end
	
	if(maxsz > 2^31-1)
		mexflags = [mexflags {'-largeArrayDims'}];
	else
		mexflags = [mexflags {'-compatibleArrayDims', '-DMATLAB_32BIT'}];
	end
	
	fprintf('-Compiling matshare...')
	mex(mexflags{:} , sources{:})
	fprintf(' successful.\n%s\n',['-The function is located in ' fullfile(pwd,'bin') '.'])
	
	addpath('bin');
	clear mexflags sources output_path comp endi maxsz doINSTALL i
	
	
catch ME
	
	rethrow(ME)
	
end