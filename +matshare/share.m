function varargout = share(varargin)
%% MATSHARE.SHARE  Place variables into shared memory.
%    [S1,S2,...] = MATSHARE.SHARE(V1,V2,...) copies the argument variables
%    to shared memory and returns matshare objects containing the shared
%    versions.
%
%    Specify options for MATSHARE.SHARE with character vectors beginning
%    with '-':
%        
%        <strong>-p</strong>[ersist] -- do not subject these variables to garbage 
%                      collection.
%        <strong>-n</strong>[amed]   -- supply names to these variables. In this case the 
%                      syntax is then MATSHARE.SHARE('-n',N1,V1,...)
%                      where N1 is a name specified by a character vector.
%
%    Example using names:
%        >> matshare.share('-n', 'myvarname', rand(5));
%        >> x = matshare.fetch('myvarname');

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
