function [data, newvars, allvars] = mshfetch
	% ordered by predicted usage amounts to slightly improve performance
	if(nargout <= 1)
		data = matshare_(1);
	elseif(nargout == 3)
		[data, newvars, allvars] = matshare_(1);
	else
		[data, newvars] = matshare_(1);
	end
	matshare_(12);
end

