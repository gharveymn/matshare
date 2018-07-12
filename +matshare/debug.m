function debug
%% MATSHARE.DEBUG  Print out debug information.
%    MATSHARE.DEBUG prints out information about the local state of 
%    matshare. If the process is attached to shared memory, then it will 
%    also print out information on the shared state. MATSHARE.DEBUG does 
%    not modify the local state while doing this.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.
	
	matshare_(5);
	
end