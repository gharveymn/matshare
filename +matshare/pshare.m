function varargout = pshare(varargin)
%% MATSHARE.PSHARE  Place variables into shared memory with persistence.
%    [S1,S2,...] = MATSHARE.PSHARE(V1,V2,...) copies the argument variables
%    to shared memory and returns matshare objects containing the shared
%    versions. These variables will not be removed by garbage collection 
%    and must be removed via MATSHARE.CLEAR.


%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.
	
	if(nargout == 0)
		% easiest way to pass ans
		matshare_(12, varargin);
		varargout{1} = createobject(ans);
	else
		[shared_raw{1:nargout}] = matshare_(12, varargin);
		varargout = num2cell(createobject(shared_raw, nargout));
	end
	
end

