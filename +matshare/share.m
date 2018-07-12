function varargout = share(varargin)
%% MATSHARE.SHARE  Place variables into shared memory.
%    [S1,S2,...] = MATSHARE.SHARE(V1,V2,...) copies the argument variables
%    to shared memory and returns matshare objects containing the shared
%    versions.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.

	if(nargout == 0)
		% easiest way to pass ans
		matshare_(0, varargin);
		varargout{1} = matshare.object(ans);
	else
		[shared_raw{1:nargout}] = matshare_(0, varargin);
		varargout = num2cell(matshare.object(shared_raw, nargout));
	end
	
end
