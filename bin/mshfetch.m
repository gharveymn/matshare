function [data, newvars, allvars] = mshfetch
	% ordered by predicted usage amounts to slightly improve performance
	switch(nargout)
		case 1
			data = matshare_(1);
		case 3
			[data, newvars, allvars] = matshare_(1);
		case 2
			[data, newvars] = matshare_(1);
		case 0
			matshare_(1);
	end
end

