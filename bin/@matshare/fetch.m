function fetched = fetch(varargin)
%% MSHFETCH  Fetch variables from shared memory.
%    FETCHED = MSHFETCH fetches all variables from shared memory and stores
%    them in a struct.
%    
%    FETCHED = MSHFETCH(OP_1,OP_2,...,OP_N) fetches variables from shared
%    memory with the given options.
%
%    The returned struct has fields RECENT which contains the most recently
%    shared variable; NEW which is a cell array containing variables not 
%    currently tracked by this process; ALL which is a cell array 
%    containing all currently shared variables.
%
%    You can specify the following options as character vectors:
%        
%        -r[ecent] -- return the most recent shared variable.
%
%        -n[ew]    -- return variables not tracked by this process
%
%        -a[ll]    -- return all currently shared variables.
%
%    If no options are specified then MSHFETCH will return all three.
%
%    Future: Fetching via variable identifiers, process ids, etc.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.

	fetched = matshare_(1, varargin);
	fnames = fieldnames(fetched);
	for i = 1:numel(fnames)
		cached = fetched.(fnames{i});
		if(~isempty(cached))
			fetched.(fnames{i}) = matshare(cached, numel(cached));
		end
	end
	
end

