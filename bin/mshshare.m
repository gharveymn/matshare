function [data, newvars, allvars] = mshshare(in)
	% ordered by predicted usage amounts to slightly improve performance
	switch(nargout)
		case 0
			matshare_(uint8(0), in);
		case 1
			data = matshare_(uint8(0), in);
		case 3
			[data, newvars, allvars] = matshare_(uint8(0), in);
		case 2
			[data, newvars] = matshare_(uint8(0), in);
	end
end

