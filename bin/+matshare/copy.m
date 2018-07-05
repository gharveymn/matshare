function varargout = copy(varargin)
	if(nargout == 0)
		varargout{1} = matshare_(4, varargin);
	else
		[varargout{1:nargout}] = matshare_(4, varargin);
	end
end

