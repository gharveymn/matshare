
output_path = fullfile(fileparts(which(mfilename)),'bin');

addpath(output_path)

try
	
	if(mshDebugMode)
		mexflags = {'-g', '-O', '-v', '-outdir', output_path};
	else
		mexflags = {'-O', '-v', '-outdir', output_path};
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
		'headers/opaque/mshtable.c',...
		'headers/opaque/mshvariablenode.c',...
		'headers/opaque/mshsegmentnode.c'
		};
	
	for i = 1:numel(sources)
		sources{i} = fullfile(pwd,'src',sources{i});
	end
	
	if(ispc)
		userconfigfolder = getenv('LOCALAPPDATA');
		if(isempty(userconfigfolder))
			userconfigfolder = getenv('APPDATA');
		end
		mexflags = [mexflags, {'-DMSH_WIN'}];
	else
		userconfigfolder = fullfile(getenv('HOME'), '.config');
		mexflags = [mexflags, {'-DMSH_UNIX'}];
		if(~ismac)
			mexflags = [mexflags, {'-lrt'}];
		end
	end
	mshconfigfolder = fullfile(userconfigfolder, 'matshare');
	
	if(strcmp(mshThreadSafety, 'on'))
		fprintf('-Thread safety is enabled.\n')
		mexflags = [mexflags {'-DMSH_DEFAULT_THREAD_SAFETY=TRUE'}];
	elseif(strcmp(mshThreadSafety, 'off'))
		fprintf('-Thread safety is disabled.\n')
		mexflags = [mexflags {'-DMSH_DEFAULT_THREAD_SAFETY=FALSE'}];
	else
		error('Invalid value for compilation parameter mshThreadSafety');
	end
	
	mexflags = [mexflags {['-DMSH_DEFAULT_MAX_SHARED_SEGMENTS=' mshMaxVariables]}];
	
	if(maxsz > 2^31-1)
		% R2018a
		if(verLessThan('matlab','9.4'))
			mexflags = [mexflags {'-largeArrayDims'}];
		else
			warning(['Compiling in compatibility mode; the R2018a MEX api does not support certain functions '...
				'which are integral to this function'])
			mexflags = [mexflags, {'-R2017b'}];
		end
		mexflags = [mexflags, {'-DMSH_BITNESS=64', ['-DMSH_DEFAULT_MAX_SHARED_SIZE=' mshMaxSize64]}];
		
		% delete the previous config file
		mshconfigpath = fullfile(mshconfigfolder, 'mshconfig');		
	else
		mexflags = [mexflags {'-compatibleArrayDims', '-DMSH_BITNESS=32', ['-DMSH_DEFAULT_MAX_SHARED_SIZE=' mshMaxSize32]}];
		mshconfigpath = fullfile(mshconfigfolder, 'mshconfig32');
	end
	
	if(exist(mshconfigpath, 'file'))
		delete(mshconfigpath);
	end
	
	if(strcmp(mshGarbageCollection, 'on'))
		fprintf('-Garbage collection is enabled.\n')
		mexflags = [mexflags {'-DMSH_DEFAULT_SHARED_GC=TRUE'}];
	elseif(strcmp(mshGarbageCollection, 'off'))
		fprintf('-Garbage collection is disabled.\n')
		mexflags = [mexflags {'-DMSH_DEFAULT_SHARED_GC=FALSE'}];
	else
		error('Invalid value for compilation parameter mshGarbageCollection');
	end
	
	mexflags = [mexflags {['-DMSH_DEFAULT_SECURITY=' mshSecurity]}];
	
	% R2011b
	if(~verLessThan('matlab', '7.13'))
		mexflags = [mexflags {'-DMSH_AVX_SUPPORT'}];
	end
	
	%mexflags = [mexflags {'-DMSH_DEBUG_PERF'}];
	
	fprintf('-Compiling matshare...')
	%mexflags = [mexflags {'COMPFLAGS="$COMPFLAGS /Wall"'}];
	mexflags = [mexflags {'CFLAGS="$CFLAGS -Wall"'}];
	mex(mexflags{:} , sources{:})
	fprintf(' successful.\n')
	if(mshDebugMode)
		fprintf('-Compiled for DEBUGGING.\n');
	else
		fprintf('-Compiled for RELEASE.\n');
	end
	fprintf('%s\n', ['-The function is located in ' output_path '.']);
	
catch ME
	
	rethrow(ME)
	
end