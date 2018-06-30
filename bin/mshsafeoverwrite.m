function destination = mshsafeoverwrite(destination, input)
%% MSHSAFEOVERWRITE  Safely overwrite the contents of a variable in-place.
%    MSHSAFEOVERWRITE(DEST, INPUT) recursively overwrites the contents of 
%    DEST with the contents of INPUT. The two input variables must have 
%    exactly the same size.
%
%    Note: This function is thread-safe.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.
	
	matshare_(9, destination, input);
end

