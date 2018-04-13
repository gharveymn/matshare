function varargout = mshfetch
	% get callback
	switch(nargout)
		case 0
			matshare_(uint8(1));
		case 1
			varargout{1} = matshare_(uint8(1));
		case 2
			[varargout{1},varargout{2}] = matshare_(uint8(1));
		case 3
			[varargout{1},varargout{2},varargout{3}] = matshare_(uint8(1));
		otherwise
			ME = MException('mshshare:tooManyOutputsError','Too many outputs');
			throw(ME);
	end
end

