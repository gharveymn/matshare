function config(varargin)
%% MSHCONFIG  Set or print the current configuration for matshare.
%    MSHCONFIG prints out the current configuration for matshare.
%
%    MSHCONFIG(PAR_1,VAL_1,PAR_2,VAL_2,...,PAR_N,VAL_N) sets configuration
%    parameters PAR_1,PAR_2,...,PAR_N to values VAL_1, VAL_2,..., VAL_N.
%
%    Note: Parameter names are case-independent. Values must all be 
%          strings.
%
%    Available configuration parameters:
%
%        ['ThreadSafety', 'ts'] -- Control whether matshare will use locks 
%                                  when dealing with shared memory.
%            Values: ['on','true','enable'],['off','false','disable']
%            Default: 'on'
%            Notes: I do not recommend turning this off since concurrent 
%                   usage can easily crash your session. Furthermore, 
%                   matshare is designed to be highly concurrent, so the 
%                   effect of locks is negligible in most cases.
%
%        ['MaxVariables','mv'] -- Set the maximum number of variables 
%                                 in shared memory. 
%            Values: An unsigned integer less than 2^32.
%            Default: '512'
%            Notes: It is worth noting that on Linux one may hit the limit
%                   on open file descriptors, and if this occurs while 
%                   using a parpool subprocess, there is no way of relaying 
%                   this to the main session since MATLAB uses files to 
%                   communicate. Thus, make sure this has a value lower 
%                   than the open fd limit.
%
%        ['MaxSize','ms'] -- Set the maximum total size of shared memory.
%            Values: An unsigned integer less than the maximum size 
%                             on your system.
%            Default: '0xFFFFFFFFFFFFF' on 64-bit, '0xFFFFFFFF' on 32-bit
%      
%        ['GarbageCollection','gc'] -- Control whether to have matshare 
%                                      automatically garbage collect unused
%                                      variables in shared memory.
%            Values: ['on','true','enable'],['off','false','disable']
%            Default: 'on'
%            Notes: If you don't want to disable garbage collection for all 
%                   variables, but still want a variable to persist you can 
%                   use `mshpersistshare` which will disable garbage 
%                   collection while sharing the variable.
%      
%        ['Security','sc'] -- Set the security of shared memory segments.
%            Values: An octal security value
%            Default: '0600'
%            Notes: Only available for Linux.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.
	
    matshare_(3, varargin);
    
end