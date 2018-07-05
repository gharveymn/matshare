function x = testentry(varargin)
	if(nargout == 0)
		x = {1};
	else
		x = num2cell(1:nargout);
	end
	% varargout = nullfcn(varargin);
end

