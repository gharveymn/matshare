function varargout = fetch(varargin)
%% MATSHARE.FETCH  Fetch variables from shared memory.
%    F = MATSHARE.FETCH fetches all variables from shared memory with the 
%    default option.
%    
%    [F1,F2,...] = MATSHARE.FETCH(OP_1,OP_2,...) fetches variables from 
%    shared memory with the given options.
%
%    You can specify the following options as character vectors:
%        
%        <strong>-s</strong>[truct] -- return the fetched variables in a struct.
%
%        <strong>-r</strong>[ecent] -- return the most recent shared variable.
%
%        <strong>-w</strong>        -- return variables not tracked by this process
%
%        <strong>-a</strong>[ll]    -- return all currently shared variables.
%
%        <strong>-n</strong>[amed]  -- return all named variables
%
%    X = MATSHARE.FETCH(VARNAME) returns a shared variable that you 
%    previously named. Example:
%        >> matshare.share('-n', 'myvarname', rand(5));
%        >> x = matshare.fetch('myvarname');
%
%    The returns from options are in the order that the arguments were
%    entered and can be mixed with variable names.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.

	if(nargout == 0)
		[varargout{1}] = matshare_(1, varargin);
	else
		[varargout{1:nargout}] = matshare_(1, varargin);
	end
	
	for i = numel(varargout):-1:1
		varargout{i} = unwrap(varargout{i});
	end	
end

function f = unwrap(f)
	if(isstruct(f))
		fnames = fieldnames(f);
		for i = 1:numel(fnames)
			f.(fnames{i}) = unwrap(f.(fnames{i}));
		end
	else
		f = matshare.object(f, numel(f));
	end
end

