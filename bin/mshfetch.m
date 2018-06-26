function [latest, newvars, allvars] = mshfetch
	% ordered by predicted usage amounts to slightly improve performance
	latest = [];
	if(nargout <= 1)
		matshare_(1);
	elseif(nargout == 3)
		[newvars, allvars] = matshare_(1);
	else
		newvars = matshare_(1);
	end
end

