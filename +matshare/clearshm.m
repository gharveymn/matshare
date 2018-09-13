function clearshm(varargin)
%% MATSHARE.CLEARSHM  Clear variables from shared memory.
%    MATSHARE.CLEARSHM removes all variables from shared memory.
%
%    MATSHARE.CLEARSHM(MO1,MO2,...) removes the variables associated to the
%    specified matshare objects from shared memory.
%
%    MATSHARE.CLEARSHM(VARNAME) removes all variables with the given 
%    variable name from shared memory.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.
	
	if(nargin == 0)
		matshare_(6);
	else
		for i = 1:nargin
			if(ischar(varargin{i}))
				matshare_(6, varargin{i});
			else
				varargin{i}.clearshm;
			end
		end
	end
end
