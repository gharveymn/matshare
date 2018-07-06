function varargout = func(varargin)
	%FUNC Summary of this function goes here
	%   Detailed explanation goes here
	varargout{1} = varargin{1}.Property1;
	
	if(nargin > 1)
		varargout{2} = varargin{2}.Property1;
	end
end

