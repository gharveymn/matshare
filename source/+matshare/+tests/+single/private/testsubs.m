function tv = testsubs(tv, f, subs, fcn)
	
	if(nargin < 4)
		fcn = @rand;
	end
	
	tv = subsasgn(tv, subs, fcn(size(subsref(tv, subs))));
	f = subsasgn(f, [substruct('.','data'), subs], subsref(tv, subs));

	if(~matshare.scripts.compstruct(tv, f.data))
		error('Subscripted assignment failed because results were not equal.');
	end
end

