addpath('bin')
output_path = fullfile(pwd,'bin');

try
	
	if(DebugMode)
		fprintf('-Compiling in debug mode.\n')
		mexflags = {'-g', '-O', '-v', '-outdir', output_path};
	else
		mexflags = {'-O', '-silent', '-outdir', output_path};
	end
	
	if(ThreadSafety)
		fprintf('-Thread safety is enabled.\n')
		mexflags = [mexflags {'-DMSH_THREAD_SAFE'}];
	else
		fprintf('-Thread safety is disabled.\n')
	end
	
	if(CopyOnWrite)
		fprintf('-Compiling in copy-on-write mode.\n')
		mexflags = [mexflags {'-DMSH_SHARETYPE_COPY'}];
	else
		fprintf('-Compiling in overwrite mode.\n')
	end
	
	[comp,maxsz,endi] = computer;
	
	sources = {
			'matshare_.c',...
			'matlabutils.c',...
			'mshutils.c',...
			'mshinit.c',...
			'mshlists.c',...
			'headers/opaque/mshheader.c',...
			'headers/opaque/mshexterntypes.c'
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
		if(verLessThan('matlab','9.4'))
			mexflags = [mexflags {'-largeArrayDims'}];
		else
			warning(['Compiling in compatibility mode; the R2018a MEX api does not support certain functions '...
				'which are integral to this function'])
			mexflags = [mexflags, {'-R2017b'}];
		end
	else
		mexflags = [mexflags {'-compatibleArrayDims', '-DMATLAB_32BIT'}];
	end
	
	fprintf('-Compiling matshare...')
	mexflags = [mexflags {'CFLAGS="$CFLAGS -std=c89 /Wall -Wextra -Wshadow -Wpedantic"'}];
	mex(mexflags{:} , sources{:})
	fprintf(' successful.\n%s\n',['-The function is located in ' fullfile(pwd,'bin') '.'])
	
	addpath('bin');
	clear mexflags sources output_path comp endi maxsz doINSTALL i ...
	ThreadSafety AutomaticInitialization DebugMode CopyOnWrite
	
	
catch ME
	
	rethrow(ME)
	
end