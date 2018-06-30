function [latest, newvars, allvars] = mshlocalcopy(in)
%% MSHLOCALCOPY  Create local deep copies of shared variables.%
%    LATEST = MSHLOCALCOPY returns the most recently shared variable.
%
%    [~,NEWVARS] = MSHLOCALCOPY returns shared variables not currently being 
%    used by this process.
%
%    [~,~,ALLVARS] = MSHLOCALCOPY returns all variables which are currently in 
%    shared memory.
%
%    COPY = MSHLOCALCOPY(VAR) returns a local deep copy of VAR.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.
	
	% make a local copy of the variable
	if(nargin == 1)
		if(nargout > 1)
			error('matshare:TooManyOutputsError', 'Too many outputs.');
		end
		latest = matshare_(4, in);
		return;
	end
	
	% ordered by predicted usage amounts to slightly improve performance
	switch(nargout)
		case 1
			latest = matshare_(4);
		case 3
			[latest, newvars, allvars] = matshare_(4);
		case 2
			[latest, newvars] = matshare_(4);
		case 0
			matshare_(4);
	end
end

