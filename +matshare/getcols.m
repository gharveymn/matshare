function out = getcols
%% MATSHARE.UNLOCK  Release the matshare interprocess lock.
%    MATSHARE.UNLOCK releases the matshare interprocess lock previously 
%    acquired by MATSHARE.LOCK.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.	
	
	out = matshare_(14);
	
end