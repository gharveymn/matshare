function msh_init(varargin)
	if(nargin > 0 && strcmp('unsafe',varargin{1}))
		% initialize as unsafe
		matshare_(uint8(0), 1);
	else
		matshare_(uint8(0));
	end
end

