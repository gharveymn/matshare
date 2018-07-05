function [varargout] = share(varargin)
%% MSHSHARE  Place variables into shared memory.
%    OUT = MSHSHARE(VAR_1,VAR_2,...,VAR_N) copies VAR_1,VAR_2,...,VAR_N
%    to shared memory and returns an array containing matshare objects.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.

	if(nargout == 0)
		varargout{1} = matshare(matshare_(0,varargin));
	else
		[shared_raw{1:nargout}] = matshare_(0, varargin);
		varargout = num2cell(matshare(shared_raw, nargout));
	end
	
end
