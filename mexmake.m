addpath('bin')
output_path = fullfile(pwd,'bin');

try
	
	if(DebugMode)
		fprintf('-Compiling in debug mode.\n')
		mexflags = {'-g', '-v', '-outdir', output_path};
	else
		mexflags = {'-O', '-silent', '-outdir', output_path};
	end
	
	if(AutomaticInitialization)
		fprintf('-Automatic initialization is enabled.\n')
		copyfile(fullfile(pwd,'src','entry','MatShareAI.m'), fullfile(pwd,'bin','MatShare.m'));
		mexflags = [mexflags {'-DMSH_AUTO_INIT'}];
	else
		copyfile(fullfile(pwd,'src','entry','MatShareMI.m'), fullfile(pwd,'bin','MatShare.m'));
		fprintf('-Automatic initialization is disabled.\n')
	end
	
	if(ThreadSafety)
		fprintf('-Thread safety is enabled.\n')
		mexflags = [mexflags {'-DMSH_THREAD_SAFE'}];
	else
		fprintf('-Thread safety is disabled.\n')
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
	clear mexflags sources output_path comp endi maxsz doINSTALL i ThreadSafety AutomaticInitialization DebugMode
	
	
catch ME
	
	rethrow(ME)
	
end