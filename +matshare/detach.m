function detach
%% MATSHARE.DETACH  Detach the process from shared memory.
%    MATSHARE.DETACH will reset any pointers to shared memory and 
%    deinitialize matshare for this process.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.

	matshare_(2);
	clear matshare_
	
end

