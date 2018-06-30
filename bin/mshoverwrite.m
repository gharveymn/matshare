function dest = mshoverwrite(dest, input)
%% MSHOVERWRITE  Overwrite the contents of a variable in-place.
%    MSHOVERWRITE(DEST, INPUT) recursively overwrites the contents of DEST 
%    with the contents of INPUT. The two input variables must have exactly 
%    the same size.
%
%    Note: This function is not thread-safe. If you want to prevent
%    data races use mshsafeoverwrite or surround your call with 
%    calls to mshlock and mshunlock.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.	
	
	matshare_(8, dest, input);
end

