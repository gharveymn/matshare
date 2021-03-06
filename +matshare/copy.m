function varargout = copy(varargin)
%% MATSHARE.COPY  Copy variables from shared memory.
%    MATSHARE.COPY copies all variables from shared memory.
%
%    MATSHARE.COPY(MO1,MO2,...) copies the variables associated to the
%    specified matshare objects from shared memory.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.	
	
	if(nargin > 0)
		if(nargout == 0)
			varargout{1} = varargin{1}.copy;
		else
			for i = nargout:-1:1
				varargout{i} = varargin{i}.copy;
			end
		end
	else
		error('matshare:copy:InputError', 'At least one argument needed for matshare.copy.');
	end
end

