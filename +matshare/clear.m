function clear(varargin)
%% MATSHARE.CLEAR  Clear variables from shared memory.
%    MATSHARE.CLEAR removes all variables from shared memory.
%
%    MATSHARE.CLEAR(MO1,MO2,...) removes the variables associated to the
%    specified matshare objects from shared memory.
%
%    MATSHARE.CLEAR(VARNAME) removes all variables with the given variable
%    name from shared memory.

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
				varargin{i}.clear;
			end
		end
	end
end
