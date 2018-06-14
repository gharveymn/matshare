function [shared_variable] = mshshare(in)
	% ordered by predicted usage amounts to slightly improve performance
	switch(nargout)
		case 0
			matshare_(0, in);
		case 1
			shared_variable = matshare_(0, in);
	end
end

