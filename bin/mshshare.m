function shared = mshshare(in)
%% MSHSHARE  Place a variable into shared memory.
%    MSHSHARE(VAR) copies VAR into shared memory.
%
%    SHARED = MSHSHARE(VAR) returns a shared copy of VAR.
%
%    Note: This does not return ans because it is transient
%    and by default matshare will garbage collect unused 
%    shared variables.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.

	matshare_(0, nargout, in);
	
end