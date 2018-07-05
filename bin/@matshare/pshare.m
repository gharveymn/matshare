function varargout = pshare(varargin)
%% MSHPERSISTSHARE  Place variables into shared memory with persistence.
%    SHARED = MSHPERSISTSHARE(VAR_1,VAR_2,...,VAR_N) copies 
%    VAR_1,VAR_2,...,VAR_N to shared memory and returns a cell array 
%    containing shared copies. These variables will not be removed by 
%    garbage collection.


%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.
	
	if(nargout == 0)
		varargout{1} = matshare(matshare_(12,varargin));
	else
		[shared_raw{1:nargout}] = matshare_(12, varargin);
		varargout = num2cell(matshare(shared_raw, nargout));
	end
	
end

