function [data, new, all] = mshfetch
	% ordered by predicted usage amounts to slightly improve performance
	switch(nargout)
		case 1
			data = matshare_(uint8(1));
		case 3
			[data, new, all] = matshare_(uint8(1));
		case 2
			[data, new] = matshare_(uint8(1));
		case 0
			matshare_(uint8(1));
	end
end

