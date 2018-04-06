function varargout = mshshare(in)
	if(nargout == 1)
		varargout{1} = matshare_(uint8(0), in);
	else
		matshare_(uint8(0), in);
	end
end

