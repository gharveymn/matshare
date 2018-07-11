function [obj] = createobject(shared_vars, num_objs)
	%CREATEOBJECT An entry function for creating matshare objects
	%   OBJ = CREATEOBJECT(shared_vars, num_objs) passes args to
	%   matshare.object or matshare.compatobject depending on 
	%   your version.
	%
	%   Necessary because of dependencies which are not
	%   backwards compatible.
	
	v = version;
	
	% fast verLessThan('matlab', '8.2'), fine because there are less than
	% ten 8.x versions
	if(v(1) < '8' || (v(1) == '8' && v(3) < '2'))
		if(nargin == 0)
			obj = matshare.compatobject;
		elseif(nargin == 1)
			obj = matshare.compatobject(shared_vars);
		else
			obj = matshare.compatobject(shared_vars, num_objs);
		end
	else
		if(nargin == 0)
			obj = matshare.object;
		elseif(nargin == 1)
			obj = matshare.object(shared_vars);
		else
			obj = matshare.object(shared_vars, num_objs);
		end
	end
end

