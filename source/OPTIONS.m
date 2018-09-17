function opts = OPTIONS
	%% Compilation Options
	%	Set these to true or false according to your preferences.

	%% Default preferences

	% Set the default maximum number of segments
	opts.mshMaxVariables = '512';

	% Set the maximum size of total shared memory (on 64-bit)
	opts.mshMaxSize64 = '0xFFFFFFFFFFFFF'; % (1 << 52)-1

	% Set the maximum size of total shared memory (on 32-bit)
	opts.mshMaxSize32 = '0xFFFFFFFF'; % unsigned 32 bit max

	% Control whether to garbage collect unused variables ('on' or 'off')
	opts.mshGarbageCollection = 'on';

	% Set the default security on shared variables and meta information (Linux only)
	opts.mshSecurity = '0600';

	opts.mshFetchDefault = '-r';

	opts.mshVarOpsOptsDefault = {};

	% must have at least SSE2, AVX/2 is optional
	opts.mshUseSSE2=false;

	opts.mshUseAVX=false;

	opts.mshUseAVX2=false;

	%% Enable or disable debug mode
	opts.mshDebugMode = true;
end