function [latest, newvars, allvars] = mshfetch
%% MSHFETCH  Fetch variables from shared memory.
%    LATEST = MSHFETCH returns the most recently shared variable.
%
%    [~,NEWVARS] = MSHFETCH returns shared variables not currently 
%    being used by this process.
%
%    [~,~,ALLVARS] = MSHFETCH returns all variables which are currently in 
%    shared memory.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.

	% ordered by predicted usage amounts to slightly improve performance
	if(nargout <= 1)
		matshare_(1);
	elseif(nargout == 3)
		[newvars, allvars] = matshare_(1);
	else
		newvars = matshare_(1);
	end
end

