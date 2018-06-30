function shared = mshpersistshare(in)
%% MSHPERSISTSHARE  Place a variable into shared memory with persistence.
%    MSHSHARE(VAR) copies VAR into shared memory and disables automatic
%    garbage collection on the variable.
%
%    SHARED = MSHSHARE(VAR) copies VAR into shared memory, disables automatic
%    garbage collection on the variable, and returns a shared copy.
%
%    Note: Unlike mshshare, this does return ans.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.
	
	matshare_(13, 1, in);
	
end

