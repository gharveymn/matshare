%% Compilation Options
%	Set these to true or false according to your preferences.

%% Default preferences

% Control whether or not to use locks when sharing memory ('on' or 'off')
mshThreadSafety = 'on';

% Set the default maximum number of segments
mshMaxVariables = '512';

% Set the maximum size of total shared memory (on 64-bit)
mshMaxSize64 = '0xFFFFFFFFFFFFF'; % (1 << 52)-1

% Set the maximum size of total shared memory (on 32-bit)
mshMaxSize32 = '0xFFFFFFFF'; % unsigned 32 bit max

% Control whether to garbage collect unused variables ('on' or 'off')
mshGarbageCollection = 'on';

% Set the default security on shared variables and meta information (Linux only)
mshSecurity = '0600';

%% Enable or disable debug mode
mshDebugMode = true;