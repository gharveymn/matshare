function mshclear(varargin)
%% MSHCLEAR  Clear variables from shared memory.
%    MSHCLEAR removes all variables from shared memory.
%
%    MSHCLEAR(VAR_1,VAR_2,...,VAR_N) removes the specified variables
%    from shared memory.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.
	
	matshare_(6, varargin{:});
	
end
