function f = fetch(varargin)
%% MATSHARE.FETCH  Fetch variables from shared memory.
%    F = MATSHARE.FETCH fetches all variables from shared memory and stores
%    them in a struct.
%
%    The returned struct has fields <strong>recent</strong> which contains the most recently
%    shared variable; <strong>new</strong> which is a matshare object array containing 
%    variables not currently tracked by this process; <strong>all</strong> which is a 
%    matshare object array containing all currently shared variables.
%    
%    F = MATSHARE.FETCH(OP_1,OP_2,...) fetches variables from shared
%    memory with the given options.
%
%    You can specify the following options as character vectors:
%        
%        <strong>-r</strong>[ecent] -- return the most recent shared variable.
%
%        <strong>-n</strong>[ew]    -- return variables not tracked by this process
%
%        <strong>-a</strong>[ll]    -- return all currently shared variables.
%
%    If exactly one of the options is provided then a <a href="matlab:help matshare.object">matshare object</a>
%    array will be returned directly, otherwise it will return as a struct.
%
%    Future: Fetching via variable identifiers, process ids, etc.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.

	f = matshare_(1, varargin);
	if(iscell(f))
		f = createobject(f, numel(f));
	else
		fnames = fieldnames(f);
		for i = 1:numel(fnames)
			f.(fnames{i}) = createobject(f.(fnames{i}), numel(f.(fnames{i})));
		end
	end
	
end

