function f = fetch(varargin)
%% MATSHARE.FETCH  Fetch variables from shared memory.
%    F = MATSHARE.FETCH fetches all variables from shared memory and stores
%    them in a struct.
%    
%    F = MATSHARE.FETCH(OP_1,OP_2,...) fetches variables from shared
%    memory with the given options.
%
%    The returned struct has fields RECENT which contains the most recently
%    shared variable; NEW which is a matshare object array containing 
%    variables not currently tracked by this process; ALL which is a 
%    matshare object array containing all currently shared variables.
%
%    You can specify the following options as character vectors:
%        
%        -r[ecent] -- return the most recent shared variable.
%
%        -n[ew]    -- return variables not tracked by this process
%
%        -a[ll]    -- return all currently shared variables.
%
%    If no options are specified then MATSHARE.FETCH will return all three.
%
%    Future: Fetching via variable identifiers, process ids, etc.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.

	f = matshare_(1, varargin);
	if(iscell(f))
		f = matshare.object(f, numel(f));
	else
		fnames = fieldnames(f);
		for i = 1:numel(fnames)
			f.(fnames{i}) = matshare.object(f.(fnames{i}), numel(f.(fnames{i})));
		end
	end
	
end

