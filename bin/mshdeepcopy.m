function [data, newvars, allvars] = mshdeepcopy(in)
	
	% make a local copy of the variable
	if(nargin == 1)
		data = matshare_(4, in);
	end
	
	% ordered by predicted usage amounts to slightly improve performance
	switch(nargout)
		case 1
			data = matshare_(4);
		case 3
			[data, newvars, allvars] = matshare_(4);
		case 2
			[data, newvars] = matshare_(4);
		case 0
			matshare_(4);
	end
end

