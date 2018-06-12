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
		mexflags = [mexflags {'-DMSH_THREAD_SAFETY=TRUE'}];
	else
		fprintf('-Thread safety is disabled.\n')
		mexflags = [mexflags {'-DMSH_THREAD_SAFETY=FALSE'}];
	end
	
	if(CopyOnWrite)
		fprintf('-Compiling in copy-on-write mode.\n')
		mexflags = [mexflags {'-DMSH_SHARETYPE=msh_SHARETYPE_COPY'}];
	else
		fprintf('-Compiling in overwrite mode.\n')
		mexflags = [mexflags {'-DMSH_SHARETYPE=msh_SHARETYPE_OVERWRITE'}];
	end
	
	[comp,maxsz,endi] = computer;	
	
	sources = {
				'matshare_.c',...
				'mlerrorutils.c',...
				'mshutils.c',...
				'mshinit.c',...
				'mshvariables.c',...
				'mshsegments.c',...
				'headers/opaque/mshheader.c',...
				'headers/opaque/mshexterntypes.c',...
				'headers/opaque/mshtable.c'
			};
		
	for i = 1:numel(sources)
		sources{i} = fullfile(pwd,'src',sources{i});
	end
	
	if(ispc)
		mexflags = [mexflags, {'-DMATLAB_WINDOWS'}];
    else
        mexflags = [mexflags, {'-DMATLAB_UNIX'}];
        if(~ismac)
            mexflags = [mexflags, {'-lrt'}];
        end
	end
	
	if(maxsz > 2^31-1)
		% R2018a
		if(verLessThan('matlab','9.4'))
			mexflags = [mexflags {'-largeArrayDims'}];
		else
			warning(['Compiling in compatibility mode; the R2018a MEX api does not support certain functions '...
				'which are integral to this function'])
			mexflags = [mexflags, {'-R2017b'}];
		end
		mexflags = [mexflags, {'-DMSH_BITNESS=64', '-DMSH_MAX_SHARED_SIZE=0xFFFFFFFF'}];
	else
		mexflags = [mexflags {'-compatibleArrayDims', '-DMSH_BITNESS=32', '-DMSH_MAX_SHARED_SIZE=0xFFFFFFFF'}];
	end
	
	% R2011b
	if(~verLessThan('matlab', '7.13'))
		mexflags = [mexflags {'-DMSH_AVX_SUPPORT'}];
    end
    
    mexflags = [mexflags {'-DMSH_MAX_SHARED_SEGMENTS=512'}];
    %mexflags = [mexflags {'-DMSH_DEBUG_PERF'}];
	
	fprintf('-Compiling matshare...')
	%mexflags = [mexflags {'CFLAGS="$CFLAGS -std=c89 -Wall"'}];
	mex(mexflags{:} , sources{:})
	fprintf(' successful.\n%s\n',['-The function is located in ' fullfile(pwd,'bin') '.'])
	
	addpath('bin');
	clear mexflags sources output_path comp endi maxsz doINSTALL i ...
	ThreadSafety AutomaticInitialization DebugMode CopyOnWrite
	
	
catch ME
	
	rethrow(ME)
	
end