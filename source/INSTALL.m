function INSTALL
%% INSTALL  Run this file to compile <strong>matshare</strong>.
%    The output is written to '../+matshare/private'.
%
%    Thanks for downloading!

	opts = OPTIONS;
	
	thisfolder = fileparts(which(mfilename));
	
	output_path = fullfile(thisfolder,'..', '+matshare', 'private');

	mexflags = {'-O', '-v', '-outdir', output_path, ...
		['-I' fullfile(thisfolder, 'src', 'headers')]};

	if(opts.mshDebugMode)
		mexflags = [mexflags {'-g'}];
	end
	
	% check if ints are two's complement, and right shifts of signed ints 
	% preserve the sign bit
	tstdir = fullfile(thisfolder,'tests');
	mex(fullfile(tstdir,'intdefcheck.c'), '-outdir', tstdir);
	addpath(fullfile(thisfolder, 'tests'));
	
	if(~intdefcheck())
		mexflags = [mexflags {'-DMSH_NO_VAROPS'}];
		warning(['Your compiler must represent signed integers in ' ...
			  'two''s complement, and must preserve the sign bit ' ...
			  'for right shifts. Please use a different compiler']);
	end
	
	[comp,maxsz,endi] = computer;

	sources = {
		'matshare_.c',...
		'mlerrorutils.c',...
		'mshutils.c',...
		'mshinit.c',...
		'mshvariables.c',...
		'mshsegments.c',...
		'mshlockfree.c',...
		'mshtable.c',...
		'mshvarops.c',...
		'headers/opaque/mshheader.c',...
		'headers/opaque/mshexterntypes.c',...
		'headers/opaque/mshvariablenode.c',...
		'headers/opaque/mshsegmentnode.c'
		};

	for i = 1:numel(sources)
		sources{i} = fullfile(fileparts(which(mfilename)),'src',sources{i});
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

	mexflags = [mexflags {['-DMSH_DEFAULT_MAX_SHARED_SEGMENTS=' ...
		opts.mshMaxVariables]}];

	if(maxsz > 2^31-1)
		% R2018a
		if(verLessThan('matlab','9.4'))
			mexflags = [mexflags {'-largeArrayDims'}];
		else
			warning(['Compiling in compatibility mode; the R2018a' ...
				'MEX API may not support certain functions '...
				'which are integral to this function'])
			mexflags = [mexflags, {'-R2017b'}];
		end
		mexflags = [mexflags, {'-DMSH_BITNESS=64', ...
			['-DMSH_DEFAULT_MAX_SHARED_SIZE=' opts.mshMaxSize64]}];

		% delete the previous config file
		mshconfigpath = fullfile(mshconfigfolder, 'mshconfig');
	else
		mexflags = [mexflags {'-compatibleArrayDims', ...
			'-DMSH_BITNESS=32', ...
			['-DMSH_DEFAULT_MAX_SHARED_SIZE=' opts.mshMaxSize32]}];
		mshconfigpath = fullfile(mshconfigfolder, 'mshconfig32');
	end

% 	if(exist(mshconfigpath, 'file'))
% 		delete(mshconfigpath);
% 	end

	if(strcmp(opts.mshGarbageCollection, 'on'))
		fprintf('-Garbage collection is enabled.\n')
		mexflags = [mexflags {'-DMSH_DEFAULT_SHARED_GC=TRUE'}];
	elseif(strcmp(opts.mshGarbageCollection, 'off'))
		fprintf('-Garbage collection is disabled.\n')
		mexflags = [mexflags {'-DMSH_DEFAULT_SHARED_GC=FALSE'}];
	else
		error(['Invalid value for compilation parameter' ...
			'mshGarbageCollection']);
	end

	mexflags = [mexflags {['-DMSH_DEFAULT_SECURITY=' opts.mshSecurity]}];

	mexflags = [mexflags {['-DMSH_DEFAULT_FETCH_DEFAULT="' opts.mshFetchDefault '"']}];

	% R2011b
	if(~verLessThan('matlab', '7.13'))
		mexflags = [mexflags {'-DMSH_AVX_SUPPORT'}];
	end
	
	if(opts.mshUseSSE2)
		mexflags = [mexflags {'-DMSH_USE_SSE2=TRUE'}];
	end
	
	if(opts.mshUseAVX)
		mexflags = [mexflags {'-DMSH_USE_AVX=TRUE'}];
	end
	
	if(opts.mshUseAVX2)
		mexflags = [mexflags {'-DMSH_USE_AVX2=TRUE'}];
	end
	
	varopoptsstr = '0';
	for i = 1:numel(opts.mshVarOpsOptsDefault)
		curropt = opts.mshVarOpsOptsDefault{i};
		if(strcmp(curropt , '-s'))
			varopoptsstr = [varopoptsstr '|MSH_IS_SYNCHRONOUS'];
		elseif(strcmp(curropt, '-a'))
			varopoptsstr = [varopoptsstr '&~MSH_IS_SYNCHRONOUS'];
		elseif(strcmp(curropt, '-t'))
			varopoptsstr = [varopoptsstr '|MSH_USE_ATOMIC_OPTS'];
		elseif(strcmp(curropt, '-n'))
			varopoptsstr = [varopoptsstr '&~MSH_USE_ATOMIC_OPTS'];
		else
			error('Invalid value for compilation parameter mshVarOpsOptsDefault');
		end
	end
	
	mexflags = [mexflags {['-DMSH_DEFAULT_VAROP_OPTS_DEFAULT=' varopoptsstr]}];

	fprintf('-Compiling matshare...')
	% mexflags = [mexflags {'COMPFLAGS="$COMPFLAGS /O2"'}];
	% mexflags = [mexflags {'CFLAGS="$CFLAGS -Wall -Werror -Wno-unused-function"'}];
	mex(mexflags{:} , sources{:})
	fprintf(' successful.\n')
	if(opts.mshDebugMode)
		fprintf('-Compiled for DEBUGGING.\n');
	else
		fprintf('-Compiled for RELEASE.\n');
	end
	fprintf('%s\n', ['-The function is located in ' output_path '.']);
end