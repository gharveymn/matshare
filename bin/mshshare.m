function [data, new, all] = mshshare(in)
	% ordered by predicted usage amounts to slightly improve performance
	switch(nargout)
		case 0
			matshare_(uint8(0), in);
		case 1
			data = matshare_(uint8(0), in);
		case 3
			[data, new, all] = matshare_(uint8(0), in);
		case 2
			[data, new] = matshare_(uint8(0), in);
	end
end

