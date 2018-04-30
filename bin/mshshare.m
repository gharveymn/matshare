function varargout = mshshare(varargin)
	if(nargout > 1)
		ME = MException('mshshare:tooManyOutputsError','Too many outputs');
		throw(ME);
	elseif(nargout == 1)
		varargout{1} = matshare_(uint8(0), varargin{:});
	else
		matshare_(uint8(0), varargin{:});
	end
end

